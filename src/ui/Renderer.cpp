#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "ui/Renderer.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace ui
{

    namespace
    {

        constexpr int kAtlasW = 1024;
        constexpr int kAtlasH = 512;
        constexpr float kBakePx = 56.0f; // glyphs are baked once and scaled to any size at draw time
        constexpr int kFirstChar = 32;
        constexpr int kCharCount = 96;
        constexpr float kBaseline = 0.8f; // baseline position as a fraction of text size

        std::string FindFontFile()
        {
            const char *candidates[] = {
                ASSETS_DIR "fonts/ui.ttf",
                "C:/Windows/Fonts/segoeui.ttf",
                "C:/Windows/Fonts/arial.ttf",
                "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                "/Library/Fonts/Arial.ttf",
                "/System/Library/Fonts/Supplemental/Arial.ttf",
            };
            for (const char *c : candidates)
                if (std::ifstream(c, std::ios::binary).good())
                    return c;
            throw std::runtime_error("No UI font found. Put a TrueType font at assets/fonts/ui.ttf");
        }

    } // namespace

    Renderer::Renderer() : m_shader(SHADER_DIR "ui.vert", SHADER_DIR "ui.frag")
    {
        const std::string fontPath = FindFontFile();
        std::ifstream file(fontPath, std::ios::binary);
        const std::vector<unsigned char> ttf((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        std::vector<unsigned char> bitmap(static_cast<size_t>(kAtlasW) * kAtlasH, 0);
        std::vector<stbtt_bakedchar> baked(kCharCount);
        const int rows = stbtt_BakeFontBitmap(ttf.data(), 0, kBakePx, bitmap.data(), kAtlasW, kAtlasH, kFirstChar, kCharCount, baked.data());
        if (rows == 0)
            throw std::runtime_error("Failed to bake UI font: " + fontPath);
        spdlog::info("UI font: {}", fontPath);

        // A white block in the corner lets solid rectangles use the same shader and texture as text.
        for (int y = kAtlasH - 4; y < kAtlasH; ++y)
            for (int x = kAtlasW - 4; x < kAtlasW; ++x)
                bitmap[static_cast<size_t>(y) * kAtlasW + x] = 255;
        m_whiteUv = {(kAtlasW - 2.0f) / kAtlasW, (kAtlasH - 2.0f) / kAtlasH};

        for (const stbtt_bakedchar &b : baked)
        {
            Glyph g;
            g.u0 = b.x0 / static_cast<float>(kAtlasW);
            g.v0 = b.y0 / static_cast<float>(kAtlasH);
            g.u1 = b.x1 / static_cast<float>(kAtlasW);
            g.v1 = b.y1 / static_cast<float>(kAtlasH);
            g.xoff = b.xoff;
            g.yoff = b.yoff;
            g.w = static_cast<float>(b.x1 - b.x0);
            g.h = static_cast<float>(b.y1 - b.y0);
            g.advance = b.xadvance;
            m_glyphs.push_back(g);
        }

        glCreateTextures(GL_TEXTURE_2D, 1, &m_atlas);
        glTextureStorage2D(m_atlas, 6, GL_R8, kAtlasW, kAtlasH);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTextureSubImage2D(m_atlas, 0, 0, 0, kAtlasW, kAtlasH, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glGenerateTextureMipmap(m_atlas);
        glTextureParameteri(m_atlas, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(m_atlas, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_atlas, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_atlas, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        m_vbo.Upload(nullptr, 0); // creates the buffer; contents are re-uploaded every frame
        m_vao.SetVertexBuffer(m_vbo, sizeof(Vertex),
                              {{0, 2, static_cast<GLuint>(offsetof(Vertex, pos))},
                               {1, 2, static_cast<GLuint>(offsetof(Vertex, uv))},
                               {2, 4, static_cast<GLuint>(offsetof(Vertex, color))}});
    }

    Renderer::~Renderer()
    {
        if (m_atlas)
            glDeleteTextures(1, &m_atlas);
    }

    void Renderer::Begin(const glm::vec2 &screen)
    {
        m_screen = screen;
        m_vertices.clear();
    }

    void Renderer::End()
    {
        if (m_vertices.empty())
            return;
        m_vbo.Upload(m_vertices.data(), m_vertices.size() * sizeof(Vertex));

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        m_shader.Bind();
        m_shader.SetVec2("uScreen", m_screen);
        glBindTextureUnit(0, m_atlas);
        m_vao.Bind();
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));

        glDisable(GL_BLEND);
    }

    void Renderer::Quad(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &uv0, const glm::vec2 &uv1,
                        const Color &top, const Color &bottom)
    {
        const Vertex tl{{p0.x, p0.y}, {uv0.x, uv0.y}, top};
        const Vertex tr{{p1.x, p0.y}, {uv1.x, uv0.y}, top};
        const Vertex br{{p1.x, p1.y}, {uv1.x, uv1.y}, bottom};
        const Vertex bl{{p0.x, p1.y}, {uv0.x, uv1.y}, bottom};
        m_vertices.insert(m_vertices.end(), {tl, tr, br, tl, br, bl});
    }

    void Renderer::FillRect(const Rect &r, const Color &color) { FillGradient(r, color, color); }

    void Renderer::FillGradient(const Rect &r, const Color &top, const Color &bottom)
    {
        if (r.size.x <= 0.0f || r.size.y <= 0.0f)
            return;
        Quad(r.pos, r.pos + r.size, m_whiteUv, m_whiteUv, top, bottom);
    }

    void Renderer::StrokeRect(const Rect &r, const Color &color, float t)
    {
        FillRect({r.pos, {r.size.x, t}}, color);
        FillRect({{r.pos.x, r.pos.y + r.size.y - t}, {r.size.x, t}}, color);
        FillRect({{r.pos.x, r.pos.y + t}, {t, r.size.y - 2.0f * t}}, color);
        FillRect({{r.pos.x + r.size.x - t, r.pos.y + t}, {t, r.size.y - 2.0f * t}}, color);
    }

    void Renderer::Text(const std::string &text, const glm::vec2 &pos, float size, const Color &color)
    {
        const float s = size / kBakePx;
        float pen = 0.0f;
        for (const unsigned char ch : text)
        {
            const int index = (ch >= kFirstChar && ch < kFirstChar + kCharCount) ? ch - kFirstChar : '?' - kFirstChar;
            const Glyph &g = m_glyphs[index];
            if (g.w > 0.0f && g.h > 0.0f)
            {
                const glm::vec2 p0(std::round(pos.x + (pen + g.xoff) * s), std::round(pos.y + (kBaseline * kBakePx + g.yoff) * s));
                Quad(p0, p0 + glm::vec2(g.w, g.h) * s, {g.u0, g.v0}, {g.u1, g.v1}, color, color);
            }
            pen += g.advance;
        }
    }

    glm::vec2 Renderer::MeasureText(const std::string &text, float size) const
    {
        float pen = 0.0f;
        float h = 0.0f;
        for (const unsigned char ch : text)
        {
            const int index = (ch >= kFirstChar && ch < kFirstChar + kCharCount) ? ch - kFirstChar : '?' - kFirstChar;
            pen += m_glyphs[index].advance;
            h = std::max(h, m_glyphs[index].h);
        }
        return {pen * size / kBakePx, h * size / kBakePx};
    }

} // namespace ui
