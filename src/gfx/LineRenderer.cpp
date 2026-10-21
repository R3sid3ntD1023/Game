#include "LineRenderer.h"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace gfx
{
	static const int kMaxLineCapacity = 20'000;
	static const int kMaxPointCapacity = 40'000;

	static const std::vector<VertexAttribute> kVertexLayout = {{0, 3, offsetof(LineVertex, position)}, {1, 3, offsetof(LineVertex, color)}};

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

	LineRenderer::~LineRenderer() = default;

	void LineRenderer::Begin(float dt, const glm::mat4 &viewProjection)
	{
		m_viewProjection = viewProjection;
		StartLineBatch();
		StartPointBatch();

		for (const auto &line : m_queuedLines)
			DrawLine(line);
		m_queuedLines.clear();

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

		DrawStaticBatches();

		if (m_lineCount)
		{
			FlushLines();
		}

		if (m_pointCount)
		{
			FlushPoints();
		}
	}

	void LineRenderer::DrawStaticBatches()
	{
		bool any = false;
		for (auto &b : m_batches)
			any |= b && b->visible && b->lines.count;
		if (!any)
			return;

		m_shader.Bind();
		m_shader.SetMat4("u_ViewProjection", m_viewProjection);

		glLineWidth(m_lineWidth);
		for (auto &b : m_batches)
		{
			if (!b || !b->visible || !b->lines.count)
				continue;
			b->lines.vao.Bind();
			glDrawArrays(GL_LINES, 0, (GLsizei)b->lines.count);
		}
	}

	void LineRenderer::FlushLines()
	{
		glLineWidth(m_lineWidth);

		m_shader.Bind();
		m_shader.SetMat4("u_ViewProjection", m_viewProjection);

		m_vbo.Upload(m_lines.data(), m_lineCount * sizeof(LineVertex));
		m_vao.Bind();
		glDrawArrays(GL_LINES, 0, (GLsizei)m_lineCount);
	}

	void LineRenderer::FlushPoints()
	{
		glPointSize(m_pointSize);

		m_shader.Bind();
		m_shader.SetMat4("u_ViewProjection", m_viewProjection);

		m_pointVbo.Upload(m_points.data(), m_pointCount * sizeof(Point));
		m_pointVao.Bind();
		glDrawArrays(GL_POINTS, 0, (GLsizei)m_pointCount);
	}

	void LineRenderer::DrawLine(const Line &line)
	{
		if (m_captureLines)
		{
			m_captureLines->push_back({line.p0, line.color});
			m_captureLines->push_back({line.p1, line.color});
			return;
		}

		if (m_lineCount >= kMaxLineCapacity)
		{
			FlushLines();
			StartLineBatch();
		}

		m_lines[m_lineCount++] = LineVertex{line.p0, line.color};
		m_lines[m_lineCount++] = LineVertex{line.p1, line.color};
	}

	void LineRenderer::SetLineWidth(float width)
	{
		m_lineWidth = width;
	}

	void LineRenderer::SetPointSize(float size)
	{
		m_pointSize = size;
	}

	void LineRenderer::DrawRect(const glm::vec3 &min, const glm::vec3 &max, const glm::vec3 &color)
	{
		DrawLine({{min.x, min.y, min.z}, {max.x, min.y, min.z}, color});
		DrawLine({{max.x, min.y, min.z}, {max.x, min.y, max.z}, color});
		DrawLine({{max.x, min.y, max.z}, {min.x, min.y, max.z}, color});
		DrawLine({{min.x, min.y, max.z}, {min.x, min.y, min.z}, color});
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
		}

		m_points[m_pointCount++] = point;
	}

	void LineRenderer::DrawNavigationGrid(const eng::NavigationGrid &grid, int stride, int x0, int y0, int x1, int y1)
	{
		stride = std::max(stride, 1);
		x1 = x1 < 0 ? grid.width : std::min(x1, grid.width);
		y1 = y1 < 0 ? grid.height : std::min(y1, grid.height);
		const glm::vec3 lift(0.0f, 0.2f, 0.0f);
		const glm::vec3 open(0.3f, 1.0f, 0.3f);
		const glm::vec3 blocked(1.0f, 0.0f, 0.0f);

		// Edges between neighbouring cell centres form a lattice that follows the terrain.
		for (int y = y0; y < y1; y += stride)
		{
			for (int x = x0; x < x1; x += stride)
			{
				const auto &c = grid.cells[y * grid.width + x];
				const int nx = x + stride;
				const int ny = y + stride;

				if (nx < grid.width)
				{
					const auto &r = grid.cells[y * grid.width + nx];
					DrawLine({c.position + lift, r.position + lift, (c.walkable && r.walkable) ? open : blocked});
				}
				if (ny < grid.height)
				{
					const auto &d = grid.cells[ny * grid.width + x];
					DrawLine({c.position + lift, d.position + lift, (c.walkable && d.walkable) ? open : blocked});
				}
			}
		}
	}

	void LineRenderer::QueueLine(const Line &line)
	{
		m_queuedLines.push_back(line);
	}

	void LineRenderer::DrawQuad(const glm::vec3 &center, const glm::vec3 &halfRight, const glm::vec3 &halfUp, const glm::vec3 &color)
	{
		const glm::vec3 c0 = center - halfRight - halfUp;
		const glm::vec3 c1 = center + halfRight - halfUp;
		const glm::vec3 c2 = center + halfRight + halfUp;
		const glm::vec3 c3 = center - halfRight + halfUp;
		DrawLine({c0, c1, color});
		DrawLine({c1, c2, color});
		DrawLine({c2, c3, color});
		DrawLine({c3, c0, color});
	}

	void LineRenderer::DrawBox(const glm::vec3 &center, const glm::vec3 &halfExtents, const glm::vec3 &color, const glm::quat &rotation)
	{
		glm::vec3 c[8];
		for (int i = 0; i < 8; ++i)
		{
			const glm::vec3 local((i & 1) ? halfExtents.x : -halfExtents.x, (i & 2) ? halfExtents.y : -halfExtents.y, (i & 4) ? halfExtents.z : -halfExtents.z);
			c[i] = center + rotation * local;
		}

		// Each edge connects corners differing in exactly one axis bit.
		for (int i = 0; i < 8; ++i)
		{
			for (int bit = 1; bit <= 4; bit <<= 1)
			{
				if (!(i & bit))
					DrawLine({c[i], c[i | bit], color});
			}
		}
	}

	void LineRenderer::DrawAABB(const glm::vec3 &min, const glm::vec3 &max, const glm::vec3 &color)
	{
		DrawBox((min + max) * 0.5f, (max - min) * 0.5f, color);
	}

	void LineRenderer::DrawArc(
		const glm::vec3 &center, const glm::vec3 &axisU, const glm::vec3 &axisV, float radius, float startAngle, float endAngle, const glm::vec3 &color, int segments
	)
	{
		segments = std::max(segments, 1);
		const float step = (endAngle - startAngle) / float(segments);
		auto point = [&](float a) { return center + radius * (axisU * std::cos(a) + axisV * std::sin(a)); };

		glm::vec3 prev = point(startAngle);
		for (int i = 1; i <= segments; ++i)
		{
			const glm::vec3 next = point(startAngle + step * float(i));
			DrawLine({prev, next, color});
			prev = next;
		}
	}

	void LineRenderer::DrawCircle(const glm::vec3 &center, const glm::vec3 &axisU, const glm::vec3 &axisV, float radius, const glm::vec3 &color, int segments)
	{
		DrawArc(center, axisU, axisV, radius, 0.0f, 6.2831853f, color, segments);
	}

	void LineRenderer::DrawSphere(const glm::vec3 &center, float radius, const glm::vec3 &color, int segments)
	{
		DrawCircle(center, {1, 0, 0}, {0, 1, 0}, radius, color, segments);
		DrawCircle(center, {0, 1, 0}, {0, 0, 1}, radius, color, segments);
		DrawCircle(center, {1, 0, 0}, {0, 0, 1}, radius, color, segments);
	}

	void LineRenderer::DrawCapsule(const glm::vec3 &p0, const glm::vec3 &p1, float radius, const glm::vec3 &color, int segments)
	{
		const glm::vec3 axis = p1 - p0;
		const float len = glm::length(axis);
		if (len < 1e-5f)
		{
			DrawSphere(p0, radius, color, segments);
			return;
		}

		const glm::vec3 d = axis / len;
		const glm::vec3 helper = std::abs(d.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
		const glm::vec3 u = glm::normalize(glm::cross(helper, d));
		const glm::vec3 v = glm::cross(d, u);

		DrawCircle(p0, u, v, radius, color, segments);
		DrawCircle(p1, u, v, radius, color, segments);

		for (const glm::vec3 &s : {u, -u, v, -v})
			DrawLine({p0 + s * radius, p1 + s * radius, color});

		const int half = std::max(segments / 2, 1);
		const float pi = 3.14159265f;
		DrawArc(p1, u, d, radius, 0.0f, pi, color, half);
		DrawArc(p1, v, d, radius, 0.0f, pi, color, half);
		DrawArc(p0, u, -d, radius, 0.0f, pi, color, half);
		DrawArc(p0, v, -d, radius, 0.0f, pi, color, half);
	}

	void LineRenderer::UploadPart(BatchPart &part, const std::vector<LineVertex> &verts)
	{
		part.count = (uint32_t)verts.size();
		if (verts.empty())
			return;

		part.vbo.Upload(verts.data(), verts.size() * sizeof(LineVertex));
		if (!part.attached)
		{
			part.vao.SetVertexBuffer(part.vbo, sizeof(LineVertex), kVertexLayout);
			part.attached = true;
		}
	}

	void LineRenderer::FillBatch(StaticBatch &batch, const BuildFunc &build)
	{
		std::vector<LineVertex> lines;
		m_captureLines = &lines;
		build(*this);
		m_captureLines = nullptr;

		UploadPart(batch.lines, lines);
	}

	uint32_t LineRenderer::CreateStaticBatch(const BuildFunc &build)
	{
		auto batch = std::make_unique<StaticBatch>();
		FillBatch(*batch, build);
		m_batches.push_back(std::move(batch));
		return (uint32_t)m_batches.size() - 1;
	}

	void LineRenderer::RebuildStaticBatch(uint32_t id, const BuildFunc &build)
	{
		if (id < m_batches.size())
			FillBatch(*m_batches[id], build);
	}

	void LineRenderer::SetStaticBatchVisible(uint32_t id, bool visible)
	{
		if (id < m_batches.size())
			m_batches[id]->visible = visible;
	}

	bool LineRenderer::IsStaticBatchVisible(uint32_t id) const
	{
		return id < m_batches.size() && m_batches[id]->visible;
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