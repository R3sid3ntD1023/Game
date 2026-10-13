#pragma once
#include "gfx/Mesh.h"

namespace gfx::primitives {

MeshData Plane(float halfSize, float uvScale);
MeshData Cube(float halfSize);
MeshData Sphere(float radius, int segments = 48, int rings = 24);

} // namespace gfx::primitives
