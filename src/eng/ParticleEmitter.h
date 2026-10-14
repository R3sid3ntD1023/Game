#pragma once
#include "eng/Component.h"
#include "gfx/ParticleRenderer.h"
#include <glm/glm.hpp>
#include <random>
#include <vector>

namespace eng {

enum class EmitterType {
    Rain,  // streaks falling through a volume that follows the world camera
    Blood, // droplets that arc out of Burst() calls and splat on the ground
};

// Simulates one particle effect on the GameObject it is attached to and registers with the World,
// which gathers every emitter's particles for the ParticleRenderer each frame.
class ParticleEmitter : public Component {
public:
    explicit ParticleEmitter(EmitterType type) : m_type(type), m_rng(1234u + static_cast<uint32_t>(type)) {}

    void OnAttach() override;
    void OnDestroy() override;
    void OnUpdate(float dt) override;

    // Blood: spray `count` droplets from `position`, biased towards `direction`.
    void Burst(const glm::vec3& position, const glm::vec3& direction, int count);

    void Collect(gfx::ParticleRenderer& renderer);

    bool enabled = true;
    float amount = 1.0f;                    // rain: fraction of drops drawn
    glm::vec3 wind{ 2.5f, 0.0f, 1.0f };     // rain
    float waterLevel = -0.2f;               // rain splashes (and stops) at this height over low ground

private:
    struct Drop {
        glm::vec3 pos;
        float speed;
    };
    struct Blood {
        glm::vec3 pos, vel;
        float life, maxLife, size;
        bool landed;
    };
    struct Splash {
        glm::vec3 pos;
        float age; // 0..1
    };

    float Rand(float a, float b) { return std::uniform_real_distribution<float>(a, b)(m_rng); }
    void SpawnDrop(Drop& d, const glm::vec3& cam, bool fullVolume);

    EmitterType m_type;
    std::mt19937 m_rng;
    std::vector<Drop> m_rain;
    std::vector<Blood> m_blood;
    std::vector<Splash> m_splashes;
    std::vector<gfx::GpuParticle> m_gpu;
    bool m_initialised = false;
};

} // namespace eng
