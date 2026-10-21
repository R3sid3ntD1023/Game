#pragma once
#include "eng/GameObject.h"
#include "eng/World.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace game
{

	// Shared tuning for the self-preservation behaviour of every animal.
	inline constexpr float kFleeHealthFraction = 0.4f;	  // below this, being attacked makes an animal run
	inline constexpr float kRecentlyHurtSeconds = 6.0f;	  // how long "currently attacked" lasts after a hit
	inline constexpr float kSleepHealPerSecond = 0.03f;	  // fraction of max health restored per second asleep
	inline constexpr float kRestHealthFraction = 0.6f;	  // calm animals below this lie down to heal
	inline constexpr float kHealedFraction = 0.9f;		  // ...and get up again at this

	// A walkable, reachable point roughly `distance` away from `from`, swerving around blocked ground.
	// Returns the animal's own position when nowhere is open.
	inline glm::vec3 PickFleePoint(eng::GameObject &self, const glm::vec3 &from, float distance)
	{
		const glm::vec3 pos = self.GetTransform().position;
		glm::vec3 away = pos - from;
		away.y = 0.0f;
		away = glm::length(away) < 1e-3f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::normalize(away);

		auto &grid = self.GetWorld().NavigationGrid();
		const glm::ivec2 here = grid.WorldToGrid(pos);
		for (float scale : {1.0f, 0.6f, 0.3f})
		{
			for (float degrees : {0.0f, 35.0f, -35.0f, 70.0f, -70.0f, 110.0f, -110.0f})
			{
				glm::vec3 p = pos + glm::angleAxis(glm::radians(degrees), glm::vec3(0, 1, 0)) * away * distance * scale;
				const glm::ivec2 cell = grid.WorldToGrid(p);
				if (grid.IsWalkable(cell.x, cell.y) && grid.Connected(here.x, here.y, cell.x, cell.y))
				{
					p.y = pos.y;
					return p;
				}
			}
		}
		return pos;
	}

} // namespace game
