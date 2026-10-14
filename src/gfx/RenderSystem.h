#pragma once
#include "gfx/Buffer.h"
#include "gfx/Material.h"
#include "gfx/MeshManager.h"
#include "gfx/Shader.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace gfx
{

    struct DrawElementsIndirectCommand
    {
        uint32_t count;
        uint32_t instanceCount;
        uint32_t firstIndex;
        uint32_t baseVertex;
        uint32_t baseInstance;
    };

    // Matches `struct Instance` in brdf.vert / depth.vert (std430).
    struct InstanceData
    {
        glm::mat4 model;
        uint32_t boneOffset;
        float emissiveScale;
        uint32_t pad[2];
    };
    constexpr uint32_t kNoSkin = 0xFFFFFFFFu;

    // Sorts renderables by material, then mesh; emits one indirect command per (material, mesh).
    // Usage per frame: Build, DepthPass (prepass for Forward+), ShadePass.
    class RenderSystem
    {
    public:
        explicit RenderSystem(MeshManager &meshes);

        void Build(entt::registry &registry);
        // Depth only, all geometry in one multi-draw. Leaves depth testing on and depth writes enabled.
        void DepthPass(const glm::mat4 &viewProj);
        // Full shading, one multi-draw per material; expects the Forward+ buffers to be bound.
        void ShadePass(const glm::mat4 &viewProj, const glm::vec3 &cameraPos, int tileCountX);

        size_t LastDrawCalls() const { return m_drawCalls; }

    private:
        struct Item
        {
            const Material *material;
            MeshHandle mesh;
            glm::mat4 model;
            uint32_t boneOffset;
            float emissiveScale;
        };
        struct Batch
        {
            const Material *material;
            uint32_t firstCommand;
            uint32_t commandCount;
        };

        void BindBuffers() const;

        MeshManager &m_meshes;
        Shader m_depthShader;
        GpuBuffer m_indirectBuffer;
        GpuBuffer m_modelBuffer;
        GpuBuffer m_boneBuffer;
        std::vector<Item> m_items;
        std::vector<DrawElementsIndirectCommand> m_commands;
        std::vector<InstanceData> m_instances;
        std::vector<glm::mat4> m_bones;
        std::vector<Batch> m_batches;
        size_t m_drawCalls = 0;
    };

} // namespace gfx
