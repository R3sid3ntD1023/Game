#pragma once
#include "gfx/Buffer.h"
#include "gfx/Material.h"
#include "gfx/MeshManager.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace gfx {

struct DrawElementsIndirectCommand {
    uint32_t count;
    uint32_t instanceCount;
    uint32_t firstIndex;
    uint32_t baseVertex;
    uint32_t baseInstance;
};

// Matches `struct Instance` in brdf.vert (std430).
struct InstanceData {
    glm::mat4 model;
    uint32_t boneOffset;
    uint32_t pad[3];
};
constexpr uint32_t kNoSkin = 0xFFFFFFFFu;

// Sorts renderables by material, then mesh; emits one indirect command per (material, mesh)
// and one glMultiDrawElementsIndirect call per material.
class RenderSystem {
public:
    explicit RenderSystem(MeshManager& meshes);

    void Render(entt::registry& registry, const glm::mat4& viewProj, const glm::vec3& cameraPos);

    size_t LastDrawCalls() const { return m_drawCalls; }

private:
    struct Item {
        const Material* material;
        MeshHandle mesh;
        glm::mat4 model;
        uint32_t boneOffset;
    };
    struct Batch {
        const Material* material;
        uint32_t firstCommand;
        uint32_t commandCount;
    };

    MeshManager& m_meshes;
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
