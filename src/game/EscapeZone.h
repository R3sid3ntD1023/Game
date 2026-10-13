#pragma once
#include "eng/GameObject.h"
#include "eng/Script.h"
#include "game/GameState.h"

namespace game {

// Marks the player as escaped when they reach the extraction point.
class EscapeZone : public eng::Script {
public:
    EscapeZone(eng::GameObject& player, GameState& state, float radius) : m_player(player), m_state(state), m_radius(radius) {}

    void OnUpdate(float dt) override;

private:
    eng::GameObject& m_player;
    GameState& m_state;
    float m_radius;
    float m_time = 0.0f;
};

} // namespace game
