#include "eng/Lighting.h"
#include "eng/GameObject.h"
#include "eng/World.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>

namespace eng
{

	void LightComponent::OnAttach()
	{
		auto &registry = gameObject->GetWorld().Registry();
		registry.emplace_or_replace<Light>(gameObject->Entity(), Light{type, color, intensity, range, innerDegrees, outerDegrees, true});
		registry.get_or_emplace<EmissiveScale>(gameObject->Entity());
	}

	void LightComponent::SetEnabled(bool enabled)
	{
		if (auto *light = gameObject->GetWorld().Registry().try_get<Light>(gameObject->Entity()))
			light->enabled = enabled;
	}

	void CameraFollower::OnUpdate(float)
	{
		const gfx::Camera &cam = gameObject->GetWorld().GetCamera();
		const glm::vec3 right = glm::normalize(glm::cross(cam.forward, glm::vec3(0, 1, 0)));
		const glm::vec3 up = glm::cross(right, cam.forward);

		Transform &t = gameObject->GetTransform();
		t.position = cam.position + right * offset.x + up * offset.y + cam.forward * offset.z;
		t.rotation = glm::quatLookAt(cam.forward, glm::vec3(0, 1, 0));
	}

	void LightFlicker::OnStart()
	{
		if (auto *light = gameObject->GetWorld().Registry().try_get<Light>(gameObject->Entity()))
			m_base = light->intensity;
		m_timer = Rand(0.5f, 6.0f);
	}

	void LightFlicker::OnUpdate(float dt)
	{
		auto &registry = gameObject->GetWorld().Registry();
		auto *light = registry.try_get<Light>(gameObject->Entity());
		if (!light)
			return;

		m_time += dt;
		m_timer -= dt;
		if (m_timer <= 0.0f)
		{
			m_burst = !m_burst;
			m_timer = m_burst ? Rand(0.15f, 0.6f) : Rand(2.0f, 9.0f);
		}

		float level = 1.0f - 0.08f * m_amount * (0.5f + 0.5f * std::sin(m_time * 23.0f)); // slow hum
		if (m_burst)
		{
			m_stutter -= dt;
			if (m_stutter <= 0.0f)
			{
				m_stutter = Rand(0.02f, 0.08f);
				m_burstLevel = Rand(0.0f, 1.0f) < 0.45f ? 0.0f : Rand(0.2f, 1.0f);
			}
			level = glm::mix(1.0f, m_burstLevel, std::clamp(m_amount, 0.0f, 1.0f));
		}

		light->intensity = m_base * level;
		if (auto *emissive = registry.try_get<EmissiveScale>(gameObject->Entity()))
			emissive->value = level;
	}

} // namespace eng
