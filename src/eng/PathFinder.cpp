#include "PathFinder.h"

<<<<<<< HEAD
#include <algorithm>
#include <unordered_set>

namespace eng
{
	PathFinder::Path PathFinder::FindPath(const glm::ivec2 &start, const glm::ivec2 &end)
	{
		std::deque<Node> allNodes;
		std::vector<Node *> open;
		std::unordered_set<int> closed;

		allNodes.emplace_back();

		Node *startNode = &allNodes.back();
		startNode->gridPos = start;

		open.push_back(startNode);

		while (!open.empty())
		{
=======
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

>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
			size_t bestIndex = 0;
			for (size_t i = 1; i < open.size(); ++i)
			{
				if (open[i]->fCost() < open[bestIndex]->fCost())
					bestIndex = i;
			}

<<<<<<< HEAD
			Node *currentNode = open[bestIndex];
=======
			auto currentNode = open[bestIndex];
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a

			open.erase(open.begin() + bestIndex);
			closed.insert(ToId(*currentNode));

			if (currentNode->gridPos == end)
			{
<<<<<<< HEAD
				auto path = ConstructPath(currentNode);
				return GetSmoothedPath(path);
			}

			for (auto &neighborData : GetNeighbors(currentNode))
			{
				if (closed.contains(ToId(neighborData)))
					continue;

				auto existing = FindOpenNode(neighborData.gridPos, open);
				if (existing)
					continue;

				
				glm::ivec2 delta = neighborData.gridPos - currentNode->gridPos;
				bool isDiagonal = delta.x != 0 && delta.y != 0;
				float moveCost = isDiagonal ? 1.4142f : 1.0f;
				float newGCost = currentNode->gCost + moveCost;
				// add slope cost
				NavCell &cell = grid.GetCell(neighborData.gridPos.x, neighborData.gridPos.y);
				moveCost += cell.slope * 5.0f;

				// prevent diagonal cutting through corners
				if (isDiagonal)
				{
					glm::ivec2 check1 = currentNode->gridPos + glm::ivec2(delta.x, 0);
					glm::ivec2 check2 = currentNode->gridPos + glm::ivec2(0, delta.y);
					if (!grid.IsWalkable(check1.x, check1.y) || !grid.IsWalkable(check2.x, check2.y))
						continue;
				}

				if (!existing)
				{
					allNodes.emplace_back(neighborData);

					Node *neighbor = &allNodes.back();
					neighbor->gCost = newGCost;
					neighbor->hCost = Heuristic(neighborData, end);
					neighbor->parent = currentNode;

					open.push_back(&allNodes.back());
				}
				else if (existing && newGCost < existing->gCost)
				{
					existing->gCost = newGCost;
					existing->parent = currentNode;
				}
			}
		}

		return Path();
	}

	PathFinder::Path PathFinder::ConstructPath(Node *endNode)
	{
		Path path;
		Node *current = endNode;
		while (current != nullptr)
		{
			path.push_back(current->gridPos);
=======
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
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
			current = current->parent;
		}
		std::reverse(path.begin(), path.end());
		return path;
	}

<<<<<<< HEAD
	PathFinder::Path PathFinder::GetSmoothedPath(const Path &path)
	{
		if (path.size() < 3)
			return path;

		Path smoothedPath;
		smoothedPath.push_back(path.front());
		glm::ivec2 lastPoint = path.front();

		for (size_t i = 1; i < path.size() - 1; ++i)
		{
			if (!HasLineOfSight(lastPoint, path[i + 1]))
			{
				smoothedPath.push_back(path[i]);
				lastPoint = path[i];
			}
		}

		smoothedPath.push_back(path.back());
		return smoothedPath;
	}

	PathFinder::NodeList PathFinder::GetNeighbors(const Node *node) const
	{
		static glm::ivec2 neighbors[8]
			= {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1), glm::ivec2(1, 1), glm::ivec2(1, -1), glm::ivec2(-1, 1), glm::ivec2(-1, -1)};

		NodeList result;
		for (auto &offset : neighbors)
		{
			glm::ivec2 neighborPos = node->gridPos + offset;

			if (!grid.IsWalkable(neighborPos.x, neighborPos.y))
=======
	std::vector<PathFinder::Node> PathFinder::GetNeighbors(const Node &node) const
	{
		static glm::ivec2 neighbors[4] = {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)};

		std::vector<Node> result;
		for (auto &offset : neighbors)
		{
			glm::ivec2 neighborPos = node.gridPos + offset;

			auto worldPos = grid.GridToWorld(neighborPos.x, neighborPos.y);

			if (!terrain.IsWalkable(worldPos.x, worldPos.z))
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
			{
				continue;
			}

			Node neighborNode;
			neighborNode.gridPos = neighborPos;
			result.push_back(neighborNode);
		}
		return result;
	}

<<<<<<< HEAD
	float PathFinder::Heuristic(const Node &node, const glm::ivec2 &end) const
	{
		return glm::distance(glm::vec2(node.gridPos.x, node.gridPos.y), glm::vec2(end.x, end.y));
	}

	bool PathFinder::HasLineOfSight(const glm::ivec2 &start, const glm::ivec2 &end) const
	{
		int x0 = start.x;
		int y0 = start.y;
		int x1 = end.x;
		int y1 = end.y;

		int dx = abs(x1 - x0);
		int dy = abs(y1 - y0);

		int sx = (x0 < x1) ? 1 : -1;
		int sy = (y0 < y1) ? 1 : -1;

		int err = dx - dy;

		while (true)
		{
			if (!grid.IsWalkable(x0, y0))
				return false;

			if (x0 == x1 && y0 == y1)
				break;

			int e2 = 2 * err;
			if (e2 > -dy)
			{
				err -= dy;
				x0 += sx;
			}
			if (e2 < dx)
			{
				err += dx;
				y0 += sy;
			}
		}
		return true;
	}

	PathFinder::Node *PathFinder::FindOpenNode(const glm::ivec2 &gridPos, const std::vector<Node *> &open) const
	{
		auto it = std::find_if(open.begin(), open.end(), [&](Node *node) { return node->gridPos == gridPos; });
		return (it != open.end()) ? *it : nullptr;
	}

=======
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
} // namespace eng