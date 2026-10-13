#include "game/Weapon.h"
#include "eng/BuiltinComponents.h"
#include "eng/GameObject.h"
#include "eng/World.h"
#include <algorithm>

namespace game
{

    void WeaponComponent::Equip(const WeaponStats &stats)
    {
        if (m_equipped && m_stats.name == stats.name)
        {
            m_reserve += stats.magSize + stats.reserve; // picking up the same weapon again gives ammo
            return;
        }
        m_stats = stats;
        m_equipped = true;
        m_reloading = false;
        m_ammo = stats.magSize;
        m_reserve = stats.reserve;
        m_cooldown = 0.0f;
    }

    bool WeaponComponent::Fire(const glm::vec3 &origin, const glm::vec3 &direction, const eng::RigidBody *ignore)
    {
        if (!m_equipped || m_reloading || m_cooldown > 0.0f)
            return false;
        if (m_ammo <= 0)
        {
            Reload();
            return false;
        }

        --m_ammo;
        m_cooldown = m_stats.cooldown;

        eng::RayHit hit;
        if (gameObject->GetWorld().Physics().Raycast(origin, direction, m_stats.range, ignore, hit) && hit.body)
        {
            if (auto *health = hit.body->gameObject->GetComponent<eng::Health>())
                health->Damage(m_stats.damage);
        }
        return true;
    }

    void WeaponComponent::Reload()
    {
        if (!m_equipped || m_reloading || m_ammo >= m_stats.magSize || m_reserve <= 0)
            return;
        m_reloading = true;
        m_reloadTimer = m_stats.reloadTime;
    }

    void WeaponComponent::OnUpdate(float dt)
    {
        m_cooldown = std::max(0.0f, m_cooldown - dt);
        if (!m_reloading)
            return;

        m_reloadTimer -= dt;
        if (m_reloadTimer <= 0.0f)
        {
            const int take = std::min(m_stats.magSize - m_ammo, m_reserve);
            m_ammo += take;
            m_reserve -= take;
            m_reloading = false;
        }
    }

} // namespace game
