#include "editor/PreviewMesh.h"
#include "eng/AssetMeta.h"
#include "game/GrassGen.h"
#include "game/HerbivoreModel.h"
#include "game/IslandScene.h"
#include "game/RaptorModel.h"
#include "game/TreeGen.h"
#include "gfx/ModelLoader.h"
#include "gfx/Primitives.h"
#include <stdexcept>

namespace editor
{

	namespace
	{

		float Param(const std::vector<float> &p, size_t i, float fallback)
		{
			return i < p.size() ? p[i] : fallback;
		}

	} // namespace

	gfx::MeshData BuildPreviewMesh(const eng::MeshDesc &d, const game::Terrain &terrain)
	{
		using namespace game;
		if (d.kind == "model")
			return gfx::LoadModel(eng::ResolveAssetPath(d.path), Param(d.params, 0, 1.0f));

		// Same rule as the game: assets/models/<kind>.* replaces generated meshes, except the fixed ones below.
		const bool fixed = d.kind == kMeshTerrain || d.kind == kMeshRaptor || d.kind == kMeshTriceratops || d.kind == kMeshProtoceratops;
		if (!fixed && d.kind != "cube" && d.kind != "sphere" && d.kind != "plane")
		{
			if (const std::string external = gfx::FindModelOverride(d.kind); !external.empty())
				return gfx::LoadModel(external);
		}

		if (d.kind == "cube")
			return gfx::primitives::Cube(Param(d.params, 0, 0.5f));
		if (d.kind == "sphere")
			return gfx::primitives::Sphere(Param(d.params, 0, 0.5f));
		if (d.kind == "plane")
			return gfx::primitives::Plane(Param(d.params, 0, 1.0f), Param(d.params, 1, 1.0f));
		if (d.kind == kMeshTerrain)
			return terrain.BuildMesh(static_cast<int>(Param(d.params, 0, 160.0f)));
		if (d.kind == kMeshRaptor)
		{
			if (const std::string external = gfx::FindModelOverride("raptor"); !external.empty())
				return gfx::LoadSkinnedModel(external).mesh;
			return RaptorMesh();
		}
		if (d.kind == kMeshTriceratops)
			return HerbivoreMesh(HerbivoreSpecies::Triceratops);
		if (d.kind == kMeshProtoceratops)
			return HerbivoreMesh(HerbivoreSpecies::Protoceratops);
		if (d.kind == kMeshTreeTrunk)
			return GenerateRainforestTree(static_cast<uint32_t>(Param(d.params, 0, 100.0f))).trunk;
		if (d.kind == kMeshTreeCanopy)
			return GenerateRainforestTree(static_cast<uint32_t>(Param(d.params, 0, 100.0f))).canopy;
		if (d.kind == kMeshFern)
			return GenerateFern(static_cast<uint32_t>(Param(d.params, 0, 200.0f)));
		if (d.kind == kMeshGrass)
			return GenerateGrassTuft(static_cast<uint32_t>(Param(d.params, 0, 500.0f)));
		throw std::runtime_error("Unknown mesh kind '" + d.kind + "'");
	}

	const std::vector<const char *> &KnownMeshKinds()
	{
		using namespace game;
		static const std::vector<const char *> kinds = {
			"cube", "sphere", "plane", "model", kMeshTerrain, kMeshRaptor, kMeshTriceratops, kMeshProtoceratops, kMeshTreeTrunk, kMeshTreeCanopy, kMeshFern, kMeshGrass};
		return kinds;
	}

} // namespace editor
