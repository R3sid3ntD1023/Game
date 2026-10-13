#include "gfx/MeshManager.h"
#include <cstddef>

namespace gfx
{

    MeshManager::MeshManager()
    {
        m_vbo.Create(1 << 20);
        m_ibo.Create(1 << 18);
        m_vao.SetVertexBuffer(m_vbo, sizeof(Vertex),
                              {{0, 3, static_cast<GLuint>(offsetof(Vertex, position))},
                               {1, 3, static_cast<GLuint>(offsetof(Vertex, normal))},
                               {2, 2, static_cast<GLuint>(offsetof(Vertex, uv))},
                               {3, 4, static_cast<GLuint>(offsetof(Vertex, tangent))},
                               {4, 4, static_cast<GLuint>(offsetof(Vertex, boneIds)), true},
                               {5, 4, static_cast<GLuint>(offsetof(Vertex, boneWeights))}});
        m_vao.SetIndexBuffer(m_ibo);
    }

    MeshHandle MeshManager::Create(const MeshData &data)
    {
        Mesh mesh;
        mesh.baseVertex = m_vertexCount;
        mesh.firstIndex = m_indexCount;
        mesh.indexCount = static_cast<uint32_t>(data.indices.size());

        m_vbo.Append(data.vertices.data(), data.vertices.size() * sizeof(Vertex));
        m_ibo.Append(data.indices.data(), data.indices.size() * sizeof(uint32_t));
        m_vertexCount += static_cast<uint32_t>(data.vertices.size());
        m_indexCount += mesh.indexCount;

        // Buffers may have been reallocated; re-attach to the VAO.
        m_vao.SetVertexBuffer(m_vbo, sizeof(Vertex), {});
        m_vao.SetIndexBuffer(m_ibo);

        m_meshes.push_back(mesh);
        return static_cast<MeshHandle>(m_meshes.size() - 1);
    }

} // namespace gfx
