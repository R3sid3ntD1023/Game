#pragma once
#include "eng/Component.h"
#include "eng/RigidBody.h"
#include <glm/glm.hpp>
#include <string>

namespace game
{

    struct WeaponStats
    {
        std::string name = "Rifle";
        float damage = 35.0f;
        float cooldown = 0.25f; // seconds between shots
        float range = 100.0f;
        int magSize = 12;
        int reserve = 36;
        bool automatic = false;
        float reloadTime = 1.4f;
    };

    // Hitscan weapon held by the player. Unarmed until a WeaponPickup calls Equip().
    class WeaponComponent : public eng::Component
    {
    public:
        void AddWeapon(const WeaponStats &stats);

        void Equip(const WeaponStats &stats);

        void SelectWeapon(int offset);

        // Returns true if a shot was fired. `ignore` is the shooter's own body.
        bool Fire(const glm::vec3 &origin, const glm::vec3 &direction, const eng::RigidBody *ignore);
        void Reload();
        void AddAmmo(int rounds) { m_reserve += rounds; }

        void OnUpdate(float dt) override;

        bool Equipped() const { return m_equipped; }
        bool Reloading() const { return m_reloading; }
        float ReloadProgress() const { return m_reloading && m_currentStats.reloadTime > 0.0f ? 1.0f - m_reloadTimer / m_currentStats.reloadTime : 1.0f; }
        int Ammo() const { return m_ammo; }
        int Reserve() const { return m_reserve; }
        const WeaponStats &Stats() const { return m_currentStats; }

    private:
        std::vector<WeaponStats> m_stats;
        bool m_equipped = false;
        bool m_reloading = false;
        int m_ammo = 0;
        int m_reserve = 0;
        float m_cooldown = 0.0f;
        float m_reloadTimer = 0.0f;
        WeaponStats m_currentStats;
        int m_selectedWeapon = 0;
    };

} // namespace game
