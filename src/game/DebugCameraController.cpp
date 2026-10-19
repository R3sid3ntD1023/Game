#include "DebugCameraController.h"
#include <GLFW/glfw3.h>

namespace game
{
	void DebugCameraController::Update(gfx::Camera &camera, const core::Input &input)
	{
		glm::vec3 dir{0, 0, 0};
		if (input.IsKeyDown(GLFW_KEY_W)) // forward
			dir.z += m_sensitivity;
		if (input.IsKeyDown(GLFW_KEY_S)) // backward
			dir.z -= m_sensitivity;
		if (input.IsKeyDown(GLFW_KEY_A)) // left
			dir.x -= m_sensitivity;
		if (input.IsKeyDown(GLFW_KEY_D)) // right
			dir.x += m_sensitivity;
		if (input.IsKeyDown(GLFW_KEY_Q)) // down
			dir.y -= m_sensitivity;
		if (input.IsKeyDown(GLFW_KEY_E)) // up
			dir.y += m_sensitivity;

		if (dir != glm::vec3(0, 0, 0))
			Pan(dir.x, dir.y, dir.z);

		auto mouseDelta = input.MouseDelta();
		if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)
			Rotate(mouseDelta.x * m_sensitivity, mouseDelta.y * m_sensitivity);

		if (input.GetMouseScroll() != 0.0f)
			Zoom(input.GetMouseScroll() * m_sensitivity);

		camera.position = m_position;
		camera.forward = GetForward();
	}

	void DebugCameraController::Zoom(float delta)
	{
		float zoomFactor = powf(.95f, delta);
		m_position.z += zoomFactor;
	}

	void DebugCameraController::Rotate(float yawDelta, float pitchDelta)
	{
		m_yaw += yawDelta;
		m_pitch -= pitchDelta;
		m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
	}

	void DebugCameraController::Pan(float xDelta, float yDelta, float zDelta)
	{
		m_position += GetRight() * xDelta + GetUp() * yDelta + GetForward() * zDelta;
	}

	glm::vec3 DebugCameraController::GetRight() const
	{
		return glm::normalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
	}

	glm::vec3 DebugCameraController::GetUp() const
	{
		return glm::normalize(glm::cross(GetRight(), GetForward()));
	}

	glm::vec3 DebugCameraController::GetForward() const
	{
		glm::vec3 fwd;
		fwd.x = glm::cos(glm::radians(m_yaw)) * glm::cos(glm::radians(m_pitch));
		fwd.y = glm::sin(glm::radians(m_pitch));
		fwd.z = glm::sin(glm::radians(m_yaw)) * glm::cos(glm::radians(m_pitch));
		return glm::normalize(fwd);
	}

} // namespace game