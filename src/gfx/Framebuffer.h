#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace gfx
{

    // Off-screen render target: one color texture (HDR by default) and an optional depth renderbuffer.
    class Framebuffer
    {
    public:
        Framebuffer(int width, int height, GLenum colorFormat = GL_RGBA16F, bool withDepth = true);
        ~Framebuffer();
        Framebuffer(const Framebuffer &) = delete;
        Framebuffer &operator=(const Framebuffer &) = delete;

        void Resize(int width, int height);
        void Bind() const;
        static void BindScreen(int width, int height);
        void Clear(const glm::vec4 &color) const;

        // Copies color (and optionally depth) into another framebuffer / the default framebuffer.
        void BlitTo(const Framebuffer &dst, bool includeDepth = false) const;
        void BlitToScreen(int screenWidth, int screenHeight) const;

        GLuint DepthTexture() const { return m_depth; }
        GLuint ColorTexture() const { return m_color; }
        int Width() const { return m_width; }
        int Height() const { return m_height; }

    private:
        void Create();
        void Destroy();

        GLuint m_fbo = 0;
        GLuint m_color = 0;
        GLuint m_depth = 0;
        GLenum m_format;
        bool m_withDepth;
        int m_width;
        int m_height;
    };

} // namespace gfx
