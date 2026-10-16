#pragma once
#include "ai/BehaviorTree.h"
#include "ai/Blackboard.h"
#include "eng/Animator.h"
#include "eng/BuiltinComponents.h"
#include "eng/GameObject.h"
#include "eng/RigidBody.h"
#include "eng/Script.h"
#include "game/GameState.h"
#include <glm/glm.hpp>

namespace game
{
	enum class DinosaurState
	{
		Idle,
		Wandering,
		Chasing,
		Attacking,
		Dead
	};

	// Placeholder dinosaur brain: wander, chase the player when close, bite when in range.
	class DinosaurAI : public eng::Script
	{
	public:
		DinosaurAI(eng::GameObject &target, GameState &state, float speed, float damage)
			: m_target(target),
			  m_state(state),
			  m_speed(speed),
			  m_damage(damage)
		{
		}

		void OnStart() override;
		void OnUpdate(float dt) override;
		void OnFixedUpdate(float dt) override;

	private:
		void InitializeBehaviorTree();
		void UpdateAudio(float dt);
		void UpdateAnimation();

		void SetState(DinosaurState state);
		void OnStateChanged(DinosaurState state);

	private:
		eng::GameObject &m_target;
		GameState &m_state;
		eng::RigidBody *m_body = nullptr;
		eng::Health *m_health = nullptr;
		eng::Health *m_targetHealth = nullptr;
		eng::Animator *m_animator = nullptr;
		float m_speed;
		float m_damage;
		float m_attackTimer = 0.0f;
		float m_stepDistance = 0.0f;
		float m_wanderTimer = 0.0f;
		glm::vec3 m_wanderDir{0.0f};
		float m_aggroRange = 35.0f;
		float m_attackRange = 3.2f;

		ai::BehaviorTree m_behaviorTree;

		DinosaurState m_stateEnum = DinosaurState::Idle;
		DinosaurState m_prevStateEnum = DinosaurState::Idle;
	};

} // namespace game
