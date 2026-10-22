#pragma once
#include "eng/Scene.h"
#include "gfx/Mesh.h"
#include "game/Terrain.h"

namespace editor
{

	// CPU mesh for a scene mesh entry, covering the engine primitives and the game's generated meshes
	// (terrain, raptor/T-Rex, triceratops, protoceratops, trees, ferns, grass). Throws on unknown kinds.
	gfx::MeshData BuildPreviewMesh(const eng::MeshDesc &desc, const game::Terrain &terrain);

	// Mesh kinds the editor can create, in menu order.
	const std::vector<const char *> &KnownMeshKinds();

} // namespace editor
