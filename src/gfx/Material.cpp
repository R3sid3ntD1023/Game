#include "gfx/Material.h"
#include <atomic>

namespace gfx
{

    namespace
    {
        std::atomic<uint32_t> g_nextId{1};

        // Default textures are shared; each upload is a pixel transfer the driver may sync with rendering.
        std::shared_ptr<Texture> SharedColor(std::weak_ptr<Texture> &cache, const glm::vec4 &color, bool srgb)
        {
            if (auto t = cache.lock())
                return t;
            auto t = Texture::FromColor(color, srgb);
            cache = t;
            return t;
        }

        std::weak_ptr<Texture> g_white, g_whiteLinear, g_flatNormal;
    }

    Material::Material(std::shared_ptr<Shader> shader) : m_id(g_nextId++), m_shader(std::move(shader))
    {
        albedoMap = SharedColor(g_white, {1, 1, 1, 1}, true);
        normalMap = SharedColor(g_flatNormal, {0.5f, 0.5f, 1.0f, 1}, false);
        metalRoughMap = SharedColor(g_whiteLinear, {1, 1, 1, 1}, false);
    }

    void Material::Bind() const
    {
        m_shader->Bind();
        albedoMap->Bind(0);
        normalMap->Bind(1);
        metalRoughMap->Bind(2);
        m_shader->SetInt("uAlbedoMap", 0);
        m_shader->SetInt("uNormalMap", 1);
        m_shader->SetInt("uMetalRoughMap", 2);
        m_shader->SetVec4("uBaseColor", baseColor);
        m_shader->SetFloat("uMetallic", metallic);
        m_shader->SetFloat("uRoughness", roughness);
        m_shader->SetFloat("uAO", ao);
        m_shader->SetVec3("uEmissive", emissive);
    }

} // namespace gfx
