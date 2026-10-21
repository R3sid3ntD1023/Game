#pragma once
#include "eng/Component.h"
#include "eng/ParticleSystem.h"
#include <memory>
#include <vector>

namespace eng {

// Owns one or more ParticleSystems on a GameObject, simulates them every frame and registers with
// the World, which gathers their particles and submits them to the ParticleRenderer.
// The first system is the primary one; extra systems (e.g. splashes) are usually sub-emitters.
class ParticleSystemComponent : public Component {
public:
    void OnAttach() override;
    void OnDestroy() override;
    void OnUpdate(float dt) override;

    ParticleSystem& AddSystem();
    ParticleSystem& Primary() { return *m_systems.front(); }
    void Collect(gfx::ParticleRenderer& renderer);
    // Multiplies the emission rate of every emitting system (e.g. rain intensity).
    void SetEmissionScale(float scale);

    bool enabled = true;
    bool followCamera = false; // emitter origin tracks the world camera instead of the GameObject
    float prewarmSeconds = 0.0f;

private:
    std::vector<std::unique_ptr<ParticleSystem>> m_systems;
    bool m_prewarmed = false;
};

// Ready-made setups for the effects the game uses.
// Rain: streaks falling over the whole island, splashing on the ground. Drops are spawned within `areaSize` metres
// of the camera and then live in the world. `amount` is 0..1; the weather system scales it.
void ConfigureRain(ParticleSystemComponent& c, float amount, const glm::vec3& wind, float areaSize = 150.0f);
// Blood: droplets that arc out of Emit() calls and splat on the ground.
void ConfigureBlood(ParticleSystemComponent& c);

} // namespace eng
