#include "gfx/RenderSystem.h"
#include "ecs/Components.h"
#include <algorithm>

namespace gfx
{

    RenderSystem::RenderSystem(MeshManager &meshes)
        : m_meshes(meshes), m_depthShader(SHADER_DIR "depth.vert", SHADER_DIR "depth.frag"),
          m_depthAlphaShader(SHADER_DIR "depth.vert", SHADER_DIR "depth_alpha.frag"),
          m_overlayShader(SHADER_DIR "overlay.vert", SHADER_DIR "overlay.frag") {}

    void RenderSystem::OverlayPass(const glm::mat4 &viewProj, float alpha)
    {
        if (!m_hasOverlay || !m_overlayVisible)
            return;
        const Mesh &mesh = m_meshes.Get(m_overlayMesh);
        if (mesh.indexCount == 0)
            return;

        m_meshes.Bind();
        m_overlayShader.Bind();
        m_overlayShader.SetMat4("uViewProj", viewProj);
        m_overlayShader.SetFloat("uAlpha", alpha);
        m_overlayShader.SetInt("uMode", m_overlayMode);
        m_overlayShader.SetVec2("uOrigin", m_overlayOrigin);
        m_overlayShader.SetFloat("uCellSize", m_overlayCellSize);

        const GLboolean cull = glIsEnabled(GL_CULL_FACE);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        glDrawElementsBaseVertex(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT,
                                 reinterpret_cast<const void *>(static_cast<uintptr_t>(mesh.firstIndex) * sizeof(uint32_t)), mesh.baseVertex);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
        if (cull)
            glEnable(GL_CULL_FACE);
    }

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
            m_items.push_back({r.material.get(), r.mesh, t.Matrix(), boneOffset, emissive ? emissive->value : 1.0f, r.material->windStrength}); });
        if (m_items.empty())
        {
            m_drawCalls = 0;
            return;
        }

        std::sort(m_items.begin(), m_items.end(), [](const Item &a, const Item &b)
                  {
            // Cutout materials go last (they need per-material depth draws), then emissive ones, so shadow
            // passes can drop emissive geometry with a shorter multi-draw.
            const bool ca = a.material->IsCutout(), cb = b.material->IsCutout();
            if (ca != cb)
                return cb;
            const bool ea = a.material->IsEmissive(), eb = b.material->IsEmissive();
            if (ea != eb)
                return eb;
            // Same shader together so the pass binds each program once, then group by material.
            const GLuint sa = a.material->GetShader()->Id(), sb = b.material->GetShader()->Id();
            if (sa != sb)
                return sa < sb;
            if (a.material->Id() != b.material->Id())
                return a.material->Id() < b.material->Id();
            return a.mesh < b.mesh; });

        m_nonEmissiveCommands = 0;
        m_opaqueCommands = 0;
        bool countedNonEmissive = false, countedOpaque = false;
        // One command per (material, mesh) run; instances read instances[baseInstance + gl_InstanceID].
        for (size_t i = 0; i < m_items.size();)
        {
            const Material *mat = m_items[i].material;
            if (!countedNonEmissive && (mat->IsEmissive() || mat->IsCutout()))
            {
                m_nonEmissiveCommands = static_cast<uint32_t>(m_commands.size());
                countedNonEmissive = true;
            }
            if (!countedOpaque && mat->IsCutout())
            {
                m_opaqueCommands = static_cast<uint32_t>(m_commands.size());
                countedOpaque = true;
            }
            Batch batch{mat, static_cast<uint32_t>(m_commands.size()), 0};
            while (i < m_items.size() && m_items[i].material == mat)
            {
                const MeshHandle mesh = m_items[i].mesh;
                const uint32_t baseInstance = static_cast<uint32_t>(m_instances.size());
                while (i < m_items.size() && m_items[i].material == mat && m_items[i].mesh == mesh)
                {
                    m_instances.push_back({m_items[i].model, m_items[i].boneOffset, m_items[i].emissiveScale, m_items[i].windStrength, 0});
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

        if (!countedNonEmissive)
            m_nonEmissiveCommands = static_cast<uint32_t>(m_commands.size());
        if (!countedOpaque)
            m_opaqueCommands = static_cast<uint32_t>(m_commands.size());

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

    void RenderSystem::DepthPass(const glm::mat4 &viewProj, bool skipEmissive)
    {
        if (m_commands.empty())
            return;
        BindBuffers();
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        const size_t opaque = skipEmissive ? m_nonEmissiveCommands : m_opaqueCommands;
        if (opaque > 0)
        {
            m_depthShader.Bind();
            m_depthShader.SetMat4("uViewProj", viewProj);
            m_depthShader.SetFloat("uTime", m_time);
            m_depthShader.SetVec2("uWindDir", m_windDir);
            glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(opaque), 0);
        }

        if (m_opaqueCommands < m_commands.size())
        {
            // Cutout batches sample their albedo alpha, so each needs its own texture bound.
            const GLboolean cull = glIsEnabled(GL_CULL_FACE);
            glDisable(GL_CULL_FACE);
            m_depthAlphaShader.Bind();
            m_depthAlphaShader.SetMat4("uViewProj", viewProj);
            m_depthAlphaShader.SetFloat("uTime", m_time);
            m_depthAlphaShader.SetVec2("uWindDir", m_windDir);
            m_depthAlphaShader.SetInt("uAlbedoMap", 0);
            for (const Batch &b : m_batches)
            {
                if (!b.material->IsCutout())
                    continue;
                b.material->albedoMap->Bind(0);
                m_depthAlphaShader.SetVec4("uBaseColor", b.material->baseColor);
                m_depthAlphaShader.SetFloat("uAlphaCutoff", b.material->alphaCutoff);
                glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                            reinterpret_cast<const void *>(static_cast<uintptr_t>(b.firstCommand) * sizeof(DrawElementsIndirectCommand)),
                                            static_cast<GLsizei>(b.commandCount), 0);
            }
            if (cull)
                glEnable(GL_CULL_FACE);
        }
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    void RenderSystem::SetShoreMap(const std::vector<float> &heights, int size, const glm::vec2 &center, float extent)
    {
        if (m_shoreTex)
            glDeleteTextures(1, &m_shoreTex);
        glCreateTextures(GL_TEXTURE_2D, 1, &m_shoreTex);
        glTextureStorage2D(m_shoreTex, 1, GL_R16F, size, size);
        glTextureSubImage2D(m_shoreTex, 0, 0, 0, size, size, GL_RED, GL_FLOAT, heights.data());
        glTextureParameteri(m_shoreTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_shoreTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_shoreTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_shoreTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_shoreCenter = center;
        m_shoreExtent = extent;
    }

    void RenderSystem::ShadePass(const glm::mat4 &viewProj, const glm::vec3 &cameraPos, int tileCountX)
    {
        if (m_commands.empty())
            return;
        BindBuffers();

        glDepthFunc(GL_LEQUAL); // the prepass already wrote depth
        glDepthMask(GL_FALSE);
        if (m_shoreTex)
            glBindTextureUnit(9, m_shoreTex);
        const bool cullWasOn = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
        const Shader *bound = nullptr;
        for (const Batch &b : m_batches)
        {
            if (b.material->doubleSided)
                glDisable(GL_CULL_FACE);
            const Shader *shader = b.material->GetShader().get();
            if (shader != bound)
            {
                bound = shader;
                shader->Bind();
                shader->SetMat4("uViewProj", viewProj);
                shader->SetVec3("uCameraPos", cameraPos);
                shader->SetInt("uTileCountX", tileCountX);
                shader->SetFloat("uTime", m_time);
                shader->SetVec2("uWindDir", m_windDir);
                shader->SetFloat("uWetness", m_wetness);
                shader->SetVec2("uShoreCenter", m_shoreCenter);
                shader->SetFloat("uShoreExtent", m_shoreExtent);
            }
            b.material->Bind();
            glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                        reinterpret_cast<const void *>(static_cast<uintptr_t>(b.firstCommand) * sizeof(DrawElementsIndirectCommand)),
                                        static_cast<GLsizei>(b.commandCount), 0);
            if (b.material->doubleSided && cullWasOn)
                glEnable(GL_CULL_FACE);
        }
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

} // namespace gfx
