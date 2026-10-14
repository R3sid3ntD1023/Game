#include "game/TreeGen.h"
#include "gfx/Primitives.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <glm/gtc/constants.hpp>
#include <random>
#include <vector>

namespace game
{

    namespace
    {

        using glm::vec3;

        struct Rng
        {
            std::mt19937 g;
            explicit Rng(uint32_t seed) : g(seed) {}
            float U(float a, float b) { return std::uniform_real_distribution<float>(a, b)(g); }
            int I(int a, int b) { return std::uniform_int_distribution<int>(a, b)(g); }
        };

        // Lofts a closed tube along `pts` (radius per point) using parallel-transported frames.
        void AddTube(gfx::MeshData &m, const std::vector<vec3> &pts, const std::vector<float> &radii, int sides)
        {
            const uint32_t base = static_cast<uint32_t>(m.vertices.size());
            const size_t n = pts.size();
            vec3 prevUp(0.0f);
            float along = 0.0f;

            for (size_t i = 0; i < n; ++i)
            {
                const vec3 a = i > 0 ? pts[i - 1] : pts[i];
                const vec3 b = i + 1 < n ? pts[i + 1] : pts[i];
                const vec3 dir = glm::normalize(b - a);
                if (i > 0)
                    along += glm::length(pts[i] - pts[i - 1]);

                const vec3 ref = i == 0 ? (std::abs(dir.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0)) : prevUp;
                vec3 right = glm::cross(dir, ref);
                if (glm::dot(right, right) < 1e-6f)
                    right = glm::cross(dir, vec3(1, 0, 0));
                right = glm::normalize(right);
                const vec3 up = glm::normalize(glm::cross(right, dir));
                prevUp = up;

                for (int s = 0; s <= sides; ++s)
                {
                    const float u = static_cast<float>(s) / static_cast<float>(sides);
                    const float ang = u * glm::two_pi<float>();
                    const vec3 normal = std::cos(ang) * right + std::sin(ang) * up;
                    const vec3 tangent = -std::sin(ang) * right + std::cos(ang) * up;
                    m.vertices.push_back({pts[i] + normal * radii[i], normal, {u * 2.0f, along * 0.25f}, glm::vec4(tangent, 1.0f)});
                }
            }

            const uint32_t ring = static_cast<uint32_t>(sides + 1);
            for (size_t i = 0; i + 1 < n; ++i)
            {
                for (int s = 0; s < sides; ++s)
                {
                    const uint32_t v00 = base + static_cast<uint32_t>(i) * ring + static_cast<uint32_t>(s);
                    const uint32_t v01 = v00 + 1, v10 = v00 + ring, v11 = v10 + 1;
                    m.indices.insert(m.indices.end(), {v00, v10, v11, v00, v11, v01});
                }
            }
        }

        // Lumpy flattened ellipsoid used as a clump of leaves.
        void AddBlob(gfx::MeshData &m, Rng &rng, const vec3 &center, float radius)
        {
            const gfx::MeshData sphere = gfx::primitives::Sphere(1.0f, 10, 6);
            const uint32_t base = static_cast<uint32_t>(m.vertices.size());
            const vec3 scale(radius, radius * 0.55f, radius);
            const float p1 = rng.U(0.0f, 6.28f), p2 = rng.U(0.0f, 6.28f), p3 = rng.U(0.0f, 6.28f);

            for (const gfx::Vertex &sv : sphere.vertices)
            {
                const vec3 d = sv.normal;
                const float lump = 1.0f + 0.25f * std::sin(d.x * 5.0f + p1) * std::sin(d.y * 4.0f + p2) * std::sin(d.z * 6.0f + p3);
                gfx::Vertex v = sv;
                v.position = center + d * scale * lump;
                v.normal = glm::normalize(d / scale);
                m.vertices.push_back(v);
            }
            for (uint32_t i : sphere.indices)
                m.indices.push_back(base + i);
        }

        vec3 PointAt(const std::vector<vec3> &pts, float y)
        {
            for (size_t i = 0; i + 1 < pts.size(); ++i)
                if (y <= pts[i + 1].y)
                {
                    const float t = (y - pts[i].y) / std::max(pts[i + 1].y - pts[i].y, 1e-4f);
                    return glm::mix(pts[i], pts[i + 1], glm::clamp(t, 0.0f, 1.0f));
                }
            return pts.back();
        }

    } // namespace

    TreeModel GenerateRainforestTree(uint32_t seed)
    {
        Rng rng(seed * 7919u + 13u);
        TreeModel out;
        const float H = rng.U(26.0f, 36.0f);
        out.height = H;

        // Trunk: gentle lean and wobble, strong flare at the base.
        std::vector<vec3> trunk;
        std::vector<float> radii;
        const int segs = 14;
        const float leanAngle = rng.U(0.0f, glm::two_pi<float>());
        const float lean = rng.U(0.5f, 2.5f);
        const float phase = rng.U(0.0f, 6.28f);
        const float r0 = rng.U(0.5f, 0.75f);
        for (int i = 0; i <= segs; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(segs);
            const vec3 off = vec3(std::cos(leanAngle), 0, std::sin(leanAngle)) * lean * t * t +
                             vec3(std::sin(t * 5.0f + phase), 0, std::cos(t * 4.0f + phase)) * 0.35f * t;
            trunk.push_back({off.x, H * t, off.z});
            radii.push_back(r0 * (1.0f - 0.6f * t) * (1.0f + 1.1f * std::exp(-t * 14.0f)));
        }
        AddTube(out.trunk, trunk, radii, 9);

        // Buttress roots fanning out from the lower trunk to the ground.
        const int roots = rng.I(4, 6);
        for (int k = 0; k < roots; ++k)
        {
            const float a = glm::two_pi<float>() * static_cast<float>(k) / static_cast<float>(roots) + rng.U(-0.3f, 0.3f);
            const vec3 dir(std::cos(a), 0.0f, std::sin(a));
            const float startY = rng.U(2.5f, 4.5f);
            const float reach = rng.U(1.8f, 3.0f);
            const vec3 origin = PointAt(trunk, startY);
            std::vector<vec3> pts;
            std::vector<float> rad;
            for (int j = 0; j <= 4; ++j)
            {
                const float u = static_cast<float>(j) / 4.0f;
                pts.push_back(vec3(origin.x, 0.0f, origin.z) + dir * (0.3f + reach * u) + vec3(0.0f, startY * std::pow(1.0f - u, 1.5f), 0.0f));
                rad.push_back(0.32f * (1.0f - 0.65f * u));
            }
            AddTube(out.trunk, pts, rad, 5);
        }

        // Forking branches; leaf clumps sit at the tips.
        std::vector<vec3> leafCenters;
        std::vector<float> leafSizes;
        std::function<void(vec3, vec3, float, float, int)> branch = [&](vec3 p, vec3 d, float len, float r, int depth)
        {
            std::vector<vec3> pts{p};
            std::vector<float> rad{r};
            const int steps = 5;
            for (int s = 1; s <= steps; ++s)
            {
                d = glm::normalize(d + vec3(0.0f, 0.12f, 0.0f) + vec3(rng.U(-0.15f, 0.15f), rng.U(-0.05f, 0.1f), rng.U(-0.15f, 0.15f)));
                p += d * (len / static_cast<float>(steps));
                pts.push_back(p);
                rad.push_back(r * (1.0f - 0.7f * static_cast<float>(s) / static_cast<float>(steps)));
            }
            AddTube(out.trunk, pts, rad, 5);

            if (depth > 0)
            {
                const int subs = rng.I(2, 3);
                for (int k = 0; k < subs; ++k)
                {
                    const int idx = rng.I(2, 4);
                    const vec3 sd = glm::normalize(d + vec3(rng.U(-0.8f, 0.8f), rng.U(0.0f, 0.4f), rng.U(-0.8f, 0.8f)));
                    branch(pts[idx], sd, len * rng.U(0.35f, 0.55f), rad[idx] * 0.7f, depth - 1);
                }
            }
            else
            {
                leafCenters.push_back(p);
                leafSizes.push_back(rng.U(2.4f, 3.8f));
            }
        };

        const int mainBranches = rng.I(7, 10);
        for (int i = 0; i < mainBranches; ++i)
        {
            const float t = rng.U(0.5f, 0.92f);
            const float a = rng.U(0.0f, glm::two_pi<float>());
            const vec3 dir = glm::normalize(vec3(std::cos(a), rng.U(0.25f, 0.7f), std::sin(a)));
            branch(PointAt(trunk, H * t), dir, rng.U(5.0f, 10.0f) * (1.15f - t * 0.5f), 0.28f, 1);
        }
        for (int i = 0; i < 3; ++i)
        { // crown
            leafCenters.push_back(trunk.back() + vec3(rng.U(-2.0f, 2.0f), rng.U(-1.5f, 1.0f), rng.U(-2.0f, 2.0f)));
            leafSizes.push_back(rng.U(3.8f, 5.0f));
        }
        for (size_t i = 0; i < leafCenters.size(); ++i)
            AddBlob(out.canopy, rng, leafCenters[i], leafSizes[i]);

        for (gfx::Vertex &v : out.trunk.vertices)
            v.position.y -= H * 0.5f;
        for (gfx::Vertex &v : out.canopy.vertices)
            v.position.y -= H * 0.5f;
        return out;
    }

    gfx::MeshData GenerateFern(uint32_t seed)
    {
        Rng rng(seed * 104729u + 7u);
        gfx::MeshData m;
        const int fronds = rng.I(7, 11);
        const int pointsPerFrond = 7;

        for (int k = 0; k < fronds; ++k)
        {
            const float a = glm::two_pi<float>() * static_cast<float>(k) / static_cast<float>(fronds) + rng.U(-0.25f, 0.25f);
            const float len = rng.U(0.9f, 1.4f);
            const vec3 out(std::cos(a), 0.0f, std::sin(a));
            const vec3 side(-std::sin(a), 0.0f, std::cos(a));

            std::vector<vec3> center;
            std::vector<float> width;
            for (int j = 0; j < pointsPerFrond; ++j)
            {
                const float t = static_cast<float>(j) / static_cast<float>(pointsPerFrond - 1);
                center.push_back(out * (len * t) + vec3(0.0f, 0.12f + len * (1.1f * t - 1.0f * t * t), 0.0f));
                width.push_back(0.13f * (1.0f - t * 0.9f) * (0.4f + std::sin(t * glm::pi<float>())));
            }

            const uint32_t base = static_cast<uint32_t>(m.vertices.size());
            for (int j = 0; j < pointsPerFrond; ++j)
            {
                const vec3 tangent = glm::normalize(center[std::min(j + 1, pointsPerFrond - 1)] - center[std::max(j - 1, 0)]);
                const vec3 normal = glm::normalize(glm::cross(side, tangent));
                const float v = static_cast<float>(j) / static_cast<float>(pointsPerFrond - 1);
                const glm::vec4 tan4(side, 1.0f);
                // top face then bottom face, so the fronds are lit from both sides
                m.vertices.push_back({center[j] - side * width[j], normal, {0.0f, v}, tan4});
                m.vertices.push_back({center[j] + side * width[j], normal, {1.0f, v}, tan4});
                m.vertices.push_back({center[j] - side * width[j], -normal, {0.0f, v}, tan4});
                m.vertices.push_back({center[j] + side * width[j], -normal, {1.0f, v}, tan4});
            }
            for (int j = 0; j + 1 < pointsPerFrond; ++j)
            {
                const uint32_t l0 = base + static_cast<uint32_t>(j) * 4, r0v = l0 + 1, bl0 = l0 + 2, br0 = l0 + 3;
                const uint32_t l1 = l0 + 4, r1 = l1 + 1, bl1 = l1 + 2, br1 = l1 + 3;
                m.indices.insert(m.indices.end(), {l0, r0v, r1, l0, r1, l1});      // up-facing
                m.indices.insert(m.indices.end(), {bl0, br1, br0, bl0, bl1, br1}); // down-facing
            }
        }
        return m;
    }

} // namespace game
