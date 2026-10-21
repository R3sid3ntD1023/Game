#include "PathFinder.h"
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace eng
{
	PathFinder::Path PathFinder::FindPath(const glm::ivec2 &start, const glm::ivec2 &end)
	{
		if (!grid.IsWalkable(start.x, start.y) || !grid.IsWalkable(end.x, end.y))
			return Path();

		// Unreachable goals would otherwise exhaust the whole search budget every call.
		if (!grid.Connected(start.x, start.y, end.x, end.y))
			return Path();

		// node storage is pointer-stable; each cell has at most one node
		std::unordered_map<int, Node> allNodes;
		using QueueEntry = std::pair<float, Node *>;
		auto cmp = [](const QueueEntry &a, const QueueEntry &b) { return a.first > b.first; };
		std::priority_queue<QueueEntry, std::vector<QueueEntry>, decltype(cmp)> open(cmp);
		std::unordered_set<int> closed;

		Node *startNode = &allNodes[ToId(Node{start})];
		startNode->gridPos = start;
		startNode->hCost = Heuristic(*startNode, end);
		open.emplace(startNode->fCost(), startNode);

		// Bounds worst-case cost (e.g. unreachable target) on a large grid to avoid frame spikes.
		constexpr size_t kMaxExpansions = 60000;

		while (!open.empty() && closed.size() < kMaxExpansions)
		{
			auto [queuedCost, currentNode] = open.top();
			open.pop();

			// skip stale queue entries
			if (closed.contains(ToId(*currentNode)))
				continue;

			closed.insert(ToId(*currentNode));

			if (currentNode->gridPos == end)
			{
				auto path = ConstructPath(currentNode);
				return GetSmoothedPath(path);
			}

			for (auto &neighborData : GetNeighbors(currentNode))
			{
				if (closed.contains(ToId(neighborData)))
					continue;

				glm::ivec2 delta = neighborData.gridPos - currentNode->gridPos;
				bool isDiagonal = delta.x != 0 && delta.y != 0;

				NavCell &cell = grid.GetCell(neighborData.gridPos.x, neighborData.gridPos.y);
				float moveCost = isDiagonal ? 1.4142f : 1.0f;
				moveCost += cell.slope * 5.0f;
				float newGCost = currentNode->gCost + moveCost;

				// prevent diagonal cutting through corners
				if (isDiagonal)
				{
					glm::ivec2 check1 = currentNode->gridPos + glm::ivec2(delta.x, 0);
					glm::ivec2 check2 = currentNode->gridPos + glm::ivec2(0, delta.y);
					if (!grid.IsWalkable(check1.x, check1.y) || !grid.IsWalkable(check2.x, check2.y))
						continue;
				}

				int id = ToId(neighborData);
				auto it = allNodes.find(id);
				if (it == allNodes.end())
				{
					Node *neighbor = &allNodes.emplace(id, neighborData).first->second;
					neighbor->gCost = newGCost;
					neighbor->hCost = Heuristic(neighborData, end);
					neighbor->parent = currentNode;

					open.emplace(neighbor->fCost(), neighbor);
				}
				else if (newGCost < it->second.gCost)
				{
					Node *existing = &it->second;
					existing->gCost = newGCost;
					existing->parent = currentNode;
					open.emplace(existing->fCost(), existing);
				}
			}
		}

		return Path();
	}

	PathFinder::Path PathFinder::ConstructPath(Node *endNode)
	{
		Path path;
		Node *current = endNode;
		while (current)
		{
			path.push_back(current->gridPos);

			current = current->parent;
		}
		std::reverse(path.begin(), path.end());
		return path;
	}

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
				continue;

			Node neighborNode;
			neighborNode.gridPos = neighborPos;
			result.push_back(neighborNode);
		}

		return result;
	}

	float PathFinder::Heuristic(const Node &node, const glm::ivec2 &end) const
	{
		// Octile distance matches the 8-way movement costs and expands far fewer nodes than Euclidean.
		float dx = float(std::abs(node.gridPos.x - end.x));
		float dy = float(std::abs(node.gridPos.y - end.y));
		return (dx + dy) + (1.4142f - 2.0f) * std::min(dx, dy);
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
} // namespace eng