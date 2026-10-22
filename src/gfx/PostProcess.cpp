#include "gfx/PostProcess.h"

namespace gfx
{

    PostProcess::PostProcess(int width, int height)
        : m_scene(width, height), m_bloom(width, height),
          m_composite(SHADER_DIR "fullscreen.vert", SHADER_DIR "composite.frag") {}

    void PostProcess::Resize(int width, int height)
    {
        m_scene.Resize(width, height);
        m_bloom.Resize(width, height);
    }

    void PostProcess::BeginScene()
    {
        m_scene.Bind();
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        m_scene.Clear({0.005f, 0.008f, 0.015f, 1.0f});
    }

    void PostProcess::EndScene(int screenWidth, int screenHeight)
    {
        if (showRaw)
        {
            m_scene.BlitToScreen(screenWidth, screenHeight);
            return;
        }

        GLuint bloomTex = 0;
        if (bloomEnabled)
            bloomTex = m_bloom.Process(m_scene.ColorTexture(), threshold, radius);

        glBindFramebuffer(GL_FRAMEBUFFER, outputFbo);
        glViewport(0, 0, screenWidth, screenHeight);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        m_composite.Bind();
        m_composite.SetFloat("uBloomIntensity", bloomEnabled ? intensity : 0.0f);
        m_composite.SetFloat("uExposure", exposure);
        m_composite.SetFloat("uVignette", vignette);
        m_composite.SetFloat("uGrain", grain);
        m_composite.SetFloat("uTime", time);
        m_composite.SetFloat("uUnderwater", underwater);
        m_composite.SetFloat("uNear", nearPlane);
        m_composite.SetFloat("uFar", farPlane);
        glBindTextureUnit(0, m_scene.ColorTexture());
        glBindTextureUnit(1, bloomTex);
        glBindTextureUnit(2, m_scene.DepthTexture());
        m_vao.Bind();
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

} // namespace gfx
