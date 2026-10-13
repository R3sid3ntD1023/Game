#include "game/Interaction.h"
#include "eng/BuiltinComponents.h"
#include "eng/GameObject.h"
#include "eng/World.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <format>
#include <glm/gtc/quaternion.hpp>

namespace game
{

    namespace
    {

        eng::Interactable *FindCandidate(eng::World &world, eng::GameObject *self)
        {
            const gfx::Camera &cam = world.camera;
            eng::Interactable *best = nullptr;
            float bestScore = 1e9f;

            for (const auto &o : world.Objects())
            {
                if (!o->IsAlive() || o.get() == self)
                    continue;
                const glm::vec3 to = o->GetTransform().position - cam.position;
                const float dist = glm::length(to);
                if (dist > 8.0f)
                    continue; // cheap reject before the component lookup

                auto *item = o->GetComponent<eng::Interactable>();
                if (!item || !item->Available() || dist > item->range)
                    continue;

                const float facing = dist > 1e-3f ? glm::dot(to / dist, cam.forward) : 1.0f;
                if (facing < 0.85f && dist > 1.5f)
                    continue;

                const float score = dist * (2.0f - facing);
                if (score < bestScore)
                {
                    bestScore = score;
                    best = item;
                }
            }
            return best;
        }

    } // namespace

    void InteractionScript::OnKey(int key, int action)
    {
        if (key == GLFW_KEY_E && action == GLFW_PRESS)
            m_usePressed = true;
    }

    void InteractionScript::OnUpdate(float)
    {
        eng::Interactable *target = FindCandidate(gameObject->GetWorld(), gameObject);
        m_state.prompt = target ? "[E] " + target->Prompt() : std::string();
        if (m_usePressed && target)
            target->Interact(*gameObject);
        m_usePressed = false;
    }

    void Pickup::OnStart() { m_baseY = gameObject->GetTransform().position.y; }

    void Pickup::OnUpdate(float dt)
    {
        m_time += dt;
        Transform &t = gameObject->GetTransform();
        t.rotation = glm::angleAxis(m_time * 1.5f, glm::vec3(0, 1, 0));
        t.position.y = m_baseY + 0.15f * std::sin(m_time * 2.0f);
    }

    HealthPickup::HealthPickup(float amount) : m_amount(amount) { prompt = std::format("Take medkit (+{:.0f} HP)", amount); }

    void HealthPickup::Interact(eng::GameObject &who)
    {
        auto *health = who.GetComponent<eng::Health>();
        if (!health || health->current >= health->max)
            return;
        health->Heal(m_amount);
        gameObject->Destroy();
    }

    WeaponPickup::WeaponPickup(const WeaponStats &stats) : m_stats(stats) { prompt = "Pick up " + stats.name; }

    void WeaponPickup::Interact(eng::GameObject &who)
    {
        auto *weapon = who.GetComponent<WeaponComponent>();
        if (!weapon)
            return;
        weapon->Equip(m_stats);
        gameObject->Destroy();
    }

    Door::Door(std::string label, const glm::vec3 &openOffset, float openTime)
        : m_label(std::move(label)), m_offset(openOffset), m_openTime(std::max(openTime, 0.01f))
    {
        range = 4.5f;
    }

    std::string Door::Prompt() const { return (m_open ? "Close " : "Open ") + m_label; }

    void Door::Interact(eng::GameObject &) { m_open = !m_open; }

    void Door::OnStart() { m_closedPos = gameObject->GetTransform().position; }

    void Door::OnUpdate(float dt)
    {
        const float target = m_open ? 1.0f : 0.0f;
        const float step = dt / m_openTime;
        m_t = m_t < target ? std::min(m_t + step, target) : std::max(m_t - step, target);
        const float eased = m_t * m_t * (3.0f - 2.0f * m_t);
        gameObject->GetTransform().position = m_closedPos + m_offset * eased;
    }

} // namespace game
