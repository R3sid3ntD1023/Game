#include "game/DinosaurAI.h"
#include "eng/World.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <string>

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
            gameObject->GetWorld().EmitBlood(gameObject->GetTransform().position, glm::vec3(0.0f, 1.0f, 0.0f), 60);
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
                gameObject->GetWorld().EmitBlood(m_target.GetTransform().position + glm::vec3(0.0f, 0.5f, 0.0f),
                                                 toTarget / std::max(dist, 0.01f), 16);
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
        if (audio::AudioEngine *audio = gameObject->GetWorld().audio)
        {
            if (speed > 0.5f && m_body->grounded)
            {
                m_stepDistance += speed * dt;
                if (m_stepDistance > 2.8f)
                {
                    m_stepDistance = 0.0f;
                    audio->PlayOneShot("footstep_" + std::to_string(std::rand() % 4), self.position, 1.2f, 0.45f);
                }
            }
            const bool chasing = dist >= m_attackRange && dist < m_aggroRange;
            if (chasing && !m_chasing)
                audio->PlayOneShot("raptor_roar", self.position, 1.0f, 0.9f + 0.2f * static_cast<float>(std::rand() % 100) / 100.0f);
            m_chasing = chasing;
        }
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
