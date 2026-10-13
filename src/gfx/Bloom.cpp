#include "gfx/Bloom.h"
#include <algorithm>

namespace gfx
{

    Bloom::Bloom(int width, int height, int levels)
        : m_width(width), m_height(height), m_levels(levels),
          m_down(SHADER_DIR "fullscreen.vert", SHADER_DIR "bloom_down.frag"),
          m_up(SHADER_DIR "fullscreen.vert", SHADER_DIR "bloom_up.frag")
    {
        Build();
    }

    void Bloom::Resize(int width, int height)
    {
        if (width == m_width && height == m_height)
            return;
        m_width = width;
        m_height = height;
        Build();
    }

    void Bloom::Build()
    {
        m_mips.clear();
        for (int i = 0; i < m_levels; ++i)
        {
            const int w = std::max(1, m_width >> (i + 1));
            const int h = std::max(1, m_height >> (i + 1));
            m_mips.push_back(std::make_unique<Framebuffer>(w, h, GL_RGBA16F, false));
        }
    }

    GLuint Bloom::Process(GLuint sceneColor, float threshold, float radius)
    {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        m_vao.Bind();

        m_down.Bind();
        m_down.SetFloat("uThreshold", threshold);
        GLuint src = sceneColor;
        int srcW = m_width, srcH = m_height;
        for (size_t i = 0; i < m_mips.size(); ++i)
        {
            m_mips[i]->Bind();
            m_down.SetInt("uFirst", i == 0 ? 1 : 0);
            m_down.SetVec2("uTexelSize", {1.0f / static_cast<float>(srcW), 1.0f / static_cast<float>(srcH)});
            glBindTextureUnit(0, src);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            src = m_mips[i]->ColorTexture();
            srcW = m_mips[i]->Width();
            srcH = m_mips[i]->Height();
        }

        m_up.Bind();
        m_up.SetFloat("uRadius", radius);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        for (size_t i = m_mips.size() - 1; i > 0; --i)
        {
            m_mips[i - 1]->Bind();
            glBindTextureUnit(0, m_mips[i]->ColorTexture());
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        glDisable(GL_BLEND);

        return m_mips[0]->ColorTexture();
    }

} // namespace gfx
