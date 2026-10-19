#pragma once

#include "eng/NavigationGrid.h"
#include "gfx/Camera.h"
#include "gfx/Shader.h"
#include "gfx/VertexArray.h"
#include <functional>
#include <vector>

namespace gfx
{
	struct Line
	{
		glm::vec3 p0{0.0f};
		glm::vec3 p1{0.0f};
		glm::vec3 color{1.0f};
	};

	struct LineVertex
	{
		glm::vec3 position{0.0f};
		glm::vec3 color{1.0f};
	};

	struct Point
	{
		glm::vec3 position{0.0f};
		glm::vec3 color{1.0f};
	};

	struct LineRenderer;

	struct PersistentLine
	{
		std::function<void(LineRenderer &)> func;
		float duration{-1.0f}; //-1 infinite
	};

	struct LineRenderer
	{
		LineRenderer();

		void Begin(float dt);

		void Render(const glm::mat4 &viewProjection);

		void DrawLine(const Line &line);

		void DrawPersistentLine(const PersistentLine &line);

		void DrawPoint(const Point &point);

		void DrawNavigationGrid(const eng::NavigationGrid &grid);

	private:
		void StartLineBatch();

		void StartPointBatch();

		void FlushLines();

		void FlushPoints();

	private:
		gfx::Shader m_shader;
		gfx::VertexBuffer m_vbo;
		gfx::VertexArray m_vao;

		gfx::VertexBuffer m_pointVbo;
		gfx::VertexArray m_pointVao;

		uint32_t m_pointCount{0};
		uint32_t m_lineCount{0};

		glm::mat4 m_viewProjection;

		std::vector<LineVertex> m_lines;
		std::vector<Point> m_points;

		std::vector<PersistentLine> m_persistentLines;
	};
} // namespace gfx