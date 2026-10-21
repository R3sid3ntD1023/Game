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

		m_awakeSenses = m_perception.config;
		m_awakeTimer = static_cast<float>(std::rand() % 4000) / 100.0f;
		m_awakeLimit = 50.0f + static_cast<float>(std::rand() % 6000) / 100.0f;
		if (std::rand() % 3 == 0)
			StartSleep();
	}

	void DinosaurAI::OnUpdate(float dt)
	{
		m_attackTimer = std::max(0.0f, m_attackTimer - dt);

		m_perception.Update(dt, *gameObject, m_target);
		if (gameObject->GetWorld().debugPerception)
			m_perception.DrawDebug(*gameObject, m_target.GetTransform().position);
		UpdateRest(dt);

		UpdateAudio(dt);
		UpdateAnimation(dt);

		m_aiContext.deltaTime = dt;
		m_behaviorTree.Tick(m_aiContext);
	}

	void DinosaurAI::StartSleep()
	{
		m_sleeping = true;
		m_sleepTimer = 25.0f + static_cast<float>(std::rand() % 2000) / 100.0f;
		m_awakeSenses = m_perception.config;
		// Asleep: only things very close or very loud get through.
		m_perception.config.sightRange = 7.0f;
		m_perception.config.hearingSensitivity = 0.35f;
		m_perception.config.sightRate *= 0.5f;
	}

	void DinosaurAI::WakeUp()
	{
		m_sleeping = false;
		m_awakeTimer = 0.0f;
		m_awakeLimit = 50.0f + static_cast<float>(std::rand() % 6000) / 100.0f;
		m_perception.config = m_awakeSenses;
	}

	// Drives the sleep and hunt timers that the behaviour tree conditions read.
	void DinosaurAI::UpdateRest(float dt)
	{
		const bool alerted = m_perception.Alerted();
		if (alerted)
			m_lastAlertPos = m_perception.LastKnownPosition();

		if (m_sleeping)
		{
			m_sleepTimer -= dt;
			if (alerted || m_sleepTimer <= 0.0f)
				WakeUp();
		}
		else if (!alerted && m_huntTimer <= 0.0f && (m_stateEnum == DinosaurState::Wandering || m_stateEnum == DinosaurState::Idle))
		{
			m_awakeTimer += dt;
			if (m_awakeTimer > m_awakeLimit)
				StartSleep();
		}

		// Lost the target: search the surrounding area for a while.
		if (m_wasAlerted && !alerted && !m_sleeping)
		{
			m_huntTimer = 25.0f;
			m_huntCentre = m_lastAlertPos;
			m_huntRepick = 0.0f;
		}
		m_wasAlerted = alerted;
		if (alerted)
			m_huntTimer = 0.0f;
		else
			m_huntTimer = std::max(0.0f, m_huntTimer - dt);
	}

	glm::vec3 DinosaurAI::PickHuntPoint() const
	{
		const float angle = static_cast<float>(std::rand() % 628) / 100.0f;
		const float radius = 6.0f + static_cast<float>(std::rand() % 1900) / 100.0f;
		glm::vec3 p = m_huntCentre + glm::vec3(std::cos(angle), 0.0f, std::sin(angle)) * radius;
		p.y = gameObject->GetTransform().position.y;
		return p;
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
		m_aiContext.blackboard.Add<glm::vec3>("huntTarget");

		auto chaseTo = m_behaviorTree.AddNode<ai::MoveTo>();
		auto wanderTo = m_behaviorTree.AddNode<ai::MoveTo>();
		auto huntTo = m_behaviorTree.AddNode<ai::MoveTo>();

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

		// sleep: lie still until the sleep timer ends or something wakes it
		auto sleepCondition = m_behaviorTree.AddNode<ai::Conditional>();
		sleepCondition->SetName("IsSleeping");
		sleepCondition->SetCondition([this](ai::AIContext &) { return m_sleeping; });

		auto sleepAction = m_behaviorTree.AddNode<ai::Action>();
		sleepAction->SetName("Sleep");
		sleepAction->SetAction(
			[this](ai::AIContext &)
			{
				if (m_branch != Branch::Sleep)
					m_branch = Branch::Sleep;
				m_body->velocity.x = m_body->velocity.z = 0.0f;
				SetState(DinosaurState::Sleeping);
				return ai::Node::State::Running;
			}
		);

		auto sleepSequence = m_behaviorTree.AddNode<ai::Sequence>();
		sleepSequence->SetName("SleepSequence");
		sleepSequence->AddChild(sleepCondition);
		sleepSequence->AddChild(sleepAction);

		// hunt: walk between random points around where the target was last known
		auto huntCondition = m_behaviorTree.AddNode<ai::Conditional>();
		huntCondition->SetName("IsHunting");
		huntCondition->SetCondition([this](ai::AIContext &) { return m_huntTimer > 0.0f; });

		huntTo->SetName("HuntTo");
		huntTo->m_target = "huntTarget";
		huntTo->m_speed = m_speed * 0.55f;

		auto huntAction = m_behaviorTree.AddNode<ai::Action>();
		huntAction->SetName("Hunt");
		huntAction->SetAction(
			[this, huntTo](ai::AIContext &ctx)
			{
				m_huntRepick -= ctx.deltaTime;
				glm::vec3 toGoal = ctx.blackboard.IsSet("huntTarget") ? ctx.blackboard.Get<glm::vec3>("huntTarget") - ctx.self->GetTransform().position : glm::vec3(0.0f);
				toGoal.y = 0.0f;
				const bool needPoint = m_branch != Branch::Hunt || !ctx.blackboard.IsSet("huntTarget") || m_huntRepick <= 0.0f || glm::length(toGoal) < 2.0f;
				if (needPoint)
				{
					m_branch = Branch::Hunt;
					m_huntRepick = 7.0f;
					ctx.blackboard.Set<glm::vec3>("huntTarget", PickHuntPoint());
					huntTo->Restart();
				}
				SetState(DinosaurState::Hunting);
				return ai::Node::State::Success;
			}
		);

		auto huntSequence = m_behaviorTree.AddNode<ai::Sequence>();
		huntSequence->SetName("HuntSequence");
		huntSequence->AddChild(huntCondition);
		huntSequence->AddChild(huntAction);
		huntSequence->AddChild(huntTo);

		prioritySelector->AddChild(attackSequence);
		prioritySelector->AddChild(chaseSequence);
		prioritySelector->AddChild(sleepSequence);
		prioritySelector->AddChild(huntSequence);
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

		if (m_stateEnum != DinosaurState::Wandering && m_stateEnum != DinosaurState::Chasing && m_stateEnum != DinosaurState::Hunting)
			return;

		const float speed = m_stateEnum == DinosaurState::Chasing ? m_speed : m_stateEnum == DinosaurState::Hunting ? m_speed * 0.55f : m_speed * 0.3f;

		if (!m_body->grounded)
			return;

		m_stepDistance += speed * dt;

		float distancePerStep = m_stateEnum == DinosaurState::Chasing ? 1.8f : m_stateEnum == DinosaurState::Hunting ? 2.2f : 2.8f;
		if (m_stepDistance >= distancePerStep)
		{
			m_stepDistance = 0.0f;
			audio->PlayOneShot("footstep_" + std::to_string(std::rand() % 4), gameObject->GetTransform().position, 1.2f, 0.45f);
		}
	}

	void DinosaurAI::UpdateAnimation(float dt)
	{
		if (!m_animator)
			return;

		// The pose hook reads blend as 0 idle .. 1 run, and 0 .. -1 for lying asleep; it eases between them.
		float target = 0.0f, speed = 1.0f;
		switch (m_stateEnum)
		{
		case DinosaurState::Wandering:
			target = 0.3f;
			speed = 0.8f;
			break;
		case DinosaurState::Hunting:
			target = 0.6f;
			speed = 1.2f;
			break;
		case DinosaurState::Sleeping:
			target = -1.0f;
			speed = 0.6f;
			break;
		case DinosaurState::Chasing:
			target = 1.0f;
			speed = 2.0f;
			break;
		case DinosaurState::Attacking:
			speed = 1.5f;
			break;
		case DinosaurState::Dead:
			speed = 0.0f;
			break;
		default:
			break;
		}
		// Lying down is slow; getting up is quick so a woken dinosaur is not a sitting duck.
		const float rate = (target < m_poseBlend && target < 0.0f) ? 1.2f : 4.0f;
		const float step = rate * dt;
		m_poseBlend += glm::clamp(target - m_poseBlend, -step, step);
		m_animator->blend = m_poseBlend;
		m_animator->speed = speed;
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
