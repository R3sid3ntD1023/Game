#pragma once
#include "gfx/Framebuffer.h"
#include "gfx/Shader.h"
#include "gfx/VertexArray.h"
#include <memory>
#include <vector>

namespace gfx {

// Dual-filter bloom: threshold + progressive downsample, then additive tent-filter upsample.
class Bloom {
public:
    Bloom(int width, int height, int levels = 6);

    void Resize(int width, int height);

    // Returns a texture containing the blurred bright areas of sceneColor (valid until the next call).
    GLuint Process(GLuint sceneColor, float threshold, float radius);

private:
    void Build();

    int m_width;
    int m_height;
    int m_levels;
    std::vector<std::unique_ptr<Framebuffer>> m_mips;
    Shader m_down;
    Shader m_up;
    VertexArray m_vao;
};

} // namespace gfx
