#pragma once
#include "eng/GameObject.h"
#include <cstdint>
#include <glm/glm.hpp>

namespace game
{

	struct PerceptionConfig
	{
		// Sight: a cone with line-of-sight checks. Awareness builds faster the closer the target is.
		float sightRange = 40.0f;
		float sightFovDegrees = 110.0f;
		float closeRange = 6.0f;   // inside this the target is noticed almost instantly
		float sightRate = 1.2f;	   // awareness per second at the edge of close range
		float eyeHeight = 0.5f;	   // above the transform position
		float sightInterval = 0.1f; // seconds between line-of-sight raycasts

		// Hearing: a noise is audible inside radius * hearingSensitivity and adds awareness the nearer it is.
		float hearingSensitivity = 1.0f;

		// Touch and damage: instant alert when the target is this close, or when a hit is at least this strong.
		float touchRadius = 1.6f;
		float damageThreshold = 5.0f;

		float alertThreshold = 1.0f; // awareness needed to start chasing
		float decayRate = 0.25f;	 // awareness lost per second without stimuli
		float memory = 8.0f;		 // seconds the AI keeps hunting after losing track
	};

	enum class Stimulus
	{
		None,
		Sight,
		Hearing,
		Touch,
		Damage
	};

	// Gathers sight, hearing, touch and damage into a single awareness value. Once it crosses the alert threshold
	// the owner should chase LastKnownPosition(); it stays alerted for `memory` seconds after the last stimulus.
	class AIPerception
	{
	public:
		PerceptionConfig config;

		void Update(float dt, eng::GameObject &self, eng::GameObject &target);
		void OnDamaged(float amount, const glm::vec3 &attackerPosition);
		// Called by the owner when it reached the last known position without finding anything.
		void NotifyArrived();
		void DrawDebug(eng::GameObject &self, const glm::vec3 &targetPosition) const;

		bool Alerted() const { return m_alerted; }
		bool SeesTarget() const { return m_sees; }
		float Awareness() const { return m_awareness; }
		Stimulus LastStimulus() const { return m_lastStimulus; }
		const glm::vec3 &LastKnownPosition() const { return m_lastKnown; }

	private:
		bool CheckSight(eng::GameObject &self, eng::GameObject &target) const;
		void Raise(float amount, Stimulus stimulus, const glm::vec3 &where);

		float m_awareness = 0.0f;
		bool m_alerted = false;
		bool m_sees = false;
		bool m_stimulatedThisFrame = false;
		float m_memoryTimer = 0.0f;
		float m_sightTimer = 0.0f;
		uint32_t m_lastNoiseId = 0;
		Stimulus m_lastStimulus = Stimulus::None;
		glm::vec3 m_lastKnown{0.0f};
	};

} // namespace game
