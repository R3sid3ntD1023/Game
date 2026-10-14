#pragma once
#include "gfx/Buffer.h"
#include "gfx/Lights.h"
#include "gfx/Shader.h"
#include <glm/glm.hpp>
#include <vector>

namespace gfx
{

    // Forward+ light culling: a compute pass assigns lights to 16x16 pixel screen tiles using the depth
    // prepass for per-tile depth bounds. The shading pass reads the per-tile lists from SSBOs 3 (lights),
    // 4 (tile counts) and 5 (tile indices).
    class ForwardPlus
    {
    public:
        static constexpr int kTileSize = 16; // keep in sync with light_cull.comp / brdf.frag
        static constexpr int kMaxLightsPerTile = 64;

        ForwardPlus();

        void Cull(const std::vector<LightData> &lights, GLuint depthTexture, const glm::mat4 &view, const glm::mat4 &proj,
                  int width, int height);

        int TilesX() const { return m_tilesX; }

    private:
        Shader m_cull;
        GpuBuffer m_lights;
        GpuBuffer m_counts;
        GpuBuffer m_indices;
        int m_tilesX = 1;
    };

} // namespace gfx
