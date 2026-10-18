#include "game/Terrain.h"
#include <algorithm>
#include <cmath>

namespace game
{
	static constexpr float kWorldHalfSize = Terrain::kHalfSize;
	static constexpr float kTerrainSize = Terrain::kIslandRadius;
	static constexpr float kWalkRadius = Terrain::kWalkRadius;
	static constexpr float kSeaLevel = Terrain::kSeaLevel;
	static constexpr float kHeightMapScale = 10.f;

	float ProceduralHeightSource::Height(float x, float z) const
	{
		const float r = std::sqrt(x * x + z * z) / kTerrainSize;
		float t = std::clamp((1.0f - r) / 0.4f, 0.0f, 1.0f);
		t = t * t * (3.0f - 2.0f * t);

		const float hills = 2.5f * std::sin(x * 0.07f) * std::cos(z * 0.06f) + 1.5f * std::sin(x * 0.13f + 1.3f) * std::sin(z * 0.11f)
							+ 0.5f * std::sin(x * 0.31f) * std::sin(z * 0.27f) + 5.0f * std::sin(x * 0.018f) * std::cos(z * 0.021f)
							+ 3.0f * std::sin(x * 0.045f + 2.0f) * std::sin(z * 0.04f); // broad rolling terrain
		return -4.0f + (4.0f + 6.0f + hills) * t;
	}

	float Terrain::Height(float x, float z) const
	{
		if (heightSource)
			return heightSource->Height(x, z);

		return 0.f;
	}

	gfx::MeshData Terrain::BuildMesh(int res) const
	{
		gfx::MeshData m;
		const float step = 2.0f * kWorldHalfSize / static_cast<float>(res);
		for (int i = 0; i <= res; ++i)
		{
			for (int j = 0; j <= res; ++j)
			{
				const float x = -kWorldHalfSize + static_cast<float>(j) * step;
				const float z = -kWorldHalfSize + static_cast<float>(i) * step;
				const float dhdx = (Height(x + 0.5f, z) - Height(x - 0.5f, z));
				const float dhdz = (Height(x, z + 0.5f) - Height(x, z - 0.5f));
				const glm::vec3 n = glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
				const glm::vec3 t = glm::normalize(glm::vec3(1.0f, dhdx, 0.0f));
				m.vertices.push_back({{x, Height(x, z), z}, n, {x / 8.0f, z / 8.0f}, {t, 1.0f}});
			}
		}
		const uint32_t stride = static_cast<uint32_t>(res + 1);
		for (int i = 0; i < res; ++i)
		{
			for (int j = 0; j < res; ++j)
			{
				const uint32_t a = static_cast<uint32_t>(i) * stride + static_cast<uint32_t>(j);
				const uint32_t b = a + stride;
				m.indices.insert(m.indices.end(), {a, b, b + 1, a, b + 1, a + 1});
			}
		}
		return m;
	}

	float HeightMapSource::Height(float x, float z) const
	{
		float u = (x + kWorldHalfSize) / (kWorldHalfSize * 2);
		float v = (z + kWorldHalfSize) / (kWorldHalfSize * 2);

		const int ix = static_cast<int>(u * source.width - 1);
		const int iz = static_cast<int>(v * source.height - 1);
		if (ix < 0 || ix >= source.width || iz < 0 || iz >= source.height)
			return 0.0f;

		float pixel = (static_cast<float>(source.data[iz * source.width + ix * 4])) / 255.0f;
		return pixel * kHeightMapScale;
	}

} // namespace game
