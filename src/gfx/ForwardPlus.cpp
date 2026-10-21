#include "gfx/ForwardPlus.h"
#include <algorithm>

namespace gfx
{

    namespace
    {
        struct GpuLight
        { // matches `struct Light` in the shaders (std430)
            glm::vec4 positionRange;
            glm::vec4 colorType;
            glm::vec4 directionInner;
            glm::vec4 outer;
        };
    } // namespace

    ForwardPlus::ForwardPlus() : m_cull(SHADER_DIR "light_cull.comp")
    {
        m_counts.Create(1 << 16);
        m_indices.Create(1 << 20);
    }

    void ForwardPlus::Cull(const std::vector<LightData> &lights, GLuint depthTexture, const glm::mat4 &view, const glm::mat4 &proj,
                           int width, int height)
    {
        std::vector<GpuLight> gpu;
        gpu.reserve(std::min<size_t>(lights.size(), kMaxLights));
        for (size_t i = 0; i < lights.size() && i < static_cast<size_t>(kMaxLights); ++i)
        {
            const LightData &l = lights[i];
            gpu.push_back({{l.position, l.range},
                           {l.color, static_cast<float>(l.type)},
                           {l.direction, l.innerCos},
                           {l.outerCos, l.castShadow ? 1.0f : 0.0f, static_cast<float>(l.shadowSlot + 1), 0.0f}});
        }
        const int lightCount = static_cast<int>(gpu.size());
        if (gpu.empty())
            gpu.push_back({});
        m_lights.Upload(gpu.data(), gpu.size() * sizeof(GpuLight));

        m_tilesX = (width + kTileSize - 1) / kTileSize;
        const int tilesY = (height + kTileSize - 1) / kTileSize;
        const size_t tiles = static_cast<size_t>(m_tilesX) * tilesY;
        m_counts.Reserve(tiles * sizeof(uint32_t));
        m_indices.Reserve(tiles * kMaxLightsPerTile * sizeof(uint32_t));

        m_cull.Bind();
        m_cull.SetMat4("uView", view);
        m_cull.SetMat4("uProj", proj);
        m_cull.SetVec2("uScreen", {static_cast<float>(width), static_cast<float>(height)});
        m_cull.SetVec2("uTiles", {static_cast<float>(m_tilesX), static_cast<float>(tilesY)});
        m_cull.SetInt("uLightCount", lightCount);

        glBindTextureUnit(0, depthTexture);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_lights.Id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_counts.Id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_indices.Id());
        glDispatchCompute(static_cast<GLuint>(m_tilesX), static_cast<GLuint>(tilesY), 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

} // namespace gfx
