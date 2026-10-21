#pragma once

#include "IWorldQuery.h"
#include "core/Core.h"
#include "gfx/Mesh.h"
#include <glm/glm.hpp>
#include <random>
#include <unordered_set>
#include <vector>

namespace eng
{
	struct NavCell
	{
		bool walkable;
		float height;
		float slope;
		glm::vec3 position;
	};

	class NavigationGrid
	{
	public:
		int width = 0;
		int height = 0;
		float cellSize;
		IWorldQuery *query;

		std::vector<NavCell> cells;

		void Initialize(int w, int h, float cell, IWorldQuery &terrain);

		void AddObstacle(const glm::vec3 &position, float radius, float agentRadius);

		// Axis-aligned footprint (world-space half extents).
		void AddBoxObstacle(const glm::vec3 &position, float halfX, float halfZ, float agentRadius);

		glm::ivec2 WorldToGrid(const glm::vec3 &worldPos) const;

		glm::vec3 GridToWorld(int x, int y) const;

		bool IsValid(int x, int y) const;

		bool IsWalkable(int x, int y) const;

		NavCell &GetCell(int x, int y) { return cells[y * width + x]; }

		// Region id of a walkable cell (-1 if blocked); cells in the same region are mutually reachable.
		int Region(int x, int y) const;

		bool Connected(int x0, int y0, int x1, int y1) const;

		// Uniformly picks a walkable cell connected to `center` within [minRadius, radius] (metres), like UE's
		// GetRandomReachablePointInRadius. Snaps to the nearest walkable cell if `center` sits on a blocked one.
		// Returns false when nothing is reachable.
		bool RandomReachablePoint(const glm::vec3 &center, float radius, float minRadius, std::mt19937 &rng, glm::vec3 &out) const;

		// Renderable surface over the walkable area: one vertex per cell (uv.x = slope 0..1), a quad per fully walkable 2x2 block.
		// Call after Initialize and any obstacles; submit the result to the RenderSystem.
		gfx::MeshData BuildMesh(float lift = 0.15f) const;

	private:
		void BuildRegions() const;

		mutable std::vector<int> m_regions;
		mutable bool m_regionsDirty = true;
	};
} // namespace eng