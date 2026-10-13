#include "eng/World.h"
#include <algorithm>

namespace eng
{

    World::World() : m_renderer(m_meshes) {}

    World::~World()
    {
        for (auto &o : m_objects)
            o->Shutdown();
        m_objects.clear();
    }

    GameObject &World::CreateGameObject(const std::string &name, const glm::vec3 &position)
    {
        const entt::entity e = m_registry.create();
        m_registry.emplace<Transform>(e, Transform{position});
        m_objects.push_back(std::make_unique<GameObject>(*this, e, name));
        return *m_objects.back();
    }

    GameObject *World::Find(const std::string &name) const
    {
        for (const auto &o : m_objects)
            if (o->IsAlive() && o->name == name)
                return o.get();
        return nullptr;
    }

    void World::Update(float dt)
    {
        for (size_t i = 0; i < m_objects.size(); ++i)
            m_objects[i]->Update(dt);

        m_accumulator = std::min(m_accumulator + dt, 0.25f);
        while (m_accumulator >= kFixedStep)
        {
            for (size_t i = 0; i < m_objects.size(); ++i)
                m_objects[i]->FixedUpdate(kFixedStep);
            m_physics.Step(kFixedStep);
            m_accumulator -= kFixedStep;
        }

        for (auto &o : m_objects)
        {
            if (o->IsAlive())
                continue;
            o->Shutdown();
            m_registry.destroy(o->Entity());
        }
        m_objects.erase(std::remove_if(m_objects.begin(), m_objects.end(), [](const auto &o)
                                       { return !o->IsAlive(); }),
                        m_objects.end());
    }

    void World::Render(float aspect)
    {
        const glm::mat4 view = camera.View();
        const glm::mat4 proj = camera.Projection(aspect);
        if (environment)
            environment->BindForLighting();
        m_renderer.Render(m_registry, proj * view, camera.position);
        if (environment)
            environment->DrawSky(view, proj, camera.position);
    }

} // namespace eng
