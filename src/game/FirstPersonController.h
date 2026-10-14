#pragma once
#include "eng/BuiltinComponents.h"
#include "eng/RigidBody.h"
#include "eng/Script.h"
#include "game/GameState.h"

namespace game
{

    // First-person movement, look and shooting. Driven by input forwarded to its ScriptComponent.
    class FirstPersonController : public eng::Script
    {
    public:
        void OnStart() override;
        void OnUpdate(float dt) override;
        void OnFixedUpdate(float dt) override;
        void OnKey(int key, int action) override;
        void OnMouseMove(float dx, float dy) override;
        void OnMouseButton(int button, int action) override;
        void OnMouseScroll(float offset) override;

        void SetLook(float yawDegrees, float pitchDegrees)
        {
            m_yaw = yawDegrees;
            m_pitch = pitchDegrees;
        }

        float walkSpeed = 6.0f;
        float sprintSpeed = 10.0f;
        float jumpSpeed = 9.0f;
        float mouseSensitivity = 0.1f;

    private:
        void Shoot();
        void UpdateFootsteps(float dt);

        eng::RigidBody *m_body = nullptr;
        float m_yaw = 0.0f; // degrees; 0 looks down +X
        float m_pitch = 0.0f;
        bool m_forward = false, m_back = false, m_left = false, m_right = false, m_sprint = false, m_jump = false;
        bool m_fireHeld = false, m_firePressed = false, m_reloadPressed = false;
        float m_stepDistance = 0.0f;
        int m_lastStep = -1;
    };

} // namespace game
