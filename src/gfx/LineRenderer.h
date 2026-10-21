#pragma once

#include "eng/NavigationGrid.h"
#include "gfx/Camera.h"
#include "gfx/Shader.h"
#include "gfx/VertexArray.h"
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <memory>
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
		using BuildFunc = std::function<void(LineRenderer &)>;
		static constexpr uint32_t kInvalidBatch = ~0u;

		LineRenderer();
		~LineRenderer();

		void Begin(float dt, const glm::mat4 &viewProjection);

		void Render(const glm::mat4 &viewProjection);

		void DrawLine(const Line &line);

		// One-frame line submitted from anywhere (e.g. Update); drawn on the next Begin.
		void QueueLine(const Line &line);

		void DrawRect(const glm::vec3 &min, const glm::vec3 &max, const glm::vec3 &color);

		// Wireframe shapes
		void DrawQuad(const glm::vec3 &center, const glm::vec3 &halfRight, const glm::vec3 &halfUp, const glm::vec3 &color);

		void DrawBox(const glm::vec3 &center, const glm::vec3 &halfExtents, const glm::vec3 &color, const glm::quat &rotation = glm::quat(1, 0, 0, 0));

		void DrawAABB(const glm::vec3 &min, const glm::vec3 &max, const glm::vec3 &color);

		void DrawCircle(const glm::vec3 &center, const glm::vec3 &axisU, const glm::vec3 &axisV, float radius, const glm::vec3 &color, int segments = 24);

		void DrawArc(
			const glm::vec3 &center, const glm::vec3 &axisU, const glm::vec3 &axisV, float radius, float startAngle, float endAngle, const glm::vec3 &color, int segments = 12
		);

		// Same shapes as DrawArc / DrawCircle, but submitted as one-frame queued lines (callable from Update).
		void QueueArc(
			const glm::vec3 &center, const glm::vec3 &axisU, const glm::vec3 &axisV, float radius, float startAngle, float endAngle, const glm::vec3 &color, int segments = 12
		);
		void QueueCircle(const glm::vec3 &center, float radius, const glm::vec3 &color, int segments = 32);

		void DrawSphere(const glm::vec3 &center, float radius, const glm::vec3 &color, int segments = 24);

		// Capsule between the two hemisphere centers p0 and p1.
		void DrawCapsule(const glm::vec3 &p0, const glm::vec3 &p1, float radius, const glm::vec3 &color, int segments = 24);

		void DrawPersistentLine(const PersistentLine &line);

		void DrawPoint(const Point &point);

		// Static batches: geometry is built once into GPU buffers and drawn with one call per frame.
		uint32_t CreateStaticBatch(const BuildFunc &build);

		void RebuildStaticBatch(uint32_t id, const BuildFunc &build);

		void SetStaticBatchVisible(uint32_t id, bool visible);

		bool IsStaticBatchVisible(uint32_t id) const;

		// Grid sample lines; stride > 1 decimates. [x0,x1) x [y0,y1) selects a cell range (x1/y1 < 0 = to the end).
		void DrawNavigationGrid(const eng::NavigationGrid &grid, int stride = 1, int x0 = 0, int y0 = 0, int x1 = -1, int y1 = -1);

		void SetLineWidth(float width);

		void SetPointSize(float size);

	private:
		struct BatchPart
		{
			VertexBuffer vbo;
			VertexArray vao;
			uint32_t count = 0;
			bool attached = false;
		};

		struct StaticBatch
		{
			BatchPart lines;
			bool visible = true;
		};

		void StartLineBatch();

		void StartPointBatch();

		void FlushLines();

		void FlushPoints();

		void FillBatch(StaticBatch &batch, const BuildFunc &build);

		static void UploadPart(BatchPart &part, const std::vector<LineVertex> &verts);

		void DrawStaticBatches();

	private:
		std::vector<LineVertex> *m_captureLines{nullptr};

		std::vector<Line> m_queuedLines;
		std::vector<std::unique_ptr<StaticBatch>> m_batches;

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

		float m_lineWidth{1.0f};
		float m_pointSize{5.0f};
	};
} // namespace gfx