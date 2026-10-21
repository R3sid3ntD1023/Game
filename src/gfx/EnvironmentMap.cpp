#include "gfx/EnvironmentMap.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <vector>

namespace gfx
{

    namespace
    {

        constexpr int kEnvSize = 512;
        constexpr int kIrradianceSize = 32;
        constexpr int kPrefilterSize = 128;
        constexpr int kPrefilterLevels = 5; // keep in sync with MAX_REFLECTION_LOD in ForwardMesh.frag
        constexpr int kLutSize = 512;

        GLuint CreateCube(int size, int levels)
        {
            GLuint tex = 0;
            glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex);
            glTextureStorage2D(tex, levels, GL_RGBA16F, size, size);
            glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
            glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            return tex;
        }

        int Groups(int size) { return (size + 7) / 8; }

        // Used when the HDR file cannot be loaded: gradient sky, dark ground and a sun disc.
        std::vector<float> ProceduralSky(int w, int h)
        {
            std::vector<float> px(static_cast<size_t>(w) * h * 4);
            const glm::vec3 sunDir = glm::normalize(glm::vec3(-0.4f, 0.8f, 0.3f));
            for (int y = 0; y < h; ++y)
            {
                const float elev = ((static_cast<float>(y) + 0.5f) / static_cast<float>(h) - 0.5f) * 3.14159265f;
                for (int x = 0; x < w; ++x)
                {
                    const float phi = ((static_cast<float>(x) + 0.5f) / static_cast<float>(w) - 0.5f) * 6.2831853f;
                    const glm::vec3 d(std::cos(elev) * std::cos(phi), std::sin(elev), std::cos(elev) * std::sin(phi));
                    glm::vec3 c;
                    if (d.y >= 0.0f)
                        c = glm::mix(glm::vec3(0.8f, 0.85f, 0.9f), glm::vec3(0.25f, 0.45f, 0.85f), std::sqrt(d.y));
                    else
                        c = glm::vec3(0.2f, 0.18f, 0.15f);
                    c += glm::vec3(60.0f, 56.0f, 50.0f) * std::pow(std::max(glm::dot(d, sunDir), 0.0f), 800.0f);
                    float *p = &px[(static_cast<size_t>(y) * w + x) * 4];
                    p[0] = c.r;
                    p[1] = c.g;
                    p[2] = c.b;
                    p[3] = 1.0f;
                }
            }
            return px;
        }

        // Moonlit night: dark gradient, scattered stars and a moon where ForwardMesh.frag puts its moonlight.
        std::vector<float> ProceduralNightSky(int w, int h)
        {
            std::vector<float> px(static_cast<size_t>(w) * h * 4);
            const glm::vec3 moonDir = glm::normalize(glm::vec3(-0.4f, 0.8f, 0.3f));
            for (int y = 0; y < h; ++y)
            {
                const float elev = ((static_cast<float>(y) + 0.5f) / static_cast<float>(h) - 0.5f) * 3.14159265f;
                for (int x = 0; x < w; ++x)
                {
                    const float phi = ((static_cast<float>(x) + 0.5f) / static_cast<float>(w) - 0.5f) * 6.2831853f;
                    const glm::vec3 d(std::cos(elev) * std::cos(phi), std::sin(elev), std::cos(elev) * std::sin(phi));

                    glm::vec3 c = d.y >= 0.0f ? glm::mix(glm::vec3(0.020f, 0.028f, 0.045f), glm::vec3(0.004f, 0.007f, 0.018f), std::sqrt(d.y))
                                              : glm::vec3(0.002f, 0.003f, 0.005f);

                    uint32_t hash = static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u;
                    hash ^= hash >> 13;
                    hash *= 0x5bd1e995u;
                    hash ^= hash >> 15;
                    if (d.y > 0.05f && (hash & 0xFFFFu) > 65400u)
                    {
                        const float b = 0.4f + 2.5f * static_cast<float>((hash >> 16) & 0xFFu) / 255.0f;
                        c += glm::vec3(b * 0.9f, b * 0.95f, b);
                    }

                    const float md = std::max(glm::dot(d, moonDir), 0.0f);
                    c += glm::vec3(6.0f, 6.5f, 8.0f) * glm::smoothstep(0.9988f, 0.9992f, md);
                    c += glm::vec3(0.5f, 0.6f, 0.9f) * 0.12f * std::pow(md, 120.0f);

                    float *p = &px[(static_cast<size_t>(y) * w + x) * 4];
                    p[0] = c.r;
                    p[1] = c.g;
                    p[2] = c.b;
                    p[3] = 1.0f;
                }
            }
            return px;
        }

    } // namespace

    GLuint EnvironmentMap::LoadEquirect(const std::string &path) const
    {
        int w = 0, h = 0, n = 0;
        stbi_set_flip_vertically_on_load(true); // row 0 = bottom, so v = 1 is up
        float *data = path.empty() ? nullptr : stbi_loadf(path.c_str(), &w, &h, &n, 4);

        std::vector<float> fallback;
        const float *pixels = data;
        if (!data)
        {
            if (path.empty())
            {
                w = 1024;
                h = 512;
                fallback = ProceduralNightSky(w, h);
            }
            else
            {
                spdlog::warn("Could not load HDR '{}' ({}); using procedural sky", path, stbi_failure_reason());
                w = 256;
                h = 128;
                fallback = ProceduralSky(w, h);
            }
            pixels = fallback.data();
        }
        else
        {
            spdlog::info("Loaded HDR '{}' ({}x{})", path, w, h);
        }

        GLuint tex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &tex);
        glTextureStorage2D(tex, 1, GL_RGBA16F, w, h);
        glTextureSubImage2D(tex, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, pixels);
        glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (data)
            stbi_image_free(data);
        return tex;
    }

    EnvironmentMap::EnvironmentMap(const std::string &hdrPath) : m_sky(SHADER_DIR "sky.vert", SHADER_DIR "sky.frag")
    {
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        m_procedural = hdrPath.empty();

        int envLevels = 1;
        for (int s = kEnvSize; s > 1; s >>= 1)
            ++envLevels;
        m_env = CreateCube(kEnvSize, envLevels);
        m_irradiance = CreateCube(kIrradianceSize, 1);
        m_prefilter = CreateCube(kPrefilterSize, kPrefilterLevels);

        if (m_procedural)
        {
            m_sunDir = glm::normalize(glm::vec3(0.4f, 0.8f, -0.3f));
            m_bakedSunDir = m_sunDir;
            BakeProceduralCube();
        }
        else
        {
            const GLuint equirect = LoadEquirect(hdrPath);
            Shader cs(SHADER_DIR "equirect_to_cube.comp");
            cs.Bind();
            glBindTextureUnit(0, equirect);
            glBindImageTexture(0, m_env, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
            glDispatchCompute(Groups(kEnvSize), Groups(kEnvSize), 6);
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
            glGenerateTextureMipmap(m_env);
            glDeleteTextures(1, &equirect);
        }
        BuildLighting();

        // BRDF integration LUT (independent of the sky)
        glCreateTextures(GL_TEXTURE_2D, 1, &m_brdf);
        glTextureStorage2D(m_brdf, 1, GL_RG16F, kLutSize, kLutSize);
        glTextureParameteri(m_brdf, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_brdf, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_brdf, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_brdf, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        {
            Shader cs(SHADER_DIR "brdf_lut.comp");
            cs.Bind();
            glBindImageTexture(0, m_brdf, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
            glDispatchCompute(Groups(kLutSize), Groups(kLutSize), 1);
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
        }
    }

    void EnvironmentMap::BakeProceduralCube()
    {
        static const glm::vec3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        static const glm::vec3 ups[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};

        GLint prevFbo = 0;
        GLint prevViewport[4];
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
        glGetIntegerv(GL_VIEWPORT, prevViewport);
        const GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
        const GLboolean cull = glIsEnabled(GL_CULL_FACE);

        GLuint fbo = 0;
        glCreateFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, kEnvSize, kEnvSize);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        m_sky.Bind();
        m_sky.SetInt("uProcedural", 1);
        m_sky.SetInt("uBake", 1);
        m_sky.SetVec3("uSunDir", m_sunDir);
        m_sky.SetVec3("uCameraPos", glm::vec3(0.0f));
        m_vao.Bind();
        for (int face = 0; face < 6; ++face)
        {
            glNamedFramebufferTextureLayer(fbo, GL_COLOR_ATTACHMENT0, m_env, 0, face);
            m_sky.SetMat4("uInvViewProj", glm::inverse(proj * glm::lookAt(glm::vec3(0.0f), dirs[face], ups[face])));
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
        glDeleteFramebuffers(1, &fbo);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
        if (depthTest)
            glEnable(GL_DEPTH_TEST);
        if (cull)
            glEnable(GL_CULL_FACE);
        glGenerateTextureMipmap(m_env);
    }

    void EnvironmentMap::SetSunDirection(const glm::vec3 &toSun)
    {
        if (!m_procedural)
            return;
        m_sunDir = glm::normalize(toSun);
        if (glm::dot(m_sunDir, m_bakedSunDir) < 0.99966f) // ~1.5 degrees
        {
            m_bakedSunDir = m_sunDir;
            BakeProceduralCube();
            BuildLighting();
        }
    }

    void EnvironmentMap::BuildLighting()
    {
        if (!m_irradianceCs)
            m_irradianceCs = std::make_unique<Shader>(SHADER_DIR "irradiance.comp");
        if (!m_prefilterCs)
            m_prefilterCs = std::make_unique<Shader>(SHADER_DIR "prefilter.comp");

        // Diffuse irradiance
        {
            Shader &cs = *m_irradianceCs;
            cs.Bind();
            glBindTextureUnit(0, m_env);
            glBindImageTexture(0, m_irradiance, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
            glDispatchCompute(Groups(kIrradianceSize), Groups(kIrradianceSize), 6);
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
        }

        // Specular prefilter, one roughness per mip
        {
            Shader &cs = *m_prefilterCs;
            cs.Bind();
            cs.SetFloat("uEnvResolution", static_cast<float>(kEnvSize));
            glBindTextureUnit(0, m_env);
            for (int mip = 0; mip < kPrefilterLevels; ++mip)
            {
                const int size = kPrefilterSize >> mip;
                cs.SetFloat("uRoughness", static_cast<float>(mip) / static_cast<float>(kPrefilterLevels - 1));
                glBindImageTexture(0, m_prefilter, mip, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
                glDispatchCompute(Groups(size), Groups(size), 6);
            }
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
        }
    }

    EnvironmentMap::~EnvironmentMap()
    {
        glDeleteTextures(1, &m_env);
        glDeleteTextures(1, &m_irradiance);
        glDeleteTextures(1, &m_prefilter);
        glDeleteTextures(1, &m_brdf);
    }

    void EnvironmentMap::BindForLighting() const
    {
        glBindTextureUnit(3, m_irradiance);
        glBindTextureUnit(4, m_prefilter);
        glBindTextureUnit(5, m_brdf);
        glBindTextureUnit(6, m_env);
    }

    void EnvironmentMap::DrawSky(const glm::mat4 &view, const glm::mat4 &proj, const glm::vec3 &cameraPos) const
    {
        m_sky.Bind();
        m_sky.SetMat4("uInvViewProj", glm::inverse(proj * view));
        m_sky.SetVec3("uCameraPos", cameraPos);
        m_sky.SetInt("uProcedural", m_procedural ? 1 : 0);
        m_sky.SetInt("uBake", 0);
        m_sky.SetVec3("uSunDir", m_sunDir);
        glBindTextureUnit(6, m_env);
        m_vao.Bind();
        glDepthFunc(GL_LEQUAL);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDepthFunc(GL_LESS);
    }

} // namespace gfx
