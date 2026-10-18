#include "core/Input.h"

namespace core
{

	// GLFW_REPEAT; avoids pulling GLFW into this header.
	constexpr int kActionRepeat = 2;

	void Input::BeginFrame()
	{
		m_dx = 0.0;
		m_dy = 0.0;
		m_scroll = 0.0f;
	}

	void Input::HandleKey(int key, int action)
	{
		if (key < 0 || key >= static_cast<int>(m_keys.size()) || action == kActionRepeat)
			return;

		m_keys[key] = action != 0;
		if (m_receiver)
			m_receiver->OnKey(key, action);
	}

	void Input::HandleMouseMove(double x, double y)
	{
		if (m_firstMouse)
		{
			m_lastX = x;
			m_lastY = y;
			m_scroll = 0.0;
			m_firstMouse = false;
		}
		const float dx = static_cast<float>(x - m_lastX);
		const float dy = static_cast<float>(y - m_lastY);
		m_lastX = x;
		m_lastY = y;
		m_dx = dx;
		m_dy = dy;
		if (m_receiver)
			m_receiver->OnMouseMove(dx, dy);
	}

	void Input::HandleMouseButton(int button, int action)
	{
		if (button < 0 || button >= static_cast<int>(m_buttons.size()))
			return;
		m_buttons[button] = action != 0;
		if (m_receiver)
			m_receiver->OnMouseButton(button, action);
	}

	void Input::HandleMouseScroll(float xOffset, float yOffset)
	{
		m_scroll = yOffset;
		if (m_receiver)
			m_receiver->OnMouseScroll(xOffset, yOffset);
	}

} // namespace core
