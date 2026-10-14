#pragma once
#include "gfx/Mesh.h"
#include <cstdint>

namespace game {

struct TreeModel {
    gfx::MeshData trunk;  // trunk, buttress roots and branches (bark material)
    gfx::MeshData canopy; // leaf clusters (foliage material)
    float height = 0.0f;
};

// Emergent rainforest tree: tapering trunk with buttress roots, forking branches and clustered canopy.
// Deterministic per seed. Both meshes are centered on mid-height, so one object origin fits trunk, canopy and collider.
TreeModel GenerateRainforestTree(uint32_t seed);

// Arching double-sided fern fronds, origin at the base.
gfx::MeshData GenerateFern(uint32_t seed);

} // namespace game
