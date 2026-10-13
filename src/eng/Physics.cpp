#include "eng/Physics.h"
#include "eng/GameObject.h"
#include "eng/RigidBody.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace eng
{

    void PhysicsWorld::Add(RigidBody *body) { m_bodies.push_back(body); }

    void PhysicsWorld::Remove(RigidBody *body)
    {
        m_bodies.erase(std::remove(m_bodies.begin(), m_bodies.end(), body), m_bodies.end());
    }

    namespace
    {

        // Pushes the dynamic body out of the static one along the axis of least penetration.
        void ResolveAabb(RigidBody &dyn, glm::vec3 &pos, const RigidBody &stat)
        {
            const glm::vec3 sp = stat.gameObject->GetTransform().position;
            const glm::vec3 d = pos - sp;
            const glm::vec3 overlap = (dyn.halfExtents + stat.halfExtents) - glm::abs(d);
            if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f)
                return;

            int axis = 0;
            if (overlap.y < overlap[axis])
                axis = 1;
            if (overlap.z < overlap[axis])
                axis = 2;

            const float sign = d[axis] < 0.0f ? -1.0f : 1.0f;
            pos[axis] += sign * overlap[axis];
            if (axis == 1 && sign > 0.0f)
                dyn.grounded = true;
            dyn.velocity[axis] = 0.0f;
        }

    } // namespace

    void PhysicsWorld::Step(float dt)
    {
        for (RigidBody *b : m_bodies)
        {
            if (b->isStatic)
                continue;
            Transform &t = b->gameObject->GetTransform();

            if (b->useGravity)
                b->velocity.y -= m_gravity * dt;
            glm::vec3 pos = t.position + b->velocity * dt;
            b->grounded = false;

            if (m_boundsRadius > 0.0f)
            {
                const float r = std::sqrt(pos.x * pos.x + pos.z * pos.z);
                if (r > m_boundsRadius)
                {
                    pos.x *= m_boundsRadius / r;
                    pos.z *= m_boundsRadius / r;
                }
            }

            for (const RigidBody *s : m_bodies)
                if (s->isStatic)
                    ResolveAabb(*b, pos, *s);

            if (m_height)
            {
                const float ground = m_height(pos.x, pos.z) + b->halfExtents.y;
                if (pos.y < ground)
                {
                    pos.y = ground;
                    if (b->velocity.y < 0.0f)
                        b->velocity.y = 0.0f;
                    b->grounded = true;
                }
            }
            t.position = pos;
        }
    }

    bool PhysicsWorld::Raycast(const glm::vec3 &origin, const glm::vec3 &dir, float maxDistance, const RigidBody *ignore, RayHit &hit) const
    {
        float best = maxDistance;
        bool found = false;
        hit = RayHit{};

        if (m_height)
        {
            constexpr float step = 0.5f;
            for (float t = 0.0f; t < best; t += step)
            {
                const glm::vec3 p = origin + dir * t;
                if (p.y < m_height(p.x, p.z))
                {
                    best = t;
                    hit.terrain = true;
                    found = true;
                    break;
                }
            }
        }

        for (RigidBody *b : m_bodies)
        {
            if (b == ignore)
                continue;
            const glm::vec3 c = b->gameObject->GetTransform().position;
            const glm::vec3 mn = c - b->halfExtents;
            const glm::vec3 mx = c + b->halfExtents;

            float tmin = 0.0f, tmax = best;
            bool ok = true;
            for (int i = 0; i < 3 && ok; ++i)
            {
                if (std::abs(dir[i]) < 1e-8f)
                {
                    ok = origin[i] >= mn[i] && origin[i] <= mx[i];
                }
                else
                {
                    float t1 = (mn[i] - origin[i]) / dir[i];
                    float t2 = (mx[i] - origin[i]) / dir[i];
                    if (t1 > t2)
                        std::swap(t1, t2);
                    tmin = std::max(tmin, t1);
                    tmax = std::min(tmax, t2);
                    ok = tmin <= tmax;
                }
            }
            if (ok && tmin < best)
            {
                best = tmin;
                hit.body = b;
                hit.terrain = false;
                found = true;
            }
        }

        if (found)
        {
            hit.distance = best;
            hit.point = origin + dir * best;
        }
        return found;
    }

} // namespace eng
