#pragma once
#include "gfx/Shader.h"
#include "gfx/VertexArray.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
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

        // Procedural skies (empty HDR path) are analytic and follow the sun; HDR skies ignore it.
        bool IsProcedural() const { return m_procedural; }
        // `toSun` points from the viewer towards the sun (the moon is opposite). The image-based lighting is
        // re-baked once the sun has moved about 1.5 degrees since the last bake.
        void SetSunDirection(const glm::vec3 &toSun);
        // Clouds: `overcast` and `dark` are 0..1, `time` drifts the layer, `flash` is lightning brightness.
        // The lighting cube is re-baked when overcast/dark change noticeably, so the ambient light follows.
        void SetWeather(float overcast, float dark, float time, float flash);

    private:
        GLuint LoadEquirect(const std::string &path) const;
        void BakeProceduralCube();
        void BuildLighting();

        bool m_procedural = false;
        glm::vec3 m_sunDir{0.0f, 1.0f, 0.0f};
        glm::vec3 m_bakedSunDir{0.0f, -1.0f, 0.0f};
        float m_overcast = 0.0f, m_dark = 0.0f, m_time = 0.0f, m_flash = 0.0f;
        float m_bakedOvercast = 0.0f, m_bakedDark = 0.0f;
        GLuint m_env = 0;
        GLuint m_irradiance = 0;
        GLuint m_prefilter = 0;
        GLuint m_brdf = 0;
        Shader m_sky;
        std::unique_ptr<Shader> m_irradianceCs; // compiled on first BuildLighting
        std::unique_ptr<Shader> m_prefilterCs;
        VertexArray m_vao;
    };

} // namespace gfx
