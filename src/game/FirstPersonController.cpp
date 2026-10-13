#include "game/FirstPersonController.h"
#include "eng/GameObject.h"
#include "eng/World.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

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
            m_fireHeld = action == GLFW_PRESS;
    }

    void FirstPersonController::OnUpdate(float dt)
    {
        m_cooldown = std::max(0.0f, m_cooldown - dt);
        if (m_fireHeld && m_cooldown <= 0.0f)
        {
            Shoot();
            m_cooldown = fireCooldown;
        }

        const float yaw = glm::radians(m_yaw), pitch = glm::radians(m_pitch);
        gfx::Camera &cam = gameObject->GetWorld().camera;
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
        const glm::vec3 target = wish * (m_sprint ? sprintSpeed : walkSpeed);

        const float accel = m_body->grounded ? 12.0f : 3.0f; // limited air control
        const float k = std::min(1.0f, accel * dt);
        m_body->velocity.x += (target.x - m_body->velocity.x) * k;
        m_body->velocity.z += (target.z - m_body->velocity.z) * k;

        if (m_jump && m_body->grounded)
            m_body->velocity.y = jumpSpeed;
    }

    void FirstPersonController::Shoot()
    {
        const gfx::Camera &cam = gameObject->GetWorld().camera;
        eng::RayHit hit;
        if (!gameObject->GetWorld().Physics().Raycast(cam.position, cam.forward, 100.0f, m_body, hit) || !hit.body)
            return;

        if (auto *health = hit.body->gameObject->GetComponent<eng::Health>())
        {
            health->Damage(shotDamage);
            spdlog::info("Hit {} ({:.0f} hp left)", hit.body->gameObject->name, health->current);
        }
    }

} // namespace game
