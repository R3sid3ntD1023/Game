#pragma once
#include "ai/BehaviorTree.h"
#include "ai/Blackboard.h"
#include "eng/Animator.h"
#include "eng/BuiltinComponents.h"
#include "eng/GameObject.h"
#include "eng/RigidBody.h"
#include "eng/Script.h"
#include "game/Ecology.h"
#include "game/GameState.h"
#include "game/HerbivoreModel.h"
#include "game/Plant.h"
#include <glm/glm.hpp>
#include <memory>

namespace game
{

	enum class HerbivoreState
	{
		Idle,
		Wandering,
		Seeking, // walking to a plant found by the food query
		Eating,
		Fleeing,
		Defending, // a triceratops charging and goring a raptor
		Dead
	};

	// Brain of a plant eater. It gets hungry over time, asks an environment query for the best plant (near, full,
	// unclaimed, away from raptors), walks there, and eats. Raptors nearby override that: the small protoceratops runs,
	// the large triceratops charges them. Otherwise it wanders and rests.
	class HerbivoreAI : public eng::Script
	{
	public:
		HerbivoreAI(HerbivoreSpecies species, eng::GameObject &player, GameState &state, float speed);
		~HerbivoreAI() override;

		void OnStart() override;
		void OnUpdate(float dt) override;

		uint32_t Id() const { return m_id; }
		bool Large() const { return m_species == HerbivoreSpecies::Triceratops; }
		bool Dead() const { return m_health && m_health->Dead(); }
		// Radius around the center within which a biting raptor must stand.
		float Radius() const { return Large() ? 1.8f : 0.8f; }
		glm::vec3 Position() const { return gameObject->GetTransform().position; }

		// Damage from another animal; unlike a player shot it does not make the herbivore panic about the player.
		void Hurt(float amount);

	private:
		void InitializeBehaviorTree();
		void SetState(HerbivoreState state);
		void OnStateChanged(HerbivoreState state);
		void UpdateAudio(float dt);
		void UpdateAnimation(float dt);
		void ReleaseClaim();
		void DrawDebug() const;

		DinosaurAI *FindThreat(float &distance) const;
		bool IsThreatened(ai::AIContext &ctx);
		ai::Node::State React(ai::AIContext &ctx);
		bool StepToward(const glm::vec3 &dir, float speed);

	private:
		HerbivoreSpecies m_species;
		eng::GameObject &m_player;
		GameState &m_state;
		std::shared_ptr<Ecology> m_ecology;
		eng::RigidBody *m_body = nullptr;
		eng::Health *m_health = nullptr;
		eng::Animator *m_animator = nullptr;
		float m_speed;
		uint32_t m_id = 0;
		bool m_registered = false;
		bool m_animalDamage = false;

		float m_hunger;
		float m_panicTimer = 0.0f; // after being shot: run from the player
		bool m_threatened = false;
		float m_goreTimer = 0.0f;
		float m_stepDistance = 0.0f;
		float m_chewTimer = 0.0f;
		float m_callTimer;
		float m_poseBlend = 0.0f;
		Plant *m_claimed = nullptr;

		ai::AIContext m_aiContext;
		ai::BehaviorTree m_behaviorTree;

		HerbivoreState m_stateEnum = HerbivoreState::Idle;
	};

} // namespace game
