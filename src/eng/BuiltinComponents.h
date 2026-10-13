#pragma once
#include "eng/Component.h"
#include "gfx/Material.h"
#include "gfx/Mesh.h"
#include <algorithm>
#include <memory>

namespace eng
{

    // Registers the object with the RenderSystem (via the ECS MeshRenderer component).
    class MeshRendererComponent : public Component
    {
    public:
        MeshRendererComponent(gfx::MeshHandle mesh, std::shared_ptr<gfx::Material> material)
            : mesh(mesh), material(std::move(material)) {}

        void OnAttach() override;

        gfx::MeshHandle mesh;
        std::shared_ptr<gfx::Material> material;
    };

    class Health : public Component
    {
    public:
        explicit Health(float maxHealth = 100.0f) : max(maxHealth), current(maxHealth) {}

        void Damage(float amount) { current = std::max(0.0f, current - amount); }
        void Heal(float amount) { current = std::min(max, current + amount); }
        bool Dead() const { return current <= 0.0f; }

        float max;
        float current;
    };

} // namespace eng
