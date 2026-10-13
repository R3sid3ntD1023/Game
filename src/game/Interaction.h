#pragma once
#include "eng/Interactable.h"
#include "eng/Script.h"
#include "game/GameState.h"
#include "game/Weapon.h"
#include <glm/glm.hpp>

namespace game {

// Player-side: finds the interactable being looked at, publishes its prompt, and uses it on the E key.
class InteractionScript : public eng::Script {
public:
    explicit InteractionScript(GameState& state) : m_state(state) {}

    void OnKey(int key, int action) override;
    void OnUpdate(float dt) override;

private:
    GameState& m_state;
    bool m_usePressed = false;
};

// Items spin and bob until picked up.
class Pickup : public eng::Interactable {
public:
    void OnStart() override;
    void OnUpdate(float dt) override;

private:
    float m_baseY = 0.0f;
    float m_time = 0.0f;
};

class HealthPickup : public Pickup {
public:
    explicit HealthPickup(float amount);
    void Interact(eng::GameObject& who) override;

private:
    float m_amount;
};

class WeaponPickup : public Pickup {
public:
    explicit WeaponPickup(const WeaponStats& stats);
    void Interact(eng::GameObject& who) override;

private:
    WeaponStats m_stats;
};

// Slides its GameObject by `openOffset` when used. Doors slide sideways, gates lift; the
// object's static RigidBody moves with it, so the opening becomes passable.
class Door : public eng::Interactable {
public:
    Door(std::string label, const glm::vec3& openOffset, float openTime);

    std::string Prompt() const override;
    void Interact(eng::GameObject& who) override;
    void OnStart() override;
    void OnUpdate(float dt) override;

    bool IsOpen() const { return m_open; }

private:
    std::string m_label;
    glm::vec3 m_offset;
    float m_openTime;
    glm::vec3 m_closedPos{ 0.0f };
    float m_t = 0.0f; // 0 closed .. 1 open
    bool m_open = false;
};

} // namespace game
