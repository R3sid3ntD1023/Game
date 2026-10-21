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

	enum class DinosaurState
	{
		Idle,
		Wandering,
		Hunting,
		Sleeping,
		Chasing,
		Attacking,
		Calling,  // roaring to bring the pack in on large prey
		Stalking, // closing in on prey
		Dead
	};

	// Brain: sight, hearing, touch and damage feed AIPerception; once alerted it chases the last known position
	// and bites in range. After losing the target it hunts (searches the area around where it was last seen).
	// Otherwise it wanders, and after being awake for a while it lies down to sleep with dulled senses.
	// It also preys on large herbivores, but only as a pack: the raptor that spots one calls the others with a
	// roar, and they attack together once at least two are committed. Small herbivores are ignored.
	class DinosaurAI : public eng::Script
	{
	public:
		DinosaurAI(eng::GameObject &target, GameState &state, float speed, float damage)
			: m_target(target),
			  m_state(state),
			  m_ecology(state.ecology),
			  m_speed(speed),
			  m_damage(damage)
		{
		}
		~DinosaurAI() override;

		void OnStart() override;
		void OnUpdate(float dt) override;
		void OnFixedUpdate(float dt) override;

		bool Dead() const { return m_health && m_health->Dead(); }
		uint32_t PreyId() const { return m_preyId; }
		// Damage from another animal: no reaction to the player and no kill credit.
		void Hurt(float amount);

	private:
		void InitializeBehaviorTree();
		void UpdateAudio(float dt);
		void UpdateAnimation(float dt);

		void SetState(DinosaurState state);
		void OnStateChanged(DinosaurState state);
		void UpdateRest(float dt);
		void StartSleep();
		void WakeUp();
		glm::vec3 PickHuntPoint() const;

		void UpdatePrey(float dt);
		void AbandonPrey(float cooldown);
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
		float m_stepDistance = 0.0f;
		float m_attackRange = 3.2f;
		float m_poseBlend = 0.0f;

		enum class Branch { None, Attack, Chase, Sleep, Hunt, Wander, Prey };
		Branch m_branch = Branch::None;

		// Pack hunting: the prey (by id, so a dead one is never dereferenced), whether the pack was called, and timers.
		uint32_t m_preyId = 0;
		bool m_preyCalled = false;
		float m_preyTime = 0.0f;
		float m_preyCooldown = 0.0f;
		float m_preyScanTimer = 0.0f;
		float m_preyBiteTimer = 0.0f;
		uint32_t m_lastCallId = 0;
		bool m_animalDamage = false;
		bool m_registered = false;

		// Sleep cycle: awake for a while, then sleep; asleep it only notices close or loud stimuli.
		bool m_sleeping = false;
		float m_sleepTimer = 0.0f;
		float m_awakeTimer = 0.0f;
		float m_awakeLimit = 60.0f;
		PerceptionConfig m_awakeSenses;

		// Hunt: search around the place the target was last known after losing it.
		bool m_wasAlerted = false;
		float m_huntTimer = 0.0f;
		float m_huntRepick = 0.0f;
		glm::vec3 m_huntCentre{0.0f};
		glm::vec3 m_lastAlertPos{0.0f};

		AIPerception m_perception;
		ai::AIContext m_aiContext;
		ai::BehaviorTree m_behaviorTree;

		DinosaurState m_stateEnum = DinosaurState::Idle;
		DinosaurState m_prevStateEnum = DinosaurState::Idle;
	};

} // namespace game
