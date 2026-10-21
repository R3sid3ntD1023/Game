#pragma once
#include <cstdint>
#include <glm/glm.hpp>

namespace gfx {

enum class LightType : uint32_t { Point = 0, Spot = 1, Directional = 2 }; // values match ForwardMesh.frag / light_cull.comp

constexpr int kMaxLights = 512;

// CPU-side light as gathered by World each frame and uploaded by ForwardPlus.
struct LightData {
    glm::vec3 position{ 0.0f };
    float range = 10.0f;                  // point/spot: cutoff radius
    glm::vec3 color{ 1.0f };              // radiance, already multiplied by intensity
    LightType type = LightType::Point;
    glm::vec3 direction{ 0.0f, 0.0f, -1.0f }; // spot/directional: direction the light travels
    float innerCos = 1.0f;                // spot: full intensity inside this cone
    float outerCos = 0.9f;                // spot: zero intensity outside this cone
    bool castShadow = false;              // directional: cascaded map; point/spot: needs shadowSlot
    int shadowSlot = -1;                  // point/spot: LocalShadowMap slot assigned by World, -1 = none
};

} // namespace gfx
