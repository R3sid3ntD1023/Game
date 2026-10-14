#pragma once
#include "gfx/Mesh.h"

namespace game {

struct Terrain {
    static constexpr float kHalfSize = 300.0f;
    static constexpr float kIslandRadius = 280.0f;
    static constexpr float kWalkRadius = 270.0f; // physics keeps bodies inside this circle
    static constexpr float kSeaLevel = -0.2f;

    static float Height(float x, float z);
    static gfx::MeshData BuildMesh(int resolution = 320);
};

} // namespace game
