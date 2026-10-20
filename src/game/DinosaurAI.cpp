#include "ai/Nodes.h"
#include "eng/World.h"
#include "game/DinosaurAI.h"
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
			[this](float /*amount*/)
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
					SetState(DinosaurState::Chasing);
				}
			}
		);

		InitializeBehaviorTree();
	}

	void DinosaurAI::OnUpdate(float dt)
	{
		m_attackTimer = std::max(0.0f, m_attackTimer - dt);

		UpdateAudio(dt);
		UpdateAnimation();

		m_behaviorTree.Tick(m_aiContext);
	}

	void DinosaurAI::OnFixedUpdate(float /*dt*/)
	{
		if (!m_body || (m_health && m_health->Dead()))
			return;
<<<<<<< HEAD
=======

		m_behaviorTree.Tick(m_aiContext);
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
	}

	void DinosaurAI::InitializeBehaviorTree()
	{
		m_aiContext.self = gameObject;
		m_aiContext.blackboard.Add<glm::vec3>("target");

		auto attackCondition = [this](ai::AIContext &ctx)
		{
			auto &self = ctx.self->GetTransform();
			auto &target = m_target.GetTransform();
			glm::vec3 dist = target.position - self.position;
			dist.y = 0.0f;
			return glm::length(dist) < m_attackRange;
		};

		auto attackAction = [this](ai::AIContext &ctx)
		{
			SetState(DinosaurState::Attacking);

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

		auto chaseCondition = [this](ai::AIContext &ctx)
		{
			auto &self = ctx.self->GetTransform();
			auto &target = m_target.GetTransform();
			float dist = glm::length(target.position - self.position);
			return (dist >= m_attackRange && dist < m_aggroRange) || m_stateEnum == DinosaurState::Chasing;
		};

		auto chaseAction = [this](ai::AIContext &ctx)
		{
			ctx.blackboard.Set<glm::vec3>("target", m_target.GetTransform().position);
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

		auto chaseTo = m_behaviorTree.AddNode<ai::MoveTo>();
		chaseTo->SetName("ChaseTo");
		chaseTo->m_target = "target";
		chaseTo->m_speed = m_speed;

		auto chaseSequence = m_behaviorTree.AddNode<ai::Sequence>();
		chaseSequence->SetName("ChaseSequence");

		auto notHasTarget = m_behaviorTree.AddNode<ai::Conditional>();
		notHasTarget->SetName("NotHasTarget");
		notHasTarget->SetCondition([this](ai::AIContext &ctx) { return !ctx.blackboard.IsSet("target"); });

		auto wanderSequence = m_behaviorTree.AddNode<ai::Sequence>();
		wanderSequence->SetName("WanderSequence");

		auto wanderTo = m_behaviorTree.AddNode<ai::MoveTo>();
		wanderTo->SetName("WanderTo");
		wanderTo->m_target = "target";
		wanderTo->m_speed = m_speed * 0.3f;

		auto wanderAction = m_behaviorTree.AddNode<ai::Action>();
		wanderAction->SetName("WanderAction");
		wanderAction->SetAction(
			[this](ai::AIContext & /*ctx*/)
			{
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

		prioritySelector->AddChild(findPointSelector);
		// prioritySelector->AddChild(attackSequence);
		// prioritySelector->AddChild(chaseSequence);
		root->SetChild(prioritySelector);

		m_behaviorTree.SetRoot(root);
		m_behaviorTree.Print(root);
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
