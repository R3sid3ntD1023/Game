#pragma once
#include "gfx/Mesh.h"
#include <cstdint>

namespace game
{

	// A tuft of curved grass blades, origin at the base. Blades are double-wound so they render without culling.
	// uv.y runs 0 (root) to 1 (tip); normals point up so the tuft lights like the ground under it.
	gfx::MeshData GenerateGrassTuft(uint32_t seed, int blades = 9);

} // namespace game
