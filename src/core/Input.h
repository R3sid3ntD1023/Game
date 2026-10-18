#pragma once
#include <array>
#include <glm/vec2.hpp>

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
		virtual void OnMouseScroll(float /*xOffset*/, float /*yOffset*/) {}
	};

	class Input
	{
	public:
		void BeginFrame();

		void SetReceiver(IInputReceiver *receiver) { m_receiver = receiver; }

		bool IsKeyDown(int key) const { return key >= 0 && key < static_cast<int>(m_keys.size()) && m_keys[key]; }
		bool IsMouseDown(int button) const { return button >= 0 && button < static_cast<int>(m_buttons.size()) && m_buttons[button]; }

		glm::vec2 CursorPosition() const { return glm::vec2(m_lastX, m_lastY); }
		glm::vec2 MouseDelta() const { return glm::vec2(m_dx, m_dy); }
		float GetMouseScroll() const { return m_scroll; }

		// Called by Window from GLFW callbacks.
		void HandleKey(int key, int action);
		void HandleMouseMove(double x, double y);
		void HandleMouseButton(int button, int action);
		void HandleMouseScroll(float xOffset, float yOffset);
		void ResetMouse() { m_firstMouse = true; }

	private:
		IInputReceiver *m_receiver = nullptr;
		std::array<bool, 512> m_keys{};
		std::array<bool, 8> m_buttons{};
		double m_lastX = 0.0, m_lastY = 0.0;
		double m_dx = 0.0, m_dy = 0.0;
		bool m_firstMouse = true;
		float m_scroll = 0.0f;
	};

} // namespace core
