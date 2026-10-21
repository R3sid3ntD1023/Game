#pragma once
#include "ai/BehaviorTree.h"
#include "ai/Blackboard.h"
#include "eng/Animator.h"
#include "eng/BuiltinComponents.h"
#include "eng/GameObject.h"
#include "eng/RigidBody.h"
#include "eng/Script.h"
#include "game/AIPerception.h"
#include "game/Ecology.h"
#include "game/GameState.h"
#include <glm/glm.hpp>
#include <memory>

namespace game
{
	class HerbivoreAI;

	enum class CarnivoreState
	{
		Idle,
		Wandering,
		Stalking, // closing in on prey
		Chasing,  // after the player
		Attacking,
		Sleeping,
		Fleeing,
		Dead
	};

	// Brain of a large solitary predator (the T-Rex). It hunts alone: it chases the player once it notices them
	// and stalks and bites herbivores. When attacked by raptors, it snaps back unless badly hurt or mobbed; calm and
	// wounded, it lies down, and sleeping heals it.
	class CarnivoreAI : public eng::Script
	{
	public:
		CarnivoreAI(eng::GameObject &target, GameState &state, float speed, float damage);
		~CarnivoreAI() override;

		void OnStart() override;
		void OnUpdate(float dt) override;

		bool Dead() const { return m_health && m_health->Dead(); }
		glm::vec3 Position() const { return gameObject->GetTransform().position; }
		// Damage from another animal: no kill credit, and `from` is where the attacker stands.
		void Hurt(float amount, const glm::vec3 &from);

	private:
		void InitializeBehaviorTree();
		void UpdateAudio(float dt);
		void UpdateAnimation(float dt);
		void SetState(CarnivoreState state);
		void OnStateChanged(CarnivoreState state);

		void UpdateRest(float dt);
		void UpdateFlee(float dt);
		void UpdatePrey(float dt);
		void StartSleep();
		void WakeUp();
		float HealthFraction() const { return m_health->max > 0.0f ? m_health->current / m_health->max : 0.0f; }
		HerbivoreAI *CurrentPrey() const;
		float PreyDistance(const HerbivoreAI &prey) const;

	private:
		eng::GameObject &m_target;
		GameState &m_state;
		std::shared_ptr<Ecology> m_ecology;
		eng::RigidBody *m_body = nullptr;
		eng::Health *m_health = nullptr;
		eng::Health *m_targetHealth = nullptr;
		eng::Animator *m_animator = nullptr;
		float m_speed;
		float m_damage;
		float m_attackTimer = 0.0f;
		float m_biteTimer = 0.0f;
		float m_attackRange = 5.0f;
		float m_stepDistance = 0.0f;
		float m_poseBlend = 0.0f;
		bool m_animalDamage = false;
		bool m_registered = false;
		glm::vec3 m_pendingAttacker{0.0f};

		enum class Branch { None, Flee, Attack, Chase, Sleep, Prey, Wander };
		Branch m_branch = Branch::None;

		// Self-preservation.
		float m_hurtTimer = 0.0f;
		glm::vec3 m_attackerPos{0.0f};
		bool m_fleeing = false;
		float m_fleeTimer = 0.0f;
		float m_fleeRepick = 0.0f;

		// Prey (by id so a dead one is never dereferenced).
		uint32_t m_preyId = 0;
		float m_preyTime = 0.0f;
		float m_preyCooldown = 0.0f;
		float m_preyScanTimer = 0.0f;

		bool m_sleeping = false;
		float m_sleepTimer = 0.0f;
		float m_awakeTimer = 0.0f;
		float m_awakeLimit = 90.0f;
		PerceptionConfig m_awakeSenses;

		AIPerception m_perception;
		ai::AIContext m_aiContext;
		ai::BehaviorTree m_behaviorTree;

		CarnivoreState m_stateEnum = CarnivoreState::Idle;
	};

} // namespace game
