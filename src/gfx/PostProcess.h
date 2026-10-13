#pragma once
#include "gfx/Bloom.h"
#include "gfx/Framebuffer.h"
#include "gfx/Shader.h"
#include "gfx/VertexArray.h"

namespace gfx {

// HDR scene target + bloom + tone-mapped composite to the screen.
class PostProcess {
public:
    PostProcess(int width, int height);

    void Resize(int width, int height);
    void BeginScene();
    void EndScene(int screenWidth, int screenHeight);

    bool bloomEnabled = true;
    bool showRaw = false; // blit the HDR buffer directly, no tone mapping
    float threshold = 1.0f;
    float intensity = 0.5f;
    float radius = 0.005f;
    float exposure = 1.0f;

private:
    Framebuffer m_scene;
    Bloom m_bloom;
    Shader m_composite;
    VertexArray m_vao;
};

} // namespace gfx
