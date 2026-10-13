#pragma once
#include "gfx/Shader.h"
#include "gfx/VertexArray.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

namespace gfx
{

    // Equirectangular HDR -> environment cubemap, diffuse irradiance, GGX prefiltered specular and BRDF LUT.
    // All maps are generated once at construction using compute shaders. Falls back to a procedural sky if the file is missing.
    class EnvironmentMap
    {
    public:
        explicit EnvironmentMap(const std::string &hdrPath);
        ~EnvironmentMap();
        EnvironmentMap(const EnvironmentMap &) = delete;
        EnvironmentMap &operator=(const EnvironmentMap &) = delete;

        // Texture units: 3 irradiance, 4 prefiltered specular, 5 BRDF LUT, 6 environment.
        void BindForLighting() const;
        void DrawSky(const glm::mat4 &view, const glm::mat4 &proj, const glm::vec3 &cameraPos) const;

    private:
        GLuint LoadEquirect(const std::string &path) const;

        GLuint m_env = 0;
        GLuint m_irradiance = 0;
        GLuint m_prefilter = 0;
        GLuint m_brdf = 0;
        Shader m_sky;
        VertexArray m_vao;
    };

} // namespace gfx
