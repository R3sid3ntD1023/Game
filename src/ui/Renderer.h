#pragma once
#include "gfx/Buffer.h"
#include "gfx/Shader.h"
#include "gfx/VertexArray.h"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace ui
{

	using Color = glm::vec4;

	struct Rect
	{
		glm::vec2 pos{0.0f};
		glm::vec2 size{0.0f};
		bool Contains(const glm::vec2 &p) const { return p.x >= pos.x && p.y >= pos.y && p.x < pos.x + size.x && p.y < pos.y + size.y; }
	};

	// Batched 2D drawing in framebuffer pixels (origin top-left): rectangles and TrueType text from one atlas texture.
	class Renderer
	{
	public:
		Renderer();
		~Renderer();
		Renderer(const Renderer &) = delete;
		Renderer &operator=(const Renderer &) = delete;

		void Begin(const glm::vec2 &screen);
		void End(); // draws everything queued since Begin

		void FillRect(const Rect &r, const Color &color);
		void FillGradient(const Rect &r, const Color &top, const Color &bottom);
		void StrokeRect(const Rect &r, const Color &color, float thickness);
		void DrawImage(GLuint texture, const Rect &r, const Color &color);
		void Text(const std::string &text, const glm::vec2 &pos, float size, const Color &color);
		glm::vec2 MeasureText(const std::string &text, float size) const; // {width, size}

	private:
		struct Vertex
		{
			glm::vec2 pos;
			glm::vec2 uv;
			glm::vec4 color;
			int mode = 0; // 0=quad, 1=texturequad
		};

		struct DrawCommand
		{
			GLuint texture;
			size_t vertexOffset = 0;
			size_t vertexCount = 0;
		};

		struct Glyph
		{
			float u0, v0, u1, v1;
			float xoff, yoff, w, h, advance; // in atlas-bake pixels
		};

		void Quad(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &uv0, const glm::vec2 &uv1, const Color &top, const Color &bottom);

		void QuadTexture(GLuint texture, const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &uv0, const glm::vec2 &uv1, const Color &top, const Color &bottom);

		void AddVertices(GLuint texture, Vertex *vertices, size_t count);

		gfx::Shader m_shader;
		gfx::VertexBuffer m_vbo;
		gfx::VertexArray m_vao;
		GLuint m_atlas = 0;
		std::vector<Glyph> m_glyphs;
		glm::vec2 m_whiteUv{0.0f};
		glm::vec2 m_screen{1.0f};
		std::vector<Vertex> m_vertices;

		std::vector<DrawCommand> m_drawCommands;
	};

} // namespace ui
