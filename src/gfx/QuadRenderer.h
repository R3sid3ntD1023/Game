#pragma once

#include "gfx/Shader.h"
#include "gfx/VertexArray.h"
#include <glm/glm.hpp>
#include <vector>

namespace gfx
{
	// Immediate-mode filled triangles and quads (flat colour, no culling). Submit from anywhere; Render draws and clears.
	class QuadRenderer
	{
	public:
		QuadRenderer();

		void DrawTriangle(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const glm::vec3 &color);

		// Quad spanned by halfRight and halfUp around center.
		void DrawQuad(const glm::vec3 &center, const glm::vec3 &halfRight, const glm::vec3 &halfUp, const glm::vec3 &color);

		// Axis-aligned rectangle in the XZ plane at min.y.
		void DrawRect(const glm::vec3 &min, const glm::vec3 &max, const glm::vec3 &color);

		void Render(const glm::mat4 &viewProjection);

	private:
		struct Vertex
		{
			glm::vec3 position;
			glm::vec3 color;
		};

		void Flush();

		Shader m_shader;
		VertexBuffer m_vbo;
		VertexArray m_vao;
		std::vector<Vertex> m_vertices;
		glm::mat4 m_viewProjection{1.0f};
	};
} // namespace gfx
