#pragma once

#include "eng/IWorldQuery.h"
#include "eng/Scene.h"
#include "gfx/Mesh.h"
#include "gfx/Texture.h"
#include <memory>

namespace game
{
	struct IHeightSource
	{
		virtual float Height(float x, float z) const = 0;
	};

	struct ProceduralHeightSource : IHeightSource
	{
		float Height(float x, float z) const override;
	};

	struct HeightMapSource : IHeightSource
	{
		gfx::DecodedTexture source;
		float scale = 20.0f; // metres at full white (and below sea level at black)

		float Height(float x, float z) const override;
	};

	// Builds the height source a world's TerrainDesc describes (procedural hills when no usable height map is set).
	std::unique_ptr<IHeightSource> MakeHeightSource(const eng::TerrainDesc &desc);

	struct Terrain : eng::IWorldQuery
	{
		static constexpr float kHalfSize = 350.0f;
		static constexpr float kIslandRadius = 400.0f;
		static constexpr float kWalkRadius = 340.0f; // physics keeps bodies inside this circle
		static constexpr float kSeaLevel = 0.0f;

		float Height(float x, float z) const override;

		bool IsWalkable(float x, float z) const override;

		glm::vec3 Normal(float x, float z) const override;

		int GetHalfSize() const override { return static_cast<int>(kHalfSize); }

		float Slope(float x, float z) const override;

		gfx::MeshData BuildMesh(int resolution = 320) const;

		void SetHeightSource(std::unique_ptr<IHeightSource> &&source) { heightSource = std::move(source); }

	private:
		std::unique_ptr<IHeightSource> heightSource = nullptr;
	};

} // namespace game
