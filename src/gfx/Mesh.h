#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <limits>
#include <vector>

namespace gfx
{

    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 tangent; // xyz + handedness
        glm::ivec4 boneIds{0};
        glm::vec4 boneWeights{0.0f}; // all zero = not skinned
    };

    // Axis-aligned bounding box in model space.
    struct Bounds
    {
        glm::vec3 min{std::numeric_limits<float>::max()};
        glm::vec3 max{std::numeric_limits<float>::lowest()};

        bool Valid() const { return min.x <= max.x; }
        void Expand(const glm::vec3 &p)
        {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }
        void Expand(const Bounds &b)
        {
            if (b.Valid())
            {
                Expand(b.min);
                Expand(b.max);
            }
        }
        glm::vec3 Center() const { return (min + max) * 0.5f; }
        glm::vec3 HalfExtents() const { return Valid() ? (max - min) * 0.5f : glm::vec3(0.0f); }
        // Bounds after a non-uniform scale (positive components assumed).
        Bounds Scaled(const glm::vec3 &s) const { return {min * s, max * s}; }
    };

    struct MeshData
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        Bounds ComputeBounds() const
        {
            Bounds b;
            for (const Vertex &v : vertices)
                b.Expand(v.position);
            return b;
        }
    };

    // Range inside the MeshManager's shared vertex/index buffers.
    struct Mesh
    {
        uint32_t baseVertex = 0;
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        Bounds bounds;
    };

    using MeshHandle = uint32_t;

} // namespace gfx
