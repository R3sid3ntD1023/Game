#pragma once
#include "gfx/Shader.h"
#include "gfx/Texture.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>

namespace gfx {

// Metallic-roughness PBR material. Texture units: 0 albedo, 1 normal, 2 metallic(B)/roughness(G).
class Material {
public:
    explicit Material(std::shared_ptr<Shader> shader);

    void Bind() const;

    uint32_t Id() const { return m_id; }
    const std::shared_ptr<Shader>& GetShader() const { return m_shader; }

    std::shared_ptr<Texture> albedoMap;
    std::shared_ptr<Texture> normalMap;
    std::shared_ptr<Texture> metalRoughMap;
    glm::vec4 baseColor{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    glm::vec3 emissive{0.0f};

private:
    uint32_t m_id;
    std::shared_ptr<Shader> m_shader;
};

} // namespace gfx
