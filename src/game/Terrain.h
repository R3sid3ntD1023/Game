#pragma once
#include "gfx/Mesh.h"

namespace game {

struct Terrain {
    static constexpr float kHalfSize = 110.0f;
    static constexpr float kIslandRadius = 100.0f;
    static constexpr float kWalkRadius = 92.0f; // physics keeps bodies inside this circle
    static constexpr float kSeaLevel = -0.2f;

    static float Height(float x, float z);
    static gfx::MeshData BuildMesh(int resolution = 160);
};

} // namespace game
