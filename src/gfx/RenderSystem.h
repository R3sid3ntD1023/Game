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

    // Matches `struct Instance` in ForwardMesh.vert / depth.vert (std430).
    struct InstanceData
    {
        glm::mat4 model;
        uint32_t boneOffset;
        float emissiveScale;
        float windStrength;
        uint32_t pad;
    };    constexpr uint32_t kNoSkin = 0xFFFFFFFFu;

    // Sorts renderables by material, then mesh; emits one indirect command per (material, mesh).
    // Usage per frame: Build, DepthPass (prepass for Forward+), ShadePass.
    class RenderSystem
    {
    public:
        explicit RenderSystem(MeshManager &meshes);

        void Build(entt::registry &registry);
        // Depth only, all geometry in one multi-draw. Leaves depth testing on and depth writes enabled.
        // skipEmissive drops light-emitting geometry (lamp heads), used by point/spot shadow passes so a lamp doesn't shadow itself.
        void DepthPass(const glm::mat4 &viewProj, bool skipEmissive = false);
        // Full shading, one multi-draw per material; expects the Forward+ buffers to be bound.
        void ShadePass(const glm::mat4 &viewProj, const glm::vec3 &cameraPos, int tileCountX);

        size_t LastDrawCalls() const { return m_drawCalls; }

        // World-space mesh drawn after shading, depth-tested and blended, with no shadows or lighting (e.g. the nav mesh).
        // uv.x of each vertex is a 0..1 value that tints it.
        void SetOverlayMesh(MeshHandle mesh) { m_overlayMesh = mesh; m_hasOverlay = true; }
        void ClearOverlayMesh() { m_hasOverlay = false; }
        void SetOverlayVisible(bool visible) { m_overlayVisible = visible; }
        bool IsOverlayVisible() const { return m_overlayVisible; }
        // mode 0: flat colour with cell outlines; mode 1: slope heat map. origin/cellSize locate the outline lattice.
        void SetOverlayStyle(int mode, const glm::vec2 &origin, float cellSize) { m_overlayMode = mode; m_overlayOrigin = origin; m_overlayCellSize = cellSize; }
        void SetOverlayMode(int mode) { m_overlayMode = mode; }
        // Foliage wind: time in seconds and a world XZ direction whose length is the wind strength (0..1).
        void SetWind(float time, const glm::vec2 &dir) { m_time = time; m_windDir = dir; }
        void OverlayPass(const glm::mat4 &viewProj, float alpha = 0.35f);


    private:
        struct Item
        {
            const Material *material;
            MeshHandle mesh;
            glm::mat4 model;
            uint32_t boneOffset;
            float emissiveScale;
            float windStrength;
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
        Shader m_depthAlphaShader;
        Shader m_overlayShader;
        MeshHandle m_overlayMesh = 0;
        bool m_hasOverlay = false;
        bool m_overlayVisible = true;
        int m_overlayMode = 0;
        float m_time = 0.0f;
        glm::vec2 m_windDir{0.0f};
        glm::vec2 m_overlayOrigin{0.0f};
        float m_overlayCellSize = 1.0f;
        GpuBuffer m_indirectBuffer;
        GpuBuffer m_modelBuffer;
        GpuBuffer m_boneBuffer;
        std::vector<Item> m_items;
        std::vector<DrawElementsIndirectCommand> m_commands;
        std::vector<InstanceData> m_instances;
        std::vector<glm::mat4> m_bones;
        std::vector<Batch> m_batches;
        size_t m_drawCalls = 0;
        uint32_t m_nonEmissiveCommands = 0;
        uint32_t m_opaqueCommands = 0; // commands before the first cutout batch
    };

} // namespace gfx
