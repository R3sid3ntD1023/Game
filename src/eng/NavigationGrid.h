#pragma once

#include "IWorldQuery.h"
<<<<<<< HEAD
#include "core/Core.h"
=======
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
#include <glm/glm.hpp>
#include <unordered_set>
#include <vector>

namespace eng
{
	struct NavCell
	{
		bool walkable;
<<<<<<< HEAD
		float height;
		float slope;
=======
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
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
<<<<<<< HEAD

		bool IsValid(int x, int y) const;

		bool IsWalkable(int x, int y) const;

		NavCell &GetCell(int x, int y) { return cells[y * width + x]; }
=======
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
	};
} // namespace eng