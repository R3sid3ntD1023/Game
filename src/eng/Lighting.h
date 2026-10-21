#pragma once
#include "eng/Component.h"
#include "gfx/Lights.h"
#include <glm/glm.hpp>
#include <random>

namespace eng
{

    // Publishes an ECS `Light` at the GameObject's Transform (position, and -Z as the direction).
    // World gathers lights each frame and the Forward+ pass culls them per screen tile.
    class LightComponent : public Component
    {
    public:
        LightComponent(gfx::LightType type, const glm::vec3 &color, float intensity, float range, float innerDegrees = 12.0f, float outerDegrees = 22.0f)
            : type(type), color(color), intensity(intensity), range(range), innerDegrees(innerDegrees), outerDegrees(outerDegrees) {}

        void OnAttach() override;
        void SetEnabled(bool enabled);
        void SetCastShadow(bool cast);

        gfx::LightType type;
        glm::vec3 color;
        float intensity;
        float range;
        float innerDegrees;
        float outerDegrees;
        bool castShadow = false; // set before the component is attached
    };

    class PointLightComponent : public LightComponent
    {
    public:
        PointLightComponent(const glm::vec3 &color, float intensity, float range)
            : LightComponent(gfx::LightType::Point, color, intensity, range) {}
    };

    class SpotLightComponent : public LightComponent
    {
    public:
        SpotLightComponent(const glm::vec3 &color, float intensity, float range, float innerDegrees, float outerDegrees)
            : LightComponent(gfx::LightType::Spot, color, intensity, range, innerDegrees, outerDegrees) {}
    };

    // Shines along the object's -Z axis from everywhere (sun/moon); range is unused.
    class DirectionalLightComponent : public LightComponent
    {
    public:
        DirectionalLightComponent(const glm::vec3 &color, float intensity)
            : LightComponent(gfx::LightType::Directional, color, intensity, 0.0f) {}
    };

    // Keeps the object at an offset from the camera, facing where the camera looks (flashlight, view models).
    class CameraFollower : public Component
    {
    public:
        explicit CameraFollower(const glm::vec3 &offset = glm::vec3(0.0f)) : offset(offset) {}

        void OnUpdate(float dt) override;

        glm::vec3 offset; // x right, y up, z forward
    };

    // Horror-style failing light: slow hum plus random bursts of stuttering and brief blackouts.
    // Also drives the emissive brightness of the object so the lamp itself flickers with its light.
    class LightFlicker : public Component
    {
    public:
        LightFlicker(float amount, uint32_t seed) : m_amount(amount), m_rng(seed) {}

        void OnStart() override;
        void OnUpdate(float dt) override;

    private:
        float Rand(float a, float b) { return std::uniform_real_distribution<float>(a, b)(m_rng); }

        float m_amount;
        float m_base = 1.0f;
        float m_time = 0.0f;
        float m_timer = 1.0f;
        float m_stutter = 0.0f;
        float m_burstLevel = 1.0f;
        bool m_burst = false;
        std::mt19937 m_rng;
    };

} // namespace eng
