#include "gfx/Framebuffer.h"
#include <spdlog/spdlog.h>

namespace gfx
{

    Framebuffer::Framebuffer(int width, int height, GLenum colorFormat, bool withDepth)
        : m_format(colorFormat), m_withDepth(withDepth), m_width(width), m_height(height)
    {
        Create();
    }

    Framebuffer::~Framebuffer() { Destroy(); }

    void Framebuffer::Create()
    {
        glCreateFramebuffers(1, &m_fbo);

        glCreateTextures(GL_TEXTURE_2D, 1, &m_color);
        glTextureStorage2D(m_color, 1, m_format, m_width, m_height);
        glTextureParameteri(m_color, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_color, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_color, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_color, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glNamedFramebufferTexture(m_fbo, GL_COLOR_ATTACHMENT0, m_color, 0);

        if (m_withDepth)
        {
            glCreateRenderbuffers(1, &m_depth);
            glNamedRenderbufferStorage(m_depth, GL_DEPTH_COMPONENT32F, m_width, m_height);
            glNamedFramebufferRenderbuffer(m_fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth);
        }

        if (glCheckNamedFramebufferStatus(m_fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            spdlog::error("Framebuffer {}x{} is incomplete", m_width, m_height);
    }

    void Framebuffer::Destroy()
    {
        if (m_depth)
            glDeleteRenderbuffers(1, &m_depth);
        if (m_color)
            glDeleteTextures(1, &m_color);
        if (m_fbo)
            glDeleteFramebuffers(1, &m_fbo);
        m_depth = m_color = m_fbo = 0;
    }

    void Framebuffer::Resize(int width, int height)
    {
        if (width == m_width && height == m_height)
            return;
        Destroy();
        m_width = width;
        m_height = height;
        Create();
    }

    void Framebuffer::Bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, m_width, m_height);
    }

    void Framebuffer::BindScreen(int width, int height)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
    }

    void Framebuffer::Clear(const glm::vec4 &color) const
    {
        glClearNamedFramebufferfv(m_fbo, GL_COLOR, 0, &color[0]);
        if (m_withDepth)
        {
            const float one = 1.0f;
            glClearNamedFramebufferfv(m_fbo, GL_DEPTH, 0, &one);
        }
    }

    void Framebuffer::BlitTo(const Framebuffer &dst, bool includeDepth) const
    {
        const GLbitfield mask = GL_COLOR_BUFFER_BIT | (includeDepth && m_withDepth && dst.m_withDepth ? GL_DEPTH_BUFFER_BIT : 0);
        glBlitNamedFramebuffer(m_fbo, dst.m_fbo, 0, 0, m_width, m_height, 0, 0, dst.m_width, dst.m_height, mask,
                               (mask & GL_DEPTH_BUFFER_BIT) ? GL_NEAREST : GL_LINEAR);
    }

    void Framebuffer::BlitToScreen(int screenWidth, int screenHeight) const
    {
        glBlitNamedFramebuffer(m_fbo, 0, 0, 0, m_width, m_height, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }

} // namespace gfx
