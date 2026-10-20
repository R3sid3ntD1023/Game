#pragma once

#include "core/Input.h"
#include "gfx/Camera.h"
#include <glm/glm.hpp>
namespace game
{
	class DebugCameraController
	{
	public:
		void Update(gfx::Camera &camera, const core::Input &input);

	private:
		void Zoom(float delta);
		void Rotate(float yawDelta, float pitchDelta);
		void Pan(float x, float y, float z);

		glm::vec3 GetRight() const;
		glm::vec3 GetUp() const;
		glm::vec3 GetForward() const;

	private:
		float m_sensitivity = 0.5f;
		float m_yaw = -90.0f;
		float m_pitch = 0.0f;

		glm::vec3 m_position = glm::vec3(0.0f);
		float m_distance = 5.0f;
	};
} // namespace game