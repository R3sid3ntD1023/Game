#pragma once
#include "ecs/Components.h"
#include "eng/Component.h"
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>

namespace eng
{

	class World;

	// Owns an ECS entity (with a Transform) and a list of Components.
	class GameObject
	{
	public:
		GameObject(World &world, entt::entity entity, std::string name);
		~GameObject();
		GameObject(const GameObject &) = delete;
		GameObject &operator=(const GameObject &) = delete;

		template <class T, class... Args>
		T &AddComponent(Args &&...args)
		{
			auto comp = std::make_unique<T>(std::forward<Args>(args)...);
			T &ref = *comp;
			comp->gameObject = this;
			m_components.push_back(std::move(comp));
			ref.OnAttach();
			return ref;
		}

		template <class T>
		T *GetComponent()
		{
			// if (auto *comp = GetRegistry().try_get<T>(m_entity))
			// 	return comp;

			for (auto &c : m_components)
				if (auto *p = dynamic_cast<T *>(c.get()))
					return p;

			return nullptr;
		}

		void RemoveComponent(eng::Component *component);

		template <class T>
		void RemoveComponentIfExists()
		{
			for (auto it = m_components.begin(); it != m_components.end(); ++it)
			{
				if (dynamic_cast<T *>(it->get()))
				{
					m_components.erase(it);
					return;
				}
			}
		}

		Transform &GetTransform();
		World &GetWorld() { return *m_world; }
		entt::entity Entity() const { return m_entity; }

		void Destroy() { m_alive = false; }
		bool IsAlive() const { return m_alive; }

		void Update(float dt);
		void FixedUpdate(float dt);
		void Shutdown();

		std::string name;

	private:
		entt::registry &GetRegistry();

	private:
		World *m_world;
		entt::entity m_entity;
		std::vector<std::unique_ptr<Component>> m_components;
		bool m_alive = true;
		bool m_shutdown = false;
	};

} // namespace eng
