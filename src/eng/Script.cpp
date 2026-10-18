#include "eng/Script.h"

namespace eng
{

	void ScriptComponent::OnUpdate(float dt)
	{
		for (size_t i = 0; i < m_scripts.size(); ++i)
		{
			Script &s = *m_scripts[i];
			if (!s.started && s.enabled)
			{
				s.started = true;
				s.OnStart();
			}
			s.OnUpdate(dt);
		}
	}

	void ScriptComponent::OnFixedUpdate(float dt)
	{
		for (auto &s : m_scripts)
			if (s->started && s->enabled)
				s->OnFixedUpdate(dt);
	}

	void ScriptComponent::OnKey(int key, int action)
	{
		for (auto &s : m_scripts)
			if (s->enabled)
				s->OnKey(key, action);
	}

	void ScriptComponent::OnMouseMove(float dx, float dy)
	{
		for (auto &s : m_scripts)
			if (s->enabled)
				s->OnMouseMove(dx, dy);
	}

	void ScriptComponent::OnMouseButton(int button, int action)
	{
		for (auto &s : m_scripts)
			if (s->enabled)
				s->OnMouseButton(button, action);
	}

	void ScriptComponent::OnMouseScroll(float xOffset, float yOffset)
	{
		for (auto &s : m_scripts)
		{
			if (s->enabled)
				s->OnMouseScroll(xOffset, yOffset);
		}
	}

} // namespace eng
