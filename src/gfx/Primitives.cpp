#include "gfx/Primitives.h"
#include <cmath>
#include <glm/gtc/constants.hpp>

namespace gfx::primitives
{

    MeshData Plane(float h, float uvScale)
    {
        MeshData m;
        const glm::vec3 n{0, 1, 0};
        const glm::vec4 t{1, 0, 0, 1};
        m.vertices = {
            {{-h, 0, -h}, n, {0, 0}, t},
            {{-h, 0, h}, n, {0, uvScale}, t},
            {{h, 0, h}, n, {uvScale, uvScale}, t},
            {{h, 0, -h}, n, {uvScale, 0}, t},
        };
        m.indices = {0, 1, 2, 0, 2, 3};
        return m;
    }

    MeshData Cube(float h)
    {
        MeshData m;
        struct Face
        {
            glm::vec3 n, u, v;
        };
        // u x v == n gives CCW winding seen from outside.
        const Face faces[6] = {
            {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},
            {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
            {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},
            {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
            {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},
            {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
        };
        for (const auto &f : faces)
        {
            uint32_t base = static_cast<uint32_t>(m.vertices.size());
            const glm::vec4 t{f.u, 1.0f};
            const glm::vec3 c = f.n * h;
            m.vertices.push_back({c - f.u * h - f.v * h, f.n, {0, 0}, t});
            m.vertices.push_back({c + f.u * h - f.v * h, f.n, {1, 0}, t});
            m.vertices.push_back({c + f.u * h + f.v * h, f.n, {1, 1}, t});
            m.vertices.push_back({c - f.u * h + f.v * h, f.n, {0, 1}, t});
            for (uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u})
                m.indices.push_back(base + i);
        }
        return m;
    }

    MeshData Sphere(float r, int segments, int rings)
    {
        MeshData m;
        for (int i = 0; i <= rings; ++i)
        {
            float theta = glm::pi<float>() * static_cast<float>(i) / static_cast<float>(rings);
            for (int j = 0; j <= segments; ++j)
            {
                float phi = glm::two_pi<float>() * static_cast<float>(j) / static_cast<float>(segments);
                glm::vec3 n{std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)};
                glm::vec4 t{-std::sin(phi), 0.0f, std::cos(phi), 1.0f};
                m.vertices.push_back({n * r, n, {static_cast<float>(j) / static_cast<float>(segments), static_cast<float>(i) / static_cast<float>(rings)}, t});
            }
        }
        const uint32_t stride = static_cast<uint32_t>(segments + 1);
        for (int i = 0; i < rings; ++i)
        {
            for (int j = 0; j < segments; ++j)
            {
                uint32_t a = static_cast<uint32_t>(i) * stride + static_cast<uint32_t>(j);
                uint32_t b = a + stride;
                m.indices.insert(m.indices.end(), {a, a + 1, b, a + 1, b + 1, b});
            }
        }
        return m;
    }

} // namespace gfx::primitives
