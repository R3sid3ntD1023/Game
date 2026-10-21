#include "game/FirstPersonController.h"
#include "eng/GameObject.h"
#include "eng/World.h"
#include "game/Weapon.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <string>

namespace game
{

	void FirstPersonController::OnStart()
	{
		m_body = gameObject->GetComponent<eng::RigidBody>();
	}

	void FirstPersonController::OnKey(int key, int action)
	{
		const bool down = action != GLFW_RELEASE;
		switch (key)
		{
		case GLFW_KEY_W:
			m_forward = down;
			break;
		case GLFW_KEY_S:
			m_back = down;
			break;
		case GLFW_KEY_A:
			m_left = down;
			break;
		case GLFW_KEY_D:
			m_right = down;
			break;
		case GLFW_KEY_LEFT_SHIFT:
			m_sprint = down;
			break;
		case GLFW_KEY_SPACE:
			m_jump = down;
			break;
		case GLFW_KEY_R:
			if (action == GLFW_PRESS)
				m_reloadPressed = true;
			break;
		case GLFW_KEY_F:
			if (action == GLFW_PRESS)
			{
				eng::World &world = gameObject->GetWorld();
				if (eng::GameObject *flashlight = world.Find("Flashlight"))
					if (auto *light = world.Registry().try_get<Light>(flashlight->Entity()))
					{
						light->enabled = !light->enabled;
						if (world.audio)
							world.audio->Play2D("flashlight", 0.6f);
					}
			}
			break;
		default:
			break;
		}
	}

	void FirstPersonController::OnMouseMove(float dx, float dy)
	{
		m_yaw += dx * mouseSensitivity;
		m_pitch = std::clamp(m_pitch - dy * mouseSensitivity, -89.0f, 89.0f);
	}

	void FirstPersonController::OnMouseButton(int button, int action)
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT)
		{
			m_fireHeld = action == GLFW_PRESS;
			if (action == GLFW_PRESS)
				m_firePressed = true;
		}
	}

	void FirstPersonController::OnMouseScroll(float /*xOffset*/, float yOffset)
	{
		gameObject->GetComponent<WeaponComponent>()->SelectWeapon((yOffset > 0 ? 1 : -1));
	}

	void FirstPersonController::OnUpdate(float dt)
	{
		UpdateFootsteps(dt);
		if (auto *weapon = gameObject->GetComponent<WeaponComponent>())
		{
			if (m_reloadPressed)
				weapon->Reload();
			if (weapon->Stats().automatic ? m_fireHeld : m_firePressed)
				Shoot();
		}
		m_firePressed = false;
		m_reloadPressed = false;

		const float yaw = glm::radians(m_yaw), pitch = glm::radians(m_pitch);
		gfx::Camera &cam = gameObject->GetWorld().GetCamera();
		cam.position = gameObject->GetTransform().position + glm::vec3(0.0f, 0.7f, 0.0f);
		cam.forward = {std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch)};
	}

	void FirstPersonController::OnFixedUpdate(float dt)
	{
		if (!m_body)
			return;
		const float yaw = glm::radians(m_yaw);
		const glm::vec3 fwd{std::cos(yaw), 0.0f, std::sin(yaw)};
		const glm::vec3 right{-std::sin(yaw), 0.0f, std::cos(yaw)};

		glm::vec3 wish = fwd * float(m_forward - m_back) + right * float(m_right - m_left);
		if (glm::dot(wish, wish) > 0.0f)
			wish = glm::normalize(wish);
		// Water drags: slower on the move, with a weaker jump.
		const bool wading = gameObject->GetWorld().underwater;
		const float drag = wading ? 0.45f : 1.0f;
		const glm::vec3 target = wish * (m_sprint ? sprintSpeed : walkSpeed) * drag;

		const float accel = m_body->grounded ? 12.0f : 3.0f; // limited air control
		const float k = std::min(1.0f, accel * dt);
		m_body->velocity.x += (target.x - m_body->velocity.x) * k;
		m_body->velocity.z += (target.z - m_body->velocity.z) * k;

		if (m_jump && m_body->grounded)
			m_body->velocity.y = jumpSpeed * (wading ? 0.6f : 1.0f);
	}

	void FirstPersonController::UpdateFootsteps(float dt)
	{
		audio::AudioEngine *audio = gameObject->GetWorld().audio;
		if (!audio || !m_body)
			return;

		const float speed = std::sqrt(m_body->velocity.x * m_body->velocity.x + m_body->velocity.z * m_body->velocity.z);
		if (!m_body->grounded || speed < 1.0f)
			return;

		const float stride = m_sprint ? 2.6f : 2.0f;
		m_stepDistance += speed * dt;
		if (m_stepDistance < stride)
			return;
		m_stepDistance -= stride;

		int variant;
		do
			variant = std::rand() % 4;
		while (variant == m_lastStep);
		m_lastStep = variant;
		audio->Play2D("footstep_" + std::to_string(variant), m_sprint ? 0.9f : 0.6f, 0.9f + 0.2f * static_cast<float>(std::rand() % 100) / 100.0f);
		gameObject->GetWorld().EmitNoise(gameObject->GetTransform().position, m_sprint ? 18.0f : 8.0f, gameObject);
	}

	void FirstPersonController::Shoot()
	{
		if (auto *weapon = gameObject->GetComponent<WeaponComponent>())
		{
			const gfx::Camera &cam = gameObject->GetWorld().GetCamera();
			weapon->Fire(cam.position, cam.forward, m_body);
		}
	}
} // namespace game
