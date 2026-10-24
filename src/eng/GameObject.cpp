#include "eng/GameObject.h"
#include "eng/World.h"

namespace eng
{

	GameObject::GameObject(World &world, entt::entity entity, std::string objName)
		: name(std::move(objName)),
		  m_world(&world),
		  m_entity(entity)
	{
	}

	GameObject::~GameObject() = default;

	Transform &GameObject::GetTransform()
	{
		return m_world->Registry().get<Transform>(m_entity);
	}

	// Index loops: components may be added while iterating.
	void GameObject::Update(float dt)
	{
		for (size_t i = 0; i < m_components.size(); ++i)
		{
			Component &c = *m_components[i];
			if (!c.started)
			{
				c.started = true;
				c.OnStart();
			}
			c.OnUpdate(dt);
		}
	}

	void GameObject::FixedUpdate(float dt)
	{
		for (size_t i = 0; i < m_components.size(); ++i)
		{
			if (m_components[i]->started)
				m_components[i]->OnFixedUpdate(dt);
		}
	}

	void GameObject::Shutdown()
	{
		if (m_shutdown)
			return;
		m_shutdown = true;
		for (auto &c : m_components)
			c->OnDestroy();
	}

	void GameObject::RemoveComponent(eng::Component *component)
	{
		for (auto it = m_components.begin(); it != m_components.end(); ++it)
		{
			if (it->get() == component)
			{
				m_components.erase(it);
				return;
			}
		}
	}

	entt::registry &GameObject::GetRegistry()
	{
		return m_world->Registry();
	}

} // namespace eng
