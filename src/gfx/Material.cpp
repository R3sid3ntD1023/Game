#include "gfx/Material.h"
#include <atomic>

namespace gfx
{

    namespace
    {
        std::atomic<uint32_t> g_nextId{1};
    }

    Material::Material(std::shared_ptr<Shader> shader) : m_id(g_nextId++), m_shader(std::move(shader))
    {
        albedoMap = Texture::FromColor({1, 1, 1, 1}, true);
        normalMap = Texture::FromColor({0.5f, 0.5f, 1.0f, 1}, false);
        metalRoughMap = Texture::FromColor({1, 1, 1, 1}, false);
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
