#pragma once

#include "IWorldQuery.h"
#include "NavigationGrid.h"
#include <deque>
#include <glm/glm.hpp>
<<<<<<< HEAD
#include <memory>
=======
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
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

<<<<<<< HEAD
		using NodeList = std::vector<Node>;
		using Path = std::vector<glm::ivec2>;

	public:
		PathFinder(NavigationGrid &grid)
			: grid(grid)
		{
		}
		Path FindPath(const glm::ivec2 &start, const glm::ivec2 &end);

		Path ConstructPath(Node *endNode);

		Path GetSmoothedPath(const Path &path);
=======
	public:
		PathFinder(NavigationGrid &grid, IWorldQuery &terrain)
			: grid(grid),
			  terrain(terrain)
		{
		}
		std::deque<Node> FindPath(const glm::ivec2 &start, const glm::ivec2 &end);

		std::deque<Node> ConstructPath(Node &endNode);
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a

	private:
		int ToId(const Node &pos) const { return pos.gridPos.x + pos.gridPos.y * grid.width; }

<<<<<<< HEAD
		NodeList GetNeighbors(const Node *node) const;

		float Heuristic(const Node &node, const glm::ivec2 &end) const;

		bool HasLineOfSight(const glm::ivec2 &start, const glm::ivec2 &end) const;

		Node *FindOpenNode(const glm::ivec2 &gridPos, const std::vector<Node *> &open) const;

	private:
		NavigationGrid &grid;
=======
		std::vector<Node> GetNeighbors(const Node &node) const;

	private:
		NavigationGrid &grid;
		IWorldQuery &terrain;
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
	};

} // namespace eng