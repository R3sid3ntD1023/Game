#include "gfx/ShadowMap.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace gfx
{

    namespace
    {
        struct UboData
        {
            glm::mat4 vp[ShadowMap::kCascades];
            glm::vec4 splits;
            glm::vec4 texel;
            glm::vec4 params;
        };
    } // namespace

    ShadowMap::ShadowMap()
    {
        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_tex);
        glTextureStorage3D(m_tex, 1, GL_DEPTH_COMPONENT32F, kSize, kSize, kCascades);
        glTextureParameteri(m_tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(m_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const float border[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTextureParameterfv(m_tex, GL_TEXTURE_BORDER_COLOR, border);
        glTextureParameteri(m_tex, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTextureParameteri(m_tex, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        glCreateFramebuffers(1, &m_fbo);
        glNamedFramebufferDrawBuffer(m_fbo, GL_NONE);
        glNamedFramebufferReadBuffer(m_fbo, GL_NONE);

        glCreateBuffers(1, &m_ubo);
        glNamedBufferStorage(m_ubo, sizeof(UboData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    }

    ShadowMap::~ShadowMap()
    {
        glDeleteTextures(1, &m_tex);
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteBuffers(1, &m_ubo);
    }

    void ShadowMap::Prepare(const Camera &camera, float aspect, const glm::vec3 &lightTravel)
    {
        const float nearP = camera.nearPlane;
        const float farP = std::min(shadowDistance, camera.farPlane);
        const glm::vec3 dir = glm::normalize(lightTravel);
        const glm::vec3 up = std::abs(dir.y) > 0.99f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f), dir, up);
        const glm::mat4 invCam = glm::inverse(camera.Projection(aspect) * camera.View());

        // Frustum corners on the near and far planes, interpolated by view depth for each cascade slice.
        glm::vec3 nearC[4], farC[4];
        int k = 0;
        for (float y : {-1.0f, 1.0f})
            for (float x : {-1.0f, 1.0f})
            {
                glm::vec4 n = invCam * glm::vec4(x, y, -1.0f, 1.0f);
                glm::vec4 f = invCam * glm::vec4(x, y, 1.0f, 1.0f);
                nearC[k] = glm::vec3(n) / n.w;
                farC[k] = glm::vec3(f) / f.w;
                ++k;
            }
        const float camRange = camera.farPlane - camera.nearPlane;

        float prev = nearP;
        for (int c = 0; c < kCascades; ++c)
        {
            const float p = static_cast<float>(c + 1) / kCascades;
            const float logSplit = nearP * std::pow(farP / nearP, p);
            const float uniSplit = nearP + (farP - nearP) * p;
            const float split = splitLambda * logSplit + (1.0f - splitLambda) * uniSplit;

            const float t0 = (prev - camera.nearPlane) / camRange;
            const float t1 = (split - camera.nearPlane) / camRange;
            glm::vec3 corners[8];
            glm::vec3 center(0.0f);
            for (int i = 0; i < 4; ++i)
            {
                const glm::vec3 ray = farC[i] - nearC[i];
                corners[i] = nearC[i] + ray * t0;
                corners[i + 4] = nearC[i] + ray * t1;
                center += corners[i] + corners[i + 4];
            }
            center /= 8.0f;

            float radius = 0.0f;
            for (const glm::vec3 &v : corners)
                radius = std::max(radius, glm::length(v - center));
            radius = std::ceil(radius * 16.0f) / 16.0f;

            const float texel = 2.0f * radius / kSize;
            glm::vec3 cls = glm::vec3(lightView * glm::vec4(center, 1.0f));
            cls.x = std::floor(cls.x / texel) * texel;
            cls.y = std::floor(cls.y / texel) * texel;

            const float pad = 200.0f; // include casters well outside the slice
            const glm::mat4 proj = glm::ortho(cls.x - radius, cls.x + radius, cls.y - radius, cls.y + radius,
                                              -cls.z - pad, -cls.z + radius);
            m_viewProj[c] = proj * lightView;
            m_splits[c] = split;
            m_texel[c] = texel;
            prev = split;
        }
        m_params = glm::vec4(camera.forward, farP);
    }

    void ShadowMap::Begin(int cascade)
    {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_prevFbo);
        glGetIntegerv(GL_VIEWPORT, m_prevViewport);
        glNamedFramebufferTextureLayer(m_fbo, GL_DEPTH_ATTACHMENT, m_tex, 0, cascade);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, kSize, kSize);
        glDepthMask(GL_TRUE);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
    }

    void ShadowMap::End()
    {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_prevFbo));
        glViewport(m_prevViewport[0], m_prevViewport[1], m_prevViewport[2], m_prevViewport[3]);
    }

    void ShadowMap::Bind(bool enabled) const
    {
        UboData data{};
        for (int i = 0; i < kCascades; ++i)
            data.vp[i] = m_viewProj[i];
        data.splits = m_splits;
        data.texel = m_texel;
        data.params = m_params;
        // Disabled: shadow distance 0 makes the shader treat everything as lit.
        if (!enabled)
            data.params.w = 0.0f;
        glNamedBufferSubData(m_ubo, 0, sizeof(UboData), &data);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_ubo);
        glBindTextureUnit(7, m_tex);
    }

} // namespace gfx
