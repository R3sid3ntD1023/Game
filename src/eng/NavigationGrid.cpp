#include "NavigationGrid.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace eng
{
	void NavigationGrid::Initialize(int w, int h, float cell, IWorldQuery &terrain)
	{
		this->width = w;
		this->height = h;
		this->cellSize = cell;
		this->cells.resize(w * h);
		this->query = &terrain;
		m_regionsDirty = true;

		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{

				glm::vec3 worldPos = GridToWorld(x, y);

				int index = y * width + x;
				auto &currentCell = this->cells[index];
				currentCell.walkable = this->query->IsWalkable(worldPos.x, worldPos.z);
				currentCell.height = this->query->Height(worldPos.x, worldPos.z);
				currentCell.slope = this->query->Slope(worldPos.x, worldPos.z);
				currentCell.position = worldPos;
			}
		}
	}

	bool NavigationGrid::RandomReachablePoint(const glm::vec3 &center, float radius, float minRadius, std::mt19937 &rng, glm::vec3 &out) const
	{
		if (width <= 0 || height <= 0)
			return false;

		glm::ivec2 start = WorldToGrid(center);
		if (!IsWalkable(start.x, start.y))
		{
			// Snap to the closest walkable cell in growing rings (agent hugging an obstacle edge).
			bool found = false;
			for (int ring = 1; ring <= 4 && !found; ++ring)
			{
				float best = std::numeric_limits<float>::max();
				for (int dy = -ring; dy <= ring; ++dy)
				{
					for (int dx = -ring; dx <= ring; ++dx)
					{
						if (std::max(std::abs(dx), std::abs(dy)) != ring || !IsWalkable(start.x + dx, start.y + dy))
							continue;
						const float d = float(dx * dx + dy * dy);
						if (d < best)
						{
							best = d;
							out = {float(start.x + dx), 0.0f, float(start.y + dy)};
							found = true;
						}
					}
				}
			}
			if (!found)
				return false;
			start = {int(out.x), int(out.z)};
		}

		const int region = Region(start.x, start.y);
		if (region < 0)
			return false;

		const glm::ivec2 c = WorldToGrid(center);
		const int reach = int(std::ceil(radius / cellSize));
		const float r2 = radius * radius;
		const float minR2 = minRadius * minRadius;

		// Reservoir sampling: one pass, uniform over qualifying cells, no allocation.
		int seen = 0;
		int pickX = -1, pickY = -1;
		for (int y = std::max(c.y - reach, 0); y <= std::min(c.y + reach, height - 1); ++y)
		{
			for (int x = std::max(c.x - reach, 0); x <= std::min(c.x + reach, width - 1); ++x)
			{
				const float dx = float(x - c.x) * cellSize;
				const float dy = float(y - c.y) * cellSize;
				const float d2 = dx * dx + dy * dy;
				if (d2 > r2 || d2 < minR2 || m_regions[y * width + x] != region)
					continue;
				++seen;
				if (std::uniform_int_distribution<int>(1, seen)(rng) == 1)
				{
					pickX = x;
					pickY = y;
				}
			}
		}
		if (seen == 0)
			return false;

		// Jitter within the cell, but stay inside it so the point remains walkable.
		std::uniform_real_distribution<float> jitter(-0.4f, 0.4f);
		const glm::vec3 p = cells[pickY * width + pickX].position;
		const float x = p.x + jitter(rng) * cellSize;
		const float z = p.z + jitter(rng) * cellSize;
		out = glm::vec3(x, query ? query->Height(x, z) : p.y, z);
		return true;
	}

	gfx::MeshData NavigationGrid::BuildMesh(float lift) const
	{
		gfx::MeshData mesh;
		if (width < 2 || height < 2)
			return mesh;

		mesh.vertices.reserve(cells.size());
		for (const NavCell &c : cells)
		{
			gfx::Vertex v{};
			v.position = c.position + glm::vec3(0.0f, lift, 0.0f);
			v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
			v.uv = glm::vec2(std::clamp(c.slope * 2.0f, 0.0f, 1.0f), 0.0f);
			v.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			mesh.vertices.push_back(v);
		}

		for (int y = 0; y < height - 1; ++y)
		{
			for (int x = 0; x < width - 1; ++x)
			{
				const uint32_t i00 = y * width + x;
				const uint32_t i10 = i00 + 1;
				const uint32_t i01 = i00 + width;
				const uint32_t i11 = i01 + 1;
				if (!(cells[i00].walkable && cells[i10].walkable && cells[i01].walkable && cells[i11].walkable))
					continue;
				mesh.indices.insert(mesh.indices.end(), {i00, i01, i10, i10, i01, i11});
			}
		}
		return mesh;
	}

	void NavigationGrid::AddObstacle(const glm::vec3 &position, float radius, float agentRadius)
	{
		m_regionsDirty = true;
		auto gridPos = WorldToGrid(position);
		const float reach = radius + agentRadius;
		int radiusInCells = static_cast<int>(std::ceil(reach / cellSize));
		for (int y = gridPos.y - radiusInCells; y <= gridPos.y + radiusInCells; ++y)
		{
			for (int x = gridPos.x - radiusInCells; x <= gridPos.x + radiusInCells; ++x)
			{
				if (!IsValid(x, y))
				{
					continue;
				}

				float dx = float(x - gridPos.x);
				float dy = float(y - gridPos.y);
				float d = std::sqrt(dx * dx + dy * dy) * cellSize;
				if (d <= reach)
					cells[y * width + x].walkable = false;
			}
		}
	}

	void NavigationGrid::AddBoxObstacle(const glm::vec3 &position, float halfX, float halfZ, float agentRadius)
	{
		m_regionsDirty = true;
		const glm::ivec2 lo = WorldToGrid(position - glm::vec3(halfX + agentRadius, 0.0f, halfZ + agentRadius));
		const glm::ivec2 hi = WorldToGrid(position + glm::vec3(halfX + agentRadius, 0.0f, halfZ + agentRadius));
		for (int y = lo.y; y <= hi.y; ++y)
			for (int x = lo.x; x <= hi.x; ++x)
				if (IsValid(x, y))
					cells[y * width + x].walkable = false;
	}

	void NavigationGrid::BuildRegions() const
	{
		m_regionsDirty = false;
		m_regions.assign(size_t(width) * height, -1);

		std::vector<int> stack;
		int nextRegion = 0;
		for (int start = 0; start < width * height; ++start)
		{
			if (!cells[start].walkable || m_regions[start] != -1)
				continue;

			m_regions[start] = nextRegion;
			stack.push_back(start);
			while (!stack.empty())
			{
				const int cur = stack.back();
				stack.pop_back();
				const int cx = cur % width;
				const int cy = cur / width;
				const int nx[4] = {cx + 1, cx - 1, cx, cx};
				const int ny[4] = {cy, cy, cy + 1, cy - 1};
				for (int i = 0; i < 4; ++i)
				{
					if (!IsWalkable(nx[i], ny[i]))
						continue;
					const int n = ny[i] * width + nx[i];
					if (m_regions[n] != -1)
						continue;
					m_regions[n] = nextRegion;
					stack.push_back(n);
				}
			}
			++nextRegion;
		}
	}

	int NavigationGrid::Region(int x, int y) const
	{
		if (!IsValid(x, y))
			return -1;
		if (m_regionsDirty)
			BuildRegions();
		return m_regions[y * width + x];
	}

	bool NavigationGrid::Connected(int x0, int y0, int x1, int y1) const
	{
		const int a = Region(x0, y0);
		return a >= 0 && a == Region(x1, y1);
	}

	glm::ivec2 NavigationGrid::WorldToGrid(const glm::vec3 &worldPos) const
	{
		// query is used to get the height of the terrain if needed
		int halfSize = query->GetHalfSize();
		int x = int((worldPos.x + halfSize) / cellSize);
		int y = int((worldPos.z + halfSize) / cellSize);
		return glm::ivec2(x, y);
	}

	glm::vec3 NavigationGrid::GridToWorld(int x, int y) const
	{
		int halfSize = query->GetHalfSize();

		glm::vec3 worldPos;
		worldPos.x = -halfSize + (x + 0.5f) * cellSize;
		worldPos.z = -halfSize + (y + 0.5f) * cellSize;
		worldPos.y = query->Height(worldPos.x, worldPos.z);

		return worldPos;
	}

	bool NavigationGrid::IsValid(int x, int y) const
	{
		return x >= 0 && x < width && y >= 0 && y < height;
	}

	bool NavigationGrid::IsWalkable(int x, int y) const
	{
		if (!IsValid(x, y))
			return false;

		int index = y * width + x;
		return cells[index].walkable;
	}
} // namespace eng