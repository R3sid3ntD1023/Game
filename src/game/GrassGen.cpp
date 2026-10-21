#include "game/GrassGen.h"
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <random>

namespace game
{

	gfx::MeshData GenerateGrassTuft(uint32_t seed, int blades)
	{
		gfx::MeshData m;
		std::mt19937 rng(seed);
		std::uniform_real_distribution<float> unit(0.0f, 1.0f);
		constexpr int kSegments = 3;

		for (int b = 0; b < blades; ++b)
		{
			const float ang = unit(rng) * glm::two_pi<float>();
			const float dist = std::sqrt(unit(rng)) * 0.25f;
			const glm::vec3 root{std::cos(ang) * dist, 0.0f, std::sin(ang) * dist};
			const float heading = unit(rng) * glm::two_pi<float>();
			const glm::vec3 lean{std::cos(heading), 0.0f, std::sin(heading)};
			const glm::vec3 side{-lean.z, 0.0f, lean.x};
			const float height = 0.35f + unit(rng) * 0.45f;
			const float width = 0.035f + unit(rng) * 0.02f;
			const float bend = 0.15f + unit(rng) * 0.35f;

			const uint32_t first = static_cast<uint32_t>(m.vertices.size());
			for (int s = 0; s <= kSegments; ++s)
			{
				const float t = static_cast<float>(s) / kSegments;
				const float w = width * (1.0f - t * 0.92f);
				const glm::vec3 centre = root + lean * (bend * t * t * height) + glm::vec3(0.0f, t * height, 0.0f);
				for (int e = 0; e < 2; ++e)
				{
					const float sign = e == 0 ? -1.0f : 1.0f;
					gfx::Vertex v;
					v.position = centre + side * (w * sign);
					v.normal = {0.0f, 1.0f, 0.0f};
					v.uv = {e == 0 ? 0.0f : 1.0f, t};
					v.tangent = {side, 1.0f};
					m.vertices.push_back(v);
				}
			}
			for (int s = 0; s < kSegments; ++s)
			{
				const uint32_t a = first + static_cast<uint32_t>(s) * 2;
				// Both windings so the blade is visible from either side.
				m.indices.insert(m.indices.end(), {a, a + 1, a + 2, a + 1, a + 3, a + 2});
				m.indices.insert(m.indices.end(), {a, a + 2, a + 1, a + 1, a + 2, a + 3});
			}
		}
		return m;
	}

} // namespace game
