#pragma once

#include "IWorldQuery.h"
#include "NavigationGrid.h"
#include <deque>
#include <glm/glm.hpp>
#include <memory>
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

	private:
		int ToId(const Node &pos) const { return pos.gridPos.y * grid.width + pos.gridPos.x; }

		NodeList GetNeighbors(const Node *node) const;

		float Heuristic(const Node &node, const glm::ivec2 &end) const;

		bool HasLineOfSight(const glm::ivec2 &start, const glm::ivec2 &end) const;

		Node *FindOpenNode(const glm::ivec2 &gridPos, const std::vector<Node *> &open) const;

	private:
		NavigationGrid &grid;
	};

} // namespace eng