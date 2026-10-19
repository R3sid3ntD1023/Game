#include "PathFinder.h"

namespace eng
{
	std::deque<PathFinder::Node> PathFinder::FindPath(const glm::ivec2 &start, const glm::ivec2 &end)
	{
		std::deque<Node> nodes;
		std::vector<Node *> open;
		std::unordered_set<int> closed;

		Node startNode;
		startNode.gridPos = start;
		open.push_back(&nodes.emplace_back(startNode));

		while (!open.empty())
		{

			size_t bestIndex = 0;
			for (size_t i = 1; i < open.size(); ++i)
			{
				if (open[i]->fCost() < open[bestIndex]->fCost())
					bestIndex = i;
			}

			auto currentNode = open[bestIndex];

			open.erase(open.begin() + bestIndex);
			closed.insert(ToId(*currentNode));

			if (currentNode->gridPos == end)
			{
				return ConstructPath(*currentNode);
			}

			for (auto neighbor : GetNeighbors(*currentNode))
			{
				if (closed.contains(ToId(neighbor)))
					continue;

				// evaluate costs
				// add/update open
				neighbor.gCost = currentNode->gCost + 1;
				neighbor.hCost = float(glm::abs(neighbor.gridPos.x - end.x)) + float(glm::abs(neighbor.gridPos.y - end.y));
				neighbor.parent = currentNode;

				open.push_back(&nodes.emplace_back(neighbor));
			}
		}

		return std::deque<Node>();
	}

	std::deque<PathFinder::Node> PathFinder::ConstructPath(Node &endNode)
	{
		std::deque<Node> path;
		Node *current = &endNode;
		while (current)
		{
			path.push_back(*current);
			current = current->parent;
		}
		std::reverse(path.begin(), path.end());
		return path;
	}

	std::vector<PathFinder::Node> PathFinder::GetNeighbors(const Node &node) const
	{
		static glm::ivec2 neighbors[4] = {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)};

		std::vector<Node> result;
		for (auto &offset : neighbors)
		{
			glm::ivec2 neighborPos = node.gridPos + offset;

			auto worldPos = grid.GridToWorld(neighborPos.x, neighborPos.y);

			if (!terrain.IsWalkable(worldPos.x, worldPos.z))
			{
				continue;
			}

			Node neighborNode;
			neighborNode.gridPos = neighborPos;
			result.push_back(neighborNode);
		}
		return result;
	}

} // namespace eng