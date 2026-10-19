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
				int index = y * width + x;
				float wx = float(x);
				float wz = float(y);
				this->cells[index].walkable = this->query->IsWalkable(wx, wz);
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
} // namespace eng