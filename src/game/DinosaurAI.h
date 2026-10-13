#pragma once
#include "eng/BuiltinComponents.h"
#include "eng/GameObject.h"
#include "eng/RigidBody.h"
#include "eng/Script.h"
#include "game/GameState.h"
#include <glm/glm.hpp>

namespace game {

// Placeholder dinosaur brain: wander, chase the player when close, bite when in range.
class DinosaurAI : public eng::Script {
public:
    DinosaurAI(eng::GameObject& target, GameState& state, float speed, float damage)
        : m_target(target), m_state(state), m_speed(speed), m_damage(damage) {}

    void OnStart() override;
    void OnUpdate(float dt) override;
    void OnFixedUpdate(float dt) override;

private:
    eng::GameObject& m_target;
    GameState& m_state;
    eng::RigidBody* m_body = nullptr;
    eng::Health* m_health = nullptr;
    eng::Health* m_targetHealth = nullptr;
    float m_speed;
    float m_damage;
    float m_attackTimer = 0.0f;
    float m_wanderTimer = 0.0f;
    glm::vec3 m_wanderDir{ 0.0f };
    float m_aggroRange = 35.0f;
    float m_attackRange = 3.2f;
};

} // namespace game
