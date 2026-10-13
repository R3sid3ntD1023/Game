#pragma once
#include "gfx/Mesh.h"
#include "gfx/VertexArray.h"
#include <vector>

namespace gfx {

// Owns one shared vertex buffer, index buffer and VAO for all meshes (required for multi-draw-indirect).
class MeshManager {
public:
    MeshManager();

    MeshHandle Create(const MeshData& data);
    const Mesh& Get(MeshHandle h) const { return m_meshes[h]; }
    void Bind() const { m_vao.Bind(); }

private:
    VertexBuffer m_vbo;
    IndexBuffer m_ibo;
    VertexArray m_vao;
    std::vector<Mesh> m_meshes;
    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;
};

} // namespace gfx
