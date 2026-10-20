#include "NavigationGrid.h"

namespace eng
{
	void NavigationGrid::Initialize(int w, int h, float cell, IWorldQuery &terrain)
	{
		this->width = w;
		this->height = h;
		this->cellSize = cell;
		this->cells.resize(w * h);
		this->query = &terrain;

		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
<<<<<<< HEAD

				glm::vec3 worldPos = GridToWorld(x, y);

				int index = y * width + x;
				auto &currentCell = this->cells[index];
				currentCell.walkable = this->query->IsWalkable(worldPos.x, worldPos.z);
				currentCell.height = this->query->Height(worldPos.x, worldPos.z);
				currentCell.slope = this->query->Slope(worldPos.x, worldPos.z);
=======
				int index = y * width + x;
				float wx = float(x);
				float wz = float(y);
				this->cells[index].walkable = this->query->IsWalkable(wx, wz);
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
			}
		}
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
<<<<<<< HEAD

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
=======
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
} // namespace eng