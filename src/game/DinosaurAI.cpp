#include "game/DinosaurAI.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>

namespace game
{

    void DinosaurAI::OnStart()
    {
        m_body = gameObject->GetComponent<eng::RigidBody>();
        m_health = gameObject->GetComponent<eng::Health>();
        m_targetHealth = m_target.GetComponent<eng::Health>();
        m_animator = gameObject->GetComponent<eng::Animator>();
    }

    void DinosaurAI::OnUpdate(float dt)
    {
        m_attackTimer = std::max(0.0f, m_attackTimer - dt);
        if (m_health && m_health->Dead())
        {
            ++m_state.kills;
            spdlog::info("Dinosaur down ({} kills)", m_state.kills);
            gameObject->Destroy();
        }
    }

    void DinosaurAI::OnFixedUpdate(float dt)
    {
        if (!m_body || (m_health && m_health->Dead()))
            return;

        Transform &self = gameObject->GetTransform();
        glm::vec3 toTarget = m_target.GetTransform().position - self.position;
        toTarget.y = 0.0f;
        const float dist = glm::length(toTarget);

        glm::vec3 dir{0.0f};
        float speed = 0.0f;
        if (dist < m_attackRange)
        {
            if (m_attackTimer <= 0.0f && m_targetHealth)
            {
                m_targetHealth->Damage(m_damage);
                m_attackTimer = 1.0f;
            }
            if (dist > 1e-3f)
                dir = toTarget / dist;
        }
        else if (dist < m_aggroRange)
        {
            dir = toTarget / dist;
            speed = m_speed;
        }
        else
        {
            m_wanderTimer -= dt;
            if (m_wanderTimer <= 0.0f)
            {
                const float a = static_cast<float>(std::rand() % 628) / 100.0f;
                m_wanderDir = (std::rand() % 3 == 0) ? glm::vec3(0.0f) : glm::vec3(std::cos(a), 0.0f, std::sin(a));
                m_wanderTimer = 2.0f + static_cast<float>(std::rand() % 300) / 100.0f;
            }
            dir = m_wanderDir;
            speed = m_speed * 0.3f;
        }

        m_body->velocity.x = dir.x * speed;
        m_body->velocity.z = dir.z * speed;
        if (m_animator)
        {
            const float blend = m_speed > 0.0f ? speed / m_speed : 0.0f;
            m_animator->blend = blend;
            m_animator->speed = 0.7f + 1.3f * blend;
        }
        // Mesh long axis is local +X; rotating about Y by theta maps it to (cos t, 0, -sin t).
        if (glm::dot(dir, dir) > 0.0f)
            self.rotation = glm::angleAxis(std::atan2(-dir.z, dir.x), glm::vec3(0, 1, 0));
    }

} // namespace game
