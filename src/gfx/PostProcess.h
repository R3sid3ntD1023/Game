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
    GLuint DepthTexture() const { return m_scene.DepthTexture(); } // scene depth, valid after the depth prepass

    GLuint outputFbo = 0; // composite target; 0 = the screen
    bool bloomEnabled = true;
    bool showRaw = false; // blit the HDR buffer directly, no tone mapping
    float threshold = 1.5f;
    float intensity = 0.15f;
    float radius = 0.005f;
    float exposure = 1.6f;
    float vignette = 0.6f;
    float grain = 0.035f;
    float time = 0.0f; // seconds; drives film grain
    // Underwater look: 0 = above water, 1 = fully submerged (the caller eases it). Needs the camera clip planes
    // to turn scene depth into distance for the fog.
    float underwater = 0.0f;
    float nearPlane = 0.1f;
    float farPlane = 800.0f;

private:
    Framebuffer m_scene;
    Bloom m_bloom;
    Shader m_composite;
    VertexArray m_vao;
};

} // namespace gfx
