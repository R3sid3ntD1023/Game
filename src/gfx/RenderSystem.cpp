#include "gfx/RenderSystem.h"
#include "ecs/Components.h"
#include <algorithm>

namespace gfx
{

    RenderSystem::RenderSystem(MeshManager &meshes)
        : m_meshes(meshes), m_depthShader(SHADER_DIR "depth.vert", SHADER_DIR "depth.frag") {}

    void RenderSystem::Build(entt::registry &registry)
    {
        m_items.clear();
        m_commands.clear();
        m_instances.clear();
        m_bones.clear();
        m_batches.clear();

        registry.view<Transform, MeshRenderer>().each([&](entt::entity e, const Transform &t, const MeshRenderer &r)
                                                      {
            if (!r.material)
                return;
            uint32_t boneOffset = kNoSkin;
            if (const auto *pose = registry.try_get<SkinnedPose>(e); pose && !pose->matrices.empty())
            {
                boneOffset = static_cast<uint32_t>(m_bones.size());
                m_bones.insert(m_bones.end(), pose->matrices.begin(), pose->matrices.end());
            }
            const auto *emissive = registry.try_get<EmissiveScale>(e);
            m_items.push_back({r.material.get(), r.mesh, t.Matrix(), boneOffset, emissive ? emissive->value : 1.0f}); });
        if (m_items.empty())
        {
            m_drawCalls = 0;
            return;
        }

        std::sort(m_items.begin(), m_items.end(), [](const Item &a, const Item &b)
                  {
            if (a.material->Id() != b.material->Id())
                return a.material->Id() < b.material->Id();
            return a.mesh < b.mesh; });

        // One command per (material, mesh) run; instances read instances[baseInstance + gl_InstanceID].
        for (size_t i = 0; i < m_items.size();)
        {
            const Material *mat = m_items[i].material;
            Batch batch{mat, static_cast<uint32_t>(m_commands.size()), 0};
            while (i < m_items.size() && m_items[i].material == mat)
            {
                const MeshHandle mesh = m_items[i].mesh;
                const uint32_t baseInstance = static_cast<uint32_t>(m_instances.size());
                while (i < m_items.size() && m_items[i].material == mat && m_items[i].mesh == mesh)
                {
                    m_instances.push_back({m_items[i].model, m_items[i].boneOffset, m_items[i].emissiveScale, {0, 0}});
                    ++i;
                }
                const Mesh &m = m_meshes.Get(mesh);
                m_commands.push_back({m.indexCount, static_cast<uint32_t>(m_instances.size()) - baseInstance,
                                      m.firstIndex, m.baseVertex, baseInstance});
                ++batch.commandCount;
            }
            m_batches.push_back(batch);
        }

        if (m_bones.empty())
            m_bones.push_back(glm::mat4(1.0f)); // keep the SSBO non-empty

        m_indirectBuffer.Upload(m_commands.data(), m_commands.size() * sizeof(DrawElementsIndirectCommand));
        m_modelBuffer.Upload(m_instances.data(), m_instances.size() * sizeof(InstanceData));
        m_boneBuffer.Upload(m_bones.data(), m_bones.size() * sizeof(glm::mat4));
        m_drawCalls = m_batches.size();
    }

    void RenderSystem::BindBuffers() const
    {
        m_meshes.Bind();
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectBuffer.Id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_modelBuffer.Id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_boneBuffer.Id());
    }

    void RenderSystem::DepthPass(const glm::mat4 &viewProj)
    {
        if (m_commands.empty())
            return;
        BindBuffers();
        m_depthShader.Bind();
        m_depthShader.SetMat4("uViewProj", viewProj);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(m_commands.size()), 0);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    void RenderSystem::ShadePass(const glm::mat4 &viewProj, const glm::vec3 &cameraPos, int tileCountX)
    {
        if (m_commands.empty())
            return;
        BindBuffers();

        glDepthFunc(GL_LEQUAL); // the prepass already wrote depth
        glDepthMask(GL_FALSE);
        for (const Batch &b : m_batches)
        {
            b.material->Bind();
            const auto &shader = b.material->GetShader();
            shader->SetMat4("uViewProj", viewProj);
            shader->SetVec3("uCameraPos", cameraPos);
            shader->SetInt("uTileCountX", tileCountX);
            glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                        reinterpret_cast<const void *>(static_cast<uintptr_t>(b.firstCommand) * sizeof(DrawElementsIndirectCommand)),
                                        static_cast<GLsizei>(b.commandCount), 0);
        }
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

} // namespace gfx
