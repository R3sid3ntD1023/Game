#pragma once
#include <array>

namespace core
{

    // Receives raw input events forwarded by Input (implemented by world components).
    class IInputReceiver
    {
    public:
        virtual ~IInputReceiver() = default;
        virtual void OnKey(int /*key*/, int /*action*/) {}
        virtual void OnMouseMove(float /*dx*/, float /*dy*/) {}
        virtual void OnMouseButton(int /*button*/, int /*action*/) {}
        virtual void OnMouseScroll(float /*offset*/) {}
    };

    class Input
    {
    public:
        void SetReceiver(IInputReceiver *receiver) { m_receiver = receiver; }

        bool IsKeyDown(int key) const { return key >= 0 && key < static_cast<int>(m_keys.size()) && m_keys[key]; }
        bool IsMouseDown(int button) const { return button >= 0 && button < static_cast<int>(m_buttons.size()) && m_buttons[button]; }

        // Called by Window from GLFW callbacks.
        void HandleKey(int key, int action);
        void HandleMouseMove(double x, double y);
        void HandleMouseButton(int button, int action);
        void HandleMouseScroll(float offset);
        void ResetMouse() { m_firstMouse = true; }

    private:
        IInputReceiver *m_receiver = nullptr;
        std::array<bool, 512> m_keys{};
        std::array<bool, 8> m_buttons{};
        double m_lastX = 0.0, m_lastY = 0.0;
        bool m_firstMouse = true;
    };

} // namespace core
