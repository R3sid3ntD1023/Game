#include "gfx/QuadRenderer.h"
#include <glad/glad.h>

namespace gfx
{
	static constexpr size_t kMaxVertices = 30'000;

	QuadRenderer::QuadRenderer() : m_shader(SHADER_DIR "line.vert", SHADER_DIR "line.frag")
	{
		m_vbo.Upload(nullptr, kMaxVertices * sizeof(Vertex));
		m_vao.SetVertexBuffer(m_vbo, sizeof(Vertex), {{0, 3, offsetof(Vertex, position)}, {1, 3, offsetof(Vertex, color)}});
		m_vertices.reserve(kMaxVertices);
	}

	void QuadRenderer::DrawTriangle(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const glm::vec3 &color)
	{
		if (m_vertices.size() + 3 > kMaxVertices)
			Flush();
		m_vertices.push_back({a, color});
		m_vertices.push_back({b, color});
		m_vertices.push_back({c, color});
	}

	void QuadRenderer::DrawQuad(const glm::vec3 &center, const glm::vec3 &halfRight, const glm::vec3 &halfUp, const glm::vec3 &color)
	{
		const glm::vec3 c0 = center - halfRight - halfUp;
		const glm::vec3 c1 = center + halfRight - halfUp;
		const glm::vec3 c2 = center + halfRight + halfUp;
		const glm::vec3 c3 = center - halfRight + halfUp;
		DrawTriangle(c0, c1, c2, color);
		DrawTriangle(c0, c2, c3, color);
	}

	void QuadRenderer::DrawRect(const glm::vec3 &min, const glm::vec3 &max, const glm::vec3 &color)
	{
		const glm::vec3 c = (min + max) * 0.5f;
		DrawQuad(c, {(max.x - min.x) * 0.5f, 0.0f, 0.0f}, {0.0f, 0.0f, (max.z - min.z) * 0.5f}, color);
	}

	void QuadRenderer::Render(const glm::mat4 &viewProjection)
	{
		m_viewProjection = viewProjection;
		Flush();
	}

	void QuadRenderer::Flush()
	{
		if (m_vertices.empty())
			return;

		m_shader.Bind();
		m_shader.SetMat4("u_ViewProjection", m_viewProjection);

		const GLboolean cull = glIsEnabled(GL_CULL_FACE);
		glDisable(GL_CULL_FACE);
		m_vbo.Upload(m_vertices.data(), m_vertices.size() * sizeof(Vertex));
		m_vao.Bind();
		glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
		if (cull)
			glEnable(GL_CULL_FACE);

		m_vertices.clear();
	}
} // namespace gfx
