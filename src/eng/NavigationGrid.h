#pragma once

#include "IWorldQuery.h"
#include <glm/glm.hpp>
#include <unordered_set>
#include <vector>

namespace eng
{
	struct NavCell
	{
		bool walkable;
	};

	class NavigationGrid
	{
	public:
		int width;
		int height;
		float cellSize;
		IWorldQuery *query;

		std::vector<NavCell> cells;

		void Initialize(int w, int h, float cell, IWorldQuery &terrain);

		glm::ivec2 WorldToGrid(const glm::vec3 &worldPos) const;

		glm::vec3 GridToWorld(int x, int y) const;
	};
} // namespace eng