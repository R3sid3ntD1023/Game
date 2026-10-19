#pragma once

#include "IWorldQuery.h"
#include "NavigationGrid.h"
#include <deque>
#include <glm/glm.hpp>
#include <vector>

namespace eng
{

	class PathFinder
	{
	public:
		struct Node
		{
			glm::ivec2 gridPos;
			float gCost = 0.0f;
			float hCost = 0.0f;

			Node *parent = nullptr;

			float fCost() const { return gCost + hCost; }
		};

	public:
		PathFinder(NavigationGrid &grid, IWorldQuery &terrain)
			: grid(grid),
			  terrain(terrain)
		{
		}
		std::deque<Node> FindPath(const glm::ivec2 &start, const glm::ivec2 &end);

		std::deque<Node> ConstructPath(Node &endNode);

	private:
		int ToId(const Node &pos) const { return pos.gridPos.x + pos.gridPos.y * grid.width; }

		std::vector<Node> GetNeighbors(const Node &node) const;

	private:
		NavigationGrid &grid;
		IWorldQuery &terrain;
	};

} // namespace eng