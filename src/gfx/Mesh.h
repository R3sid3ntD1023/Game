#pragma once
#include <cstdint>
#include <glm/glm.hpp>
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

    struct MeshData
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    // Range inside the MeshManager's shared vertex/index buffers.
    struct Mesh
    {
        uint32_t baseVertex = 0;
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
    };

    using MeshHandle = uint32_t;

} // namespace gfx
