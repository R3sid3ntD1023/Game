#pragma once
#include <functional>
#include <glm/glm.hpp>
#include <vector>

namespace eng
{

    class RigidBody;

    struct RayHit
    {
        RigidBody *body = nullptr; // null when the ray hit terrain
        bool terrain = false;
        float distance = 0.0f;
        glm::vec3 point{0.0f};
    };

    // Gravity, a heightfield ground, and AABB collisions of dynamic bodies against static ones.
    class PhysicsWorld
    {
    public:
        using HeightFn = std::function<float(float, float)>;

        void SetHeightField(HeightFn fn) { m_height = std::move(fn); }
        void SetBoundsRadius(float radius) { m_boundsRadius = radius; }
        void SetGravity(float g) { m_gravity = g; }

        void Add(RigidBody *body);
        void Remove(RigidBody *body);
        void Step(float dt);

        bool Raycast(const glm::vec3 &origin, const glm::vec3 &dir, float maxDistance, const RigidBody *ignore, RayHit &hit) const;

    private:
        std::vector<RigidBody *> m_bodies;
        HeightFn m_height;
        float m_boundsRadius = 0.0f;
        float m_gravity = 25.0f;
    };

} // namespace eng
