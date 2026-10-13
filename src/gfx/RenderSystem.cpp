#include "gfx/RenderSystem.h"
#include "ecs/Components.h"
#include <algorithm>

namespace gfx
{

    RenderSystem::RenderSystem(MeshManager &meshes) : m_meshes(meshes) {}

    void RenderSystem::Render(entt::registry &registry, const glm::mat4 &viewProj, const glm::vec3 &cameraPos)
    {
        m_items.clear();
        m_commands.clear();
        m_models.clear();
        m_batches.clear();

        registry.view<Transform, MeshRenderer>().each([&](const Transform &t, const MeshRenderer &r)
                                                      {
        if (r.material) m_items.push_back({ r.material.get(), r.mesh, t.Matrix() }); });
        if (m_items.empty())
        {
            m_drawCalls = 0;
            return;
        }

        std::sort(m_items.begin(), m_items.end(), [](const Item &a, const Item &b)
                  {
        if (a.material->Id() != b.material->Id()) return a.material->Id() < b.material->Id();
        return a.mesh < b.mesh; });

        // Build commands: one per (material, mesh) run; instances read models[baseInstance + gl_InstanceID].
        for (size_t i = 0; i < m_items.size();)
        {
            const Material *mat = m_items[i].material;
            Batch batch{mat, static_cast<uint32_t>(m_commands.size()), 0};
            while (i < m_items.size() && m_items[i].material == mat)
            {
                const MeshHandle mesh = m_items[i].mesh;
                const uint32_t baseInstance = static_cast<uint32_t>(m_models.size());
                while (i < m_items.size() && m_items[i].material == mat && m_items[i].mesh == mesh)
                {
                    m_models.push_back(m_items[i].model);
                    ++i;
                }
                const Mesh &m = m_meshes.Get(mesh);
                m_commands.push_back({m.indexCount, static_cast<uint32_t>(m_models.size()) - baseInstance,
                                      m.firstIndex, m.baseVertex, baseInstance});
                ++batch.commandCount;
            }
            m_batches.push_back(batch);
        }

        m_indirectBuffer.Upload(m_commands.data(), m_commands.size() * sizeof(DrawElementsIndirectCommand));
        m_modelBuffer.Upload(m_models.data(), m_models.size() * sizeof(glm::mat4));

        m_meshes.Bind();
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectBuffer.Id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_modelBuffer.Id());

        for (const Batch &b : m_batches)
        {
            b.material->Bind();
            const auto &shader = b.material->GetShader();
            shader->SetMat4("uViewProj", viewProj);
            shader->SetVec3("uCameraPos", cameraPos);
            glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                        reinterpret_cast<const void *>(static_cast<uintptr_t>(b.firstCommand) * sizeof(DrawElementsIndirectCommand)),
                                        static_cast<GLsizei>(b.commandCount), 0);
        }
        m_drawCalls = m_batches.size();
    }

} // namespace gfx
