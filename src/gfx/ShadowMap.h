#pragma once
#include "gfx/Camera.h"
#include <array>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace gfx
{

    // Cascaded shadow map for one directional light. Cascades are fitted to slices of the camera frustum and
    // snapped to shadow texels so shadows don't shimmer as the camera moves.
    class ShadowMap
    {
    public:
        static constexpr int kCascades = 3;

        ShadowMap();
        ~ShadowMap();
        ShadowMap(const ShadowMap &) = delete;
        ShadowMap &operator=(const ShadowMap &) = delete;

        // `lightTravel` is the direction the light moves (from the light towards the scene).
        void Prepare(const Camera &camera, float aspect, const glm::vec3 &lightTravel);

        // Renders into one cascade; call End() afterwards. Clears the cascade's depth.
        void Begin(int cascade);
        void End();
        const glm::mat4 &ViewProj(int cascade) const { return m_viewProj[cascade]; }

        // Uploads the cascade data (UBO binding 0) and binds the depth array to texture unit 7.
        void Bind(bool enabled) const;

        float shadowDistance = 150.0f;
        float splitLambda = 0.75f;

    private:
        static constexpr int kSize = 2048;

        GLuint m_tex = 0;
        GLuint m_fbo = 0;
        GLuint m_ubo = 0;
        GLint m_prevFbo = 0;
        GLint m_prevViewport[4] = {};

        std::array<glm::mat4, kCascades> m_viewProj{};
        glm::vec4 m_splits{0.0f};
        glm::vec4 m_texel{0.0f};
        glm::vec4 m_params{0.0f};
    };

} // namespace gfx
