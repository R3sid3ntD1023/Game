#include "gfx/LocalShadowMap.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace gfx
{

    namespace
    {
        struct UboData
        {
            glm::mat4 vp[LocalShadowMap::kSlots * LocalShadowMap::kFaces];
            glm::vec4 info[LocalShadowMap::kSlots]; // x = world texel size per unit distance
        };

        const glm::vec3 kFaceDirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        const glm::vec3 kFaceUps[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};

        constexpr float kPointHalfFov = glm::radians(46.0f); // slightly over 45 so filtering at face edges has data
    } // namespace

    LocalShadowMap::LocalShadowMap()
    {
        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_tex);
        glTextureStorage3D(m_tex, 1, GL_DEPTH_COMPONENT32F, kSize, kSize, kSlots * kFaces);
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

    LocalShadowMap::~LocalShadowMap()
    {
        glDeleteTextures(1, &m_tex);
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteBuffers(1, &m_ubo);
    }

    int LocalShadowMap::Assign(const LightData &light, int &faceCount)
    {
        const int slot = m_used++;
        const float nearP = 0.1f;
        const float farP = std::max(light.range, nearP * 2.0f);

        if (light.type == LightType::Point)
        {
            faceCount = kFaces;
            const glm::mat4 proj = glm::perspective(2.0f * kPointHalfFov, 1.0f, nearP, farP);
            for (int f = 0; f < kFaces; ++f)
                m_viewProj[slot * kFaces + f] = proj * glm::lookAt(light.position, light.position + kFaceDirs[f], kFaceUps[f]);
            m_info[slot] = glm::vec4(2.0f * std::tan(kPointHalfFov) / kSize, 0.0f, 0.0f, 0.0f);
        }
        else
        {
            faceCount = 1;
            const float half = std::min(std::acos(glm::clamp(light.outerCos, -1.0f, 1.0f)) + 0.05f, glm::radians(80.0f));
            const glm::vec3 dir = glm::normalize(light.direction);
            const glm::vec3 up = std::abs(dir.y) > 0.99f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
            m_viewProj[slot * kFaces] = glm::perspective(2.0f * half, 1.0f, nearP, farP) * glm::lookAt(light.position, light.position + dir, up);
            m_info[slot] = glm::vec4(2.0f * std::tan(half) / kSize, 0.0f, 0.0f, 0.0f);
        }
        return slot;
    }

    void LocalShadowMap::Begin(int slot, int face)
    {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_prevFbo);
        glGetIntegerv(GL_VIEWPORT, m_prevViewport);
        glNamedFramebufferTextureLayer(m_fbo, GL_DEPTH_ATTACHMENT, m_tex, 0, slot * kFaces + face);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, kSize, kSize);
        glDepthMask(GL_TRUE);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
    }

    void LocalShadowMap::End()
    {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_prevFbo));
        glViewport(m_prevViewport[0], m_prevViewport[1], m_prevViewport[2], m_prevViewport[3]);
    }

    void LocalShadowMap::Bind() const
    {
        UboData data{};
        for (size_t i = 0; i < m_viewProj.size(); ++i)
            data.vp[i] = m_viewProj[i];
        for (size_t i = 0; i < m_info.size(); ++i)
            data.info[i] = m_info[i];
        glNamedBufferSubData(m_ubo, 0, sizeof(UboData), &data);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_ubo);
        glBindTextureUnit(8, m_tex);
    }

} // namespace gfx
