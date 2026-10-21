#include "game/DinosaurAI.h"
#include "ai/Nodes.h"
#include "eng/World.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <string>

namespace game
{

	void DinosaurAI::OnStart()
	{
		m_body = gameObject->GetComponent<eng::RigidBody>();
		m_health = gameObject->GetComponent<eng::Health>();
		m_targetHealth = m_target.GetComponent<eng::Health>();
		m_animator = gameObject->GetComponent<eng::Animator>();

		m_health->SetOnDamageCallback(
			[this](float amount)
			{
				if (m_health->Dead())
				{
					SetState(DinosaurState::Dead);
					m_body->velocity = glm::vec3(0.0f);
					++m_state.kills;
					gameObject->GetWorld().EmitBlood(gameObject->GetTransform().position, glm::vec3(0.0f, 1.0f, 0.0f), 60);
					gameObject->Destroy();
					spdlog::info("Dinosaur down ({} kills)", m_state.kills);
				}
				else
				{
					m_perception.OnDamaged(amount, m_target.GetTransform().position);
				}
			}
		);

		InitializeBehaviorTree();
	}

	void DinosaurAI::OnUpdate(float dt)
	{
		m_attackTimer = std::max(0.0f, m_attackTimer - dt);

		m_perception.Update(dt, *gameObject, m_target);
		if (gameObject->GetWorld().debugPerception)
			m_perception.DrawDebug(*gameObject, m_target.GetTransform().position);

		UpdateAudio(dt);
		UpdateAnimation();

		m_aiContext.deltaTime = dt;
		m_behaviorTree.Tick(m_aiContext);
	}

	void DinosaurAI::OnFixedUpdate(float dt)
	{
		if (!m_body || (m_health && m_health->Dead()))
			return;
	}

	void DinosaurAI::InitializeBehaviorTree()
	{
		m_aiContext.self = gameObject;
		m_aiContext.blackboard.Add<glm::vec3>("target");
		m_aiContext.blackboard.Add<glm::vec3>("chaseTarget");

		auto chaseTo = m_behaviorTree.AddNode<ai::MoveTo>();
		auto wanderTo = m_behaviorTree.AddNode<ai::MoveTo>();

		auto attackCondition = [this](ai::AIContext &ctx)
		{
			auto &self = ctx.self->GetTransform();
			auto &target = m_target.GetTransform();
			glm::vec3 dist = target.position - self.position;
			dist.y = 0.0f;
			return m_perception.Alerted() && glm::length(dist) < m_attackRange;
		};

		auto attackAction = [this](ai::AIContext &ctx)
		{
			SetState(DinosaurState::Attacking);
			m_branch = Branch::Attack;
			auto &self = ctx.self->GetTransform();
			auto &target = m_target.GetTransform();
			glm::vec3 dir = target.position - self.position;
			dir.y = 0.0f;
			float dist = glm::length(dir);

			if (dist > 1e-3f)
			{
				dir = (target.position - self.position) / dist;
			}

			m_body->velocity.x = 0.0f;
			m_body->velocity.z = 0.0f;

			if (m_attackTimer <= 0.0f && m_targetHealth)
			{
				m_targetHealth->Damage(m_damage);
				gameObject->GetWorld().EmitBlood(target.position + glm::vec3(0.0f, 0.5f, 0.0f), dir, 16);
				m_attackTimer = 1.0f;
			}

			if (glm::dot(dir, dir) > 0.0f)
			{
				self.rotation = glm::angleAxis(std::atan2(-dir.z, dir.x), glm::vec3(0, 1, 0));
			}

			return ai::Node::State::Running;
		};

		auto chaseCondition = [this](ai::AIContext &) { return m_perception.Alerted(); };

		auto chaseAction = [this, chaseTo](ai::AIContext &ctx)
		{
			if (m_branch != Branch::Chase)
			{
				m_branch = Branch::Chase;
				chaseTo->Restart();
			}

			const glm::vec3 goal = m_perception.LastKnownPosition();
			glm::vec3 toGoal = goal - ctx.self->GetTransform().position;
			toGoal.y = 0.0f;
			if (!m_perception.SeesTarget() && glm::length(toGoal) < 2.0f)
			{
				// Reached the last known spot and found nothing: stand and look around until the memory runs out.
				m_body->velocity.x = m_body->velocity.z = 0.0f;
				ctx.blackboard.Clear("chaseTarget");
				m_perception.NotifyArrived();
				SetState(DinosaurState::Idle);
				return ai::Node::State::Running;
			}

			ctx.blackboard.Set<glm::vec3>("chaseTarget", goal);
			SetState(DinosaurState::Chasing);
			return ai::Node::State::Success;
		};

		auto root = m_behaviorTree.AddNode<ai::Root>();
		root->SetName("Root");

		auto prioritySelector = m_behaviorTree.AddNode<ai::Selector>();
		prioritySelector->SetName("PrioritySelector");

		// attack sequence
		auto attackRange = m_behaviorTree.AddNode<ai::Conditional>();
		attackRange->SetName("AttackRange");
		attackRange->SetCondition(attackCondition);

		auto attack = m_behaviorTree.AddNode<ai::Action>();
		attack->SetName("Attack");
		attack->SetAction(attackAction);

		auto attackSequence = m_behaviorTree.AddNode<ai::Sequence>();
		attackSequence->SetName("AttackSequence");

		// chase behavior

		auto chaseRange = m_behaviorTree.AddNode<ai::Conditional>();
		chaseRange->SetName("ChaseRange");
		chaseRange->SetCondition(chaseCondition);

		auto chase = m_behaviorTree.AddNode<ai::Action>();
		chase->SetName("Chase");
		chase->SetAction(chaseAction);

		chaseTo->SetName("ChaseTo");
		chaseTo->m_target = "chaseTarget";
		chaseTo->m_speed = m_speed;

		auto chaseSequence = m_behaviorTree.AddNode<ai::Sequence>();
		chaseSequence->SetName("ChaseSequence");

		auto notHasTarget = m_behaviorTree.AddNode<ai::Conditional>();
		notHasTarget->SetName("NotHasTarget");
		notHasTarget->SetCondition([this](ai::AIContext &ctx) { return !ctx.blackboard.IsSet("target"); });

		auto wanderSequence = m_behaviorTree.AddNode<ai::Sequence>();
		wanderSequence->SetName("WanderSequence");

		wanderTo->SetName("WanderTo");
		wanderTo->m_target = "target";
		wanderTo->m_speed = m_speed * 0.3f;

		auto wanderAction = m_behaviorTree.AddNode<ai::Action>();
		wanderAction->SetName("WanderAction");
		wanderAction->SetAction(
			[this, wanderTo](ai::AIContext & /*ctx*/)
			{
				if (m_branch != Branch::Wander)
				{
					m_branch = Branch::Wander;
					wanderTo->Restart();
				}
				SetState(DinosaurState::Wandering);
				return ai::Node::State::Success;
			}
		);

		auto findPoint = m_behaviorTree.AddNode<ai::FindRandomReachablePoint>();
		findPoint->SetName("FindPoint");
		findPoint->m_target = "target";

		auto findPointSelector = m_behaviorTree.AddNode<ai::Selector>();
		findPointSelector->SetName("FindPointSelector");

		auto findPointSequence = m_behaviorTree.AddNode<ai::Sequence>();
		findPointSequence->SetName("FindPointSequence");

		findPointSequence->AddChild(notHasTarget);
		findPointSequence->AddChild(findPoint);

		wanderSequence->AddChild(wanderAction);
		wanderSequence->AddChild(wanderTo);

		findPointSelector->AddChild(findPointSequence);
		findPointSelector->AddChild(wanderSequence);

		attackSequence->AddChild(attackRange);
		attackSequence->AddChild(attack);

		chaseSequence->AddChild(chaseRange);
		chaseSequence->AddChild(chase);
		chaseSequence->AddChild(chaseTo);

		prioritySelector->AddChild(attackSequence);
		prioritySelector->AddChild(chaseSequence);
		prioritySelector->AddChild(findPointSelector);
		root->SetChild(prioritySelector);

		m_behaviorTree.SetRoot(root);
		// m_behaviorTree.Print(root);
	}

	void DinosaurAI::UpdateAudio(float dt)
	{
		audio::AudioEngine *audio = gameObject->GetWorld().audio;
		if (!audio)
			return;

		if (m_stateEnum != DinosaurState::Wandering && m_stateEnum != DinosaurState::Chasing)
			return;

		float speed = m_stateEnum == DinosaurState::Chasing ? m_speed : m_speed * 0.3f;

		if (!m_body->grounded)
			return;

		m_stepDistance += speed * dt;

		float distancePerStep = m_stateEnum == DinosaurState::Chasing ? 1.8f : 2.8f;
		if (m_stepDistance >= distancePerStep)
		{
			m_stepDistance = 0.0f;
			audio->PlayOneShot("footstep_" + std::to_string(std::rand() % 4), gameObject->GetTransform().position, 1.2f, 0.45f);
		}
	}

	void DinosaurAI::UpdateAnimation()
	{
		if (!m_animator)
			return;

		switch (m_stateEnum)
		{
		case DinosaurState::Idle:
			m_animator->blend = 0.0f;
			m_animator->speed = 1.0f;
			break;
		case DinosaurState::Wandering:
			m_animator->blend = 0.3f;
			m_animator->speed = 0.8f;
			break;
		case DinosaurState::Chasing:
			m_animator->blend = 1.0f;
			m_animator->speed = 2.0f;
			break;
		case DinosaurState::Attacking:
			m_animator->blend = 0.0f;
			m_animator->speed = 1.5f;
			break;
		case DinosaurState::Dead:
			m_animator->blend = 0.0f;
			m_animator->speed = 0.0f;
			break;
		default:
			break;
		}
	}

	void DinosaurAI::SetState(DinosaurState state)
	{
		if (m_stateEnum == state)
			return;

		m_prevStateEnum = m_stateEnum;
		m_stateEnum = state;

		OnStateChanged(state);
	}

	void DinosaurAI::OnStateChanged(DinosaurState state)
	{
		auto audio = gameObject->GetWorld().audio;
		if (!audio)
			return;

		switch (state)
		{
		case DinosaurState::Chasing:
			audio->PlayOneShot("raptor_roar", gameObject->GetTransform().position, 1.0f, 0.9f + 0.2f * static_cast<float>(std::rand() % 100) / 100.0f);
			break;
		case DinosaurState::Attacking:
			audio->PlayOneShot("raptor_attack", gameObject->GetTransform().position);
			break;

		default:
			break;
		}
	}

} // namespace game
