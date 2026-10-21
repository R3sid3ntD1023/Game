#include "game/AIPerception.h"
#include "eng/RigidBody.h"
#include "eng/World.h"
#include <algorithm>
#include <cmath>

namespace game
{

	namespace
	{
		glm::vec3 Forward(eng::GameObject &self)
		{
			// Dinosaurs face +X.
			glm::vec3 f = self.GetTransform().rotation * glm::vec3(1.0f, 0.0f, 0.0f);
			f.y = 0.0f;
			const float len = glm::length(f);
			return len > 1e-4f ? f / len : glm::vec3(1.0f, 0.0f, 0.0f);
		}
	} // namespace

	void AIPerception::Raise(float amount, Stimulus stimulus, const glm::vec3 &where)
	{
		m_awareness = std::min(m_awareness + amount, 2.0f * config.alertThreshold);
		m_lastStimulus = stimulus;
		m_lastKnown = where;
		m_stimulatedThisFrame = true;
	}

	bool AIPerception::CheckSight(eng::GameObject &self, eng::GameObject &target) const
	{
		const glm::vec3 eye = self.GetTransform().position + glm::vec3(0.0f, config.eyeHeight, 0.0f);
		const glm::vec3 to = target.GetTransform().position - eye;
		const float dist = glm::length(to);
		if (dist > config.sightRange)
			return false;
		if (dist < 1e-3f)
			return true;

		glm::vec3 flat = to;
		flat.y = 0.0f;
		const float flatLen = glm::length(flat);
		if (flatLen > config.closeRange * 0.25f && glm::dot(flat / flatLen, Forward(self)) < std::cos(glm::radians(config.sightFovDegrees * 0.5f)))
			return false;

		eng::RayHit hit;
		const auto *selfBody = self.GetComponent<eng::RigidBody>();
		if (!self.GetWorld().Physics().Raycast(eye, to / dist, dist, selfBody, hit))
			return true;
		return (hit.body && hit.body->gameObject == &target) || hit.distance >= dist - 0.3f;
	}

	void AIPerception::Update(float dt, eng::GameObject &self, eng::GameObject &target)
	{
		eng::World &world = self.GetWorld();
		const glm::vec3 selfPos = self.GetTransform().position;
		const glm::vec3 targetPos = target.GetTransform().position;
		m_stimulatedThisFrame = false;

		// Sight (raycasts are throttled; the cone and range test is cheap enough to run first each time).
		m_sightTimer -= dt;
		if (m_sightTimer <= 0.0f)
		{
			m_sightTimer = config.sightInterval;
			m_sees = CheckSight(self, target);
		}
		if (m_sees)
		{
			const float dist = glm::distance(selfPos, targetPos);
			const float falloff = 1.0f - std::clamp(dist / config.sightRange, 0.0f, 1.0f);
			float rate = config.sightRate * (0.25f + 0.75f * falloff);
			if (dist < config.closeRange)
				rate *= 3.0f;
			Raise(rate * dt, Stimulus::Sight, targetPos);
		}

		// Hearing: noises are shared events; only react to each once and ignore our own.
		uint32_t newest = m_lastNoiseId;
		for (const auto &n : world.Noises())
		{
			if (n.id <= m_lastNoiseId || n.source == &self)
				continue;
			newest = std::max(newest, n.id);
			const float reach = n.radius * config.hearingSensitivity;
			const float d = glm::distance(selfPos, n.position);
			if (d <= reach)
				Raise((1.0f - d / reach) * 1.6f + 0.2f, Stimulus::Hearing, n.position);
		}
		m_lastNoiseId = newest;

		// Touch: bumping into the target is always noticed.
		glm::vec3 flat = targetPos - selfPos;
		flat.y = 0.0f;
		if (glm::length(flat) < config.touchRadius)
			Raise(2.0f * config.alertThreshold, Stimulus::Touch, targetPos);

		if (m_awareness >= config.alertThreshold)
		{
			m_alerted = true;
			if (m_sees || m_stimulatedThisFrame)
				m_memoryTimer = config.memory;
		}

		if (m_alerted)
		{
			if (!m_sees && !m_stimulatedThisFrame)
				m_memoryTimer -= dt;
			if (m_memoryTimer <= 0.0f)
			{
				m_alerted = false;
				m_awareness = 0.4f * config.alertThreshold;
			}
		}
		else if (!m_stimulatedThisFrame)
		{
			m_awareness = std::max(0.0f, m_awareness - config.decayRate * dt);
		}
	}

	void AIPerception::OnDamaged(float amount, const glm::vec3 &attackerPosition)
	{
		if (amount >= config.damageThreshold)
			Raise(2.0f * config.alertThreshold, Stimulus::Damage, attackerPosition);
		else
			Raise(0.5f * config.alertThreshold * amount / config.damageThreshold, Stimulus::Damage, attackerPosition);
		if (m_awareness >= config.alertThreshold)
		{
			m_alerted = true;
			m_memoryTimer = config.memory;
		}
	}

	void AIPerception::NotifyArrived()
	{
		m_memoryTimer = std::min(m_memoryTimer, 3.0f);
	}

	void AIPerception::DrawDebug(eng::GameObject &self, const glm::vec3 &targetPosition) const
	{
		gfx::LineRenderer &lines = self.GetWorld().Lines();
		const glm::vec3 pos = self.GetTransform().position;
		const glm::vec3 ground = pos - glm::vec3(0.0f, 0.4f, 0.0f);
		const glm::vec3 fwd = Forward(self);
		const glm::vec3 right{-fwd.z, 0.0f, fwd.x};

		const glm::vec3 green{0.2f, 1.0f, 0.3f}, yellow{1.0f, 0.9f, 0.1f}, red{1.0f, 0.15f, 0.1f};
		const glm::vec3 coneColor = m_alerted ? red : (m_awareness > 0.05f ? yellow : green);

		// Sight cone: two edges plus the far arc, and the "instant notice" radius.
		const float half = glm::radians(config.sightFovDegrees * 0.5f);
		const float heading = std::atan2(fwd.z, fwd.x);
		const glm::vec3 e0 = ground + config.sightRange * glm::vec3(std::cos(heading - half), 0.0f, std::sin(heading - half));
		const glm::vec3 e1 = ground + config.sightRange * glm::vec3(std::cos(heading + half), 0.0f, std::sin(heading + half));
		lines.QueueLine({ground, e0, coneColor});
		lines.QueueLine({ground, e1, coneColor});
		lines.QueueArc(ground, {1, 0, 0}, {0, 0, 1}, config.sightRange, heading - half, heading + half, coneColor, 24);
		lines.QueueCircle(ground, config.closeRange, coneColor * 0.6f, 24);

		// Touch range.
		lines.QueueCircle(ground, config.touchRadius, {0.9f, 0.2f, 0.9f}, 16);

		// Awareness bar above the head: fills up to the alert threshold, turns red once alerted.
		const glm::vec3 barBase = pos + glm::vec3(0.0f, 2.0f, 0.0f);
		const float fill = m_alerted ? 1.0f : std::clamp(m_awareness / config.alertThreshold, 0.0f, 1.0f);
		lines.QueueLine({barBase - right * 0.6f, barBase + right * 0.6f, glm::vec3(0.3f)});
		lines.QueueLine({barBase - right * 0.6f + glm::vec3(0, 0.05f, 0), barBase - right * 0.6f + right * 1.2f * fill + glm::vec3(0, 0.05f, 0), m_alerted ? red : yellow});
		lines.QueueLine({barBase, barBase + glm::vec3(0.0f, 0.5f * fill, 0.0f), m_alerted ? red : yellow});

		// What it knows: a green line to the target while visible, a red cross at the last known position while hunting.
		if (m_sees)
			lines.QueueLine({pos + glm::vec3(0.0f, config.eyeHeight, 0.0f), targetPosition, green});
		if (m_alerted)
		{
			lines.QueueLine({pos, m_lastKnown, red});
			lines.QueueLine({m_lastKnown - glm::vec3(0.7f, 0, 0), m_lastKnown + glm::vec3(0.7f, 0, 0), red});
			lines.QueueLine({m_lastKnown - glm::vec3(0, 0, 0.7f), m_lastKnown + glm::vec3(0, 0, 0.7f), red});
			lines.QueueLine({m_lastKnown, m_lastKnown + glm::vec3(0, 3.0f, 0), red});
		}
	}

} // namespace game
