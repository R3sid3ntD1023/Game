#pragma once
#include "eng/GameObject.h"
#include "eng/Physics.h"
#include "gfx/Camera.h"
#include "gfx/EnvironmentMap.h"
#include "gfx/MeshManager.h"
#include "gfx/RenderSystem.h"
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>

namespace eng
{

    // Owns game objects, the ECS registry, physics and the batched renderer.
    class World
    {
    public:
        World();
        ~World();
        World(const World &) = delete;
        World &operator=(const World &) = delete;

        GameObject &CreateGameObject(const std::string &name, const glm::vec3 &position = glm::vec3(0.0f));

        // Variable-rate component updates, then fixed-rate component updates + physics, then deferred destruction.
        void Update(float dt);
        // Draws into the currently bound framebuffer using `camera`.
        void Render(float aspect);

        entt::registry &Registry() { return m_registry; }
        PhysicsWorld &Physics() { return m_physics; }
        gfx::MeshManager &Meshes() { return m_meshes; }
        size_t DrawCalls() const { return m_renderer.LastDrawCalls(); }
        size_t ObjectCount() const { return m_objects.size(); }
        const std::vector<std::unique_ptr<GameObject>> &Objects() const { return m_objects; }
        GameObject *Find(const std::string &name) const; // first live object with this name

        gfx::Camera camera;
        std::shared_ptr<gfx::EnvironmentMap> environment; // optional; enables IBL and the sky

    private:
        static constexpr float kFixedStep = 1.0f / 60.0f;

        entt::registry m_registry;
        gfx::MeshManager m_meshes;
        gfx::RenderSystem m_renderer;
        PhysicsWorld m_physics;
        std::vector<std::unique_ptr<GameObject>> m_objects;
        float m_accumulator = 0.0f;
    };

} // namespace eng
