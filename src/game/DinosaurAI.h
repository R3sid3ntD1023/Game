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
	class CarnivoreAI;
	class HerbivoreAI;

	// How many healthy raptors must be together to take on a T-Rex instead of running from it.
	inline constexpr int kRexPackSize = 3;

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
		Fleeing,  // running from a threat that is too great, or while hurt and under attack
		Dead
	};

	// Brain: sight, hearing, touch and damage feed AIPerception; once alerted it chases the last known position
	// and bites in range. After losing the target it hunts (searches the area around where it was last seen).
	// Otherwise it wanders, and after being awake for a while it lies down to sleep with dulled senses.
	// It preys on any herbivore. A pack roamer with company calls the others with a roar and they attack together once
	// at least two are committed; a lone raptor weighs the risk first (small prey is an easy meal, large prey only for a
	// bold, healthy raptor with no T-Rex or herd nearby). Healthy packs of three also take on a T-Rex.
	// Self-preservation overrides all of that: attacked while hurt, or close to a T-Rex, it runs; a wounded raptor
	// that is left alone lies down, and sleeping heals it.
	class DinosaurAI : public eng::Script
	{
	public:
		DinosaurAI(eng::GameObject &target, GameState &state, float speed, float damage, bool packRoaming = false)
			: m_target(target),
			  m_state(state),
			  m_ecology(state.ecology),
			  m_speed(speed),
			  m_damage(damage),
			  m_packRoaming(packRoaming)
		{
		}
		~DinosaurAI() override;

		void OnStart() override;
		void OnUpdate(float dt) override;
		void OnFixedUpdate(float dt) override;

		bool Dead() const { return m_health && m_health->Dead(); }
		float HealthFraction() const { return m_health && m_health->max > 0.0f ? m_health->current / m_health->max : 0.0f; }
		uint32_t PreyId() const { return m_preyId; }
		// True when this raptor roams with others instead of alone.
		bool PackRoaming() const { return m_packRoaming; }
		glm::vec3 Position() const { return gameObject->GetTransform().position; }
		// Damage from another animal: no reaction to the player and no kill credit. `from` is where the attacker stands.
		void Hurt(float amount, const glm::vec3 *from = nullptr);

	private:
		void InitializeBehaviorTree();
		void UpdateAudio(float dt);
		void UpdateAnimation(float dt);
		void BuildAnimationGraph();

		void SetState(DinosaurState state);
		void OnStateChanged(DinosaurState state);
		void UpdateRest(float dt);
		void UpdateFlee(float dt);
		void UpdateGroup();
		void StartSleep();
		void WakeUp();
		glm::vec3 PickHuntPoint() const;

		void UpdatePrey(float dt);
		void AbandonPrey(float cooldown);
		bool WillRiskSolo(const HerbivoreAI &prey) const;
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
		bool m_packRoaming;
		DinosaurAI *m_leader = nullptr; // the pack member this raptor follows while wandering; null = it leads or roams solo
		float m_attackTimer = 0.0f;
		float m_stepDistance = 0.0f;
		float m_attackRange = 3.2f;
		std::shared_ptr<eng::AnimStateMachine> m_animGraph;

		enum class Branch { None, Flee, Rex, Attack, Chase, Sleep, Hunt, Wander, Follow, Prey };
		Branch m_branch = Branch::None;

		// Self-preservation: recently attacked by anything, and running from a threat that is too great.
		float m_hurtTimer = 0.0f;
		glm::vec3 m_attackerPos{0.0f};
		glm::vec3 m_pendingAttacker{0.0f};
		glm::vec3 m_fleeFrom{0.0f};
		CarnivoreAI *m_rex = nullptr; // set each frame while this raptor is part of a pack attacking a T-Rex
		bool m_fleeing = false;
		float m_fleeTimer = 0.0f;
		float m_fleeRepick = 0.0f;

		// Pack hunting: the prey (by id, so a dead one is never dereferenced), whether the pack was called, and timers.
		uint32_t m_preyId = 0;
		bool m_preyCalled = false;
		bool m_preySolo = false; // a lone raptor that judged the risk worth it
		float m_boldness = 0.5f; // 0..1, how readily it risks a solo attack on big prey
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
