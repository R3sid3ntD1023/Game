#include "LineRenderer.h"
#include <spdlog/spdlog.h>

namespace gfx
{
	static const int kMaxLineCapacity = 20'000 * 2;
	static const int kMaxPointCapacity = 40'000;

	LineRenderer::LineRenderer()
		: m_shader(SHADER_DIR "line.vert", SHADER_DIR "line.frag"),
		  m_vbo(),
		  m_vao(),
		  m_pointVbo(),
		  m_pointVao()
	{
		m_vbo.Upload(nullptr, kMaxLineCapacity * sizeof(LineVertex));
		m_vao.SetVertexBuffer(m_vbo, sizeof(LineVertex), {{0, 3, offsetof(LineVertex, position)}, {1, 3, offsetof(LineVertex, color)}});
		m_lines.resize(kMaxLineCapacity);

		m_pointVbo.Upload(nullptr, kMaxPointCapacity * sizeof(Point));
		m_pointVao.SetVertexBuffer(m_pointVbo, sizeof(Point), {{0, 3, offsetof(Point, position)}, {1, 3, offsetof(Point, color)}});
		m_points.resize(kMaxPointCapacity);
	}

	void LineRenderer::Begin(float dt)
	{
		StartLineBatch();
		StartPointBatch();

		for (auto &line : m_persistentLines)
		{
			line.func(*this);

			if (line.duration != -1.0)
			{
				if (line.duration > 0.0f)
				{
					line.duration -= dt;

					if (line.duration <= 0.0f)
						line.duration = 0.0f;
				}
			}
		}

		// Remove expired persistent lines
		m_persistentLines.erase(
			std::remove_if(m_persistentLines.begin(), m_persistentLines.end(), [](const PersistentLine &line) { return line.duration == 0.0f; }), m_persistentLines.end()
		);
	}

	void LineRenderer::Render(const glm::mat4 &viewProjection)
	{
		m_viewProjection = viewProjection;
		if (m_lineCount)
		{
			FlushLines();
		}

		if (m_pointCount)
		{
			FlushPoints();
		}
	}

	void LineRenderer::FlushLines()
	{
		glLineWidth(1.0f);

		m_shader.Bind();
		m_shader.SetMat4("u_ViewProjection", m_viewProjection);

		m_vbo.Upload(m_lines.data(), m_lineCount * sizeof(LineVertex));
		m_vao.Bind();
		glDrawArrays(GL_LINES, 0, (GLsizei)m_lineCount);
	}

	void LineRenderer::FlushPoints()
	{
		glPointSize(5.0f);

		m_shader.Bind();
		m_shader.SetMat4("u_ViewProjection", m_viewProjection);

		m_pointVbo.Upload(m_points.data(), m_pointCount * sizeof(Point));
		m_pointVao.Bind();
		glDrawArrays(GL_POINTS, 0, (GLsizei)m_pointCount);
	}

	void LineRenderer::DrawLine(const Line &line)
	{
		if (m_lineCount >= kMaxLineCapacity)
		{
			FlushLines();
			StartLineBatch();
			// Do not return here, continue to add the line
		}

		m_lines[m_lineCount++] = LineVertex{line.p0, line.color};
		m_lines[m_lineCount++] = LineVertex{line.p1, line.color};
	}

	void LineRenderer::DrawPersistentLine(const PersistentLine &line)
	{

		m_persistentLines.push_back(std::move(line));
	}

	void LineRenderer::DrawPoint(const Point &point)
	{
		if (m_pointCount >= kMaxPointCapacity)
		{
			FlushPoints();
			StartPointBatch();
			// Do not return here, continue to add the point
		}

		m_points[m_pointCount++] = point;
	}

	void LineRenderer::DrawNavigationGrid(const eng::NavigationGrid &grid)
	{
		int w = grid.width;
		int h = grid.height;

		for (int y = 0; y < h; ++y)
			for (int x = 0; x < w; ++x)
			{

				glm::vec3 p0 = grid.GridToWorld(x, y);
				glm::vec3 p1 = grid.GridToWorld(x, y + 1);
				glm::vec3 p2 = grid.GridToWorld(x + 1, y);
				glm::vec3 col = grid.IsWalkable(x, y) ? glm::vec3(0.3f, 1.0f, 0.3f) : glm::vec3(1.0f, 0.0f, 0.0f);

				p0.y += 0.2f; // Slightly elevate the line to avoid z-fighting with the grid

				DrawLine({p0, p1, col});
				DrawLine({p0, p2, col});
			}
	}

	void LineRenderer::StartLineBatch()
	{
		m_lineCount = 0;
	}

	void LineRenderer::StartPointBatch()
	{
		m_pointCount = 0;
	}
} // namespace gfx