#pragma once

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

		float Height(float x, float z) const override;
	};

	struct Terrain
	{
		static constexpr float kHalfSize = 200.0f;
		static constexpr float kIslandRadius = 400.0f;
		static constexpr float kWalkRadius = 380.0f; // physics keeps bodies inside this circle
		static constexpr float kSeaLevel = -0.2f;

		float Height(float x, float z) const;

		gfx::MeshData BuildMesh(int resolution = 320) const;

		void SetHeightSource(std::unique_ptr<IHeightSource> &&source) { heightSource = std::move(source); }

	private:
		std::unique_ptr<IHeightSource> heightSource = nullptr;
	};

} // namespace game
