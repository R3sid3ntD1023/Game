#pragma once

#include "IWorldQuery.h"
#include "core/Core.h"
#include <glm/glm.hpp>
#include <unordered_set>
#include <vector>

namespace eng
{
	struct NavCell
	{
		bool walkable;
		float height;
		float slope;
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

		bool IsValid(int x, int y) const;

		bool IsWalkable(int x, int y) const;

		NavCell &GetCell(int x, int y) { return cells[y * width + x]; }
	};
} // namespace eng