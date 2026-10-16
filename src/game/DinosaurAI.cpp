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
	}

	void DinosaurAI::OnFixedUpdate(float dt)
	{
		if (!m_body || (m_health && m_health->Dead()))
			return;

		m_wanderTimer -= dt;

		m_behaviorTree.Tick(m_aiContext);
	}

	void DinosaurAI::InitializeBehaviorTree()
	{
		m_aiContext.self = gameObject;

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

			return ai::NodeStatus::Running;
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
			SetState(DinosaurState::Chasing);

			auto &self = ctx.self->GetTransform();
			auto &target = m_target.GetTransform();
			auto dir = target.position - self.position;
			dir.y = 0.0f;
			auto len = glm::length(dir);

			if (len > 0.0f)
			{
				dir /= len;
				self.rotation = glm::angleAxis(std::atan2(-dir.z, dir.x), glm::vec3(0, 1, 0));
			}

			m_body->velocity.x = dir.x * m_speed;
			m_body->velocity.z = dir.z * m_speed;

			return ai::NodeStatus::Running;
		};

		auto wanderAction = [this](ai::AIContext &ctx)
		{
			SetState(DinosaurState::Wandering);

			auto &self = ctx.self->GetTransform();

			if (m_wanderTimer <= 0.0f)
			{
				const float a = static_cast<float>(std::rand() % 628) / 100.0f;
				m_wanderDir = (std::rand() % 3 == 0) ? glm::vec3(0.0f) : glm::vec3(std::cos(a), 0.0f, std::sin(a));
				m_wanderTimer = 2.0f + static_cast<float>(std::rand() % 300) / 100.0f;
			}

			m_body->velocity.x = m_wanderDir.x * m_speed * 0.3f;
			m_body->velocity.z = m_wanderDir.z * m_speed * 0.3f;

			if (glm::dot(m_wanderDir, m_wanderDir) > 0.0f)
				self.rotation = glm::angleAxis(std::atan2(-m_wanderDir.z, m_wanderDir.x), glm::vec3(0, 1, 0));

			return ai::NodeStatus::Running;
		};

		auto root = m_behaviorTree.AddNode<ai::Selector>();
		// attack sequence
		auto attackRange = m_behaviorTree.AddNode<ai::Conditional>();
		attackRange->SetCondition(attackCondition);

		auto attack = m_behaviorTree.AddNode<ai::Action>();
		attack->SetAction(attackAction);

		auto attackSequence = m_behaviorTree.AddNode<ai::Sequence>();
		attackSequence->AddChild(attackRange);
		attackSequence->AddChild(attack);

		// chase behavior

		auto chaseRange = m_behaviorTree.AddNode<ai::Conditional>();
		chaseRange->SetCondition(chaseCondition);

		auto chase = m_behaviorTree.AddNode<ai::Action>();
		chase->SetAction(chaseAction);

		// chase->SetAction(chaseAction); already set above

		auto chaseSequence = m_behaviorTree.AddNode<ai::Sequence>();
		chaseSequence->AddChild(chaseRange);
		chaseSequence->AddChild(chase);

		// wander behavior
		auto wander = m_behaviorTree.AddNode<ai::Action>();
		wander->SetAction(wanderAction);

		root->AddChild(attackSequence);
		root->AddChild(chaseSequence);
		root->AddChild(wander);
		m_behaviorTree.SetRoot(root);
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
