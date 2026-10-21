#pragma once
#include "gfx/ParticleRenderer.h"
#include <functional>
#include <glm/glm.hpp>
#include <random>
#include <utility>
#include <vector>

namespace eng {

// A constant, or a value picked uniformly at random per particle.
struct Range {
    float min = 1.0f, max = 1.0f;
    Range() = default;
    Range(float v) : min(v), max(v) {}
    Range(float a, float b) : min(a), max(b) {}
};

// Module layout follows Unity's ParticleSystem; only what the game uses is implemented.
// Simulation space is always world space.
class ParticleSystem {
public:
    using GroundQuery = std::function<float(float x, float z)>;

    struct MainModule {
        Range startLifetime{1.0f};
        Range startSpeed{0.0f};
        Range startSize{0.1f};
        glm::vec4 startColor{1.0f};
        float gravityModifier = 0.0f; // multiplier of 9.8 m/s^2
        size_t maxParticles = 1000;
    } main;

    struct EmissionModule {
        bool enabled = true;
        float rateOverTime = 0.0f;
        float rateMultiplier = 1.0f;
    } emission;

    struct ShapeModule {
        enum class Type { Point, Box };
        Type type = Type::Point;
        glm::vec3 offset{0.0f};       // relative to ParticleSystem::position
        glm::vec3 boxSize{1.0f};
        glm::vec3 direction{0.0f, 1.0f, 0.0f}; // start velocity direction
        glm::vec3 randomVelocityMin{0.0f};     // added to the start velocity
        glm::vec3 randomVelocityMax{0.0f};
    } shape;

    struct VelocityOverLifetimeModule {
        bool enabled = false;
        glm::vec3 linear{0.0f}; // constant extra velocity (e.g. wind)
    } velocityOverLifetime;

    struct ColorOverLifetimeModule {
        bool enabled = false;
        std::vector<std::pair<float, glm::vec4>> keys; // (normalized age, colour), sorted by age; multiplies startColor
    } colorOverLifetime;

    struct SizeOverLifetimeModule {
        bool enabled = false;
        float start = 1.0f, end = 1.0f; // linear multiplier over the lifetime
    } sizeOverLifetime;

    struct CollisionModule {
        enum class Response { Kill, Stick };
        bool enabled = false;
        Response response = Response::Kill;
        float minHeight = -1e9f;            // acts as an extra floor (e.g. water level)
        float surfaceOffset = 0.03f;        // particles rest this far above the surface
        float stickSizeMultiplier = 1.0f;   // Stick: size change on landing
        float stickLifetime = 0.0f;         // Stick: seconds the particle lingers (at least)
        ParticleSystem *subEmitter = nullptr; // Kill: emits one particle here on impact
        float subEmitterChance = 1.0f;
        float subEmitterRange = 1e9f;       // only within this distance of ParticleSystem::focusPoint
    } collision;

    struct RendererModule {
        gfx::ParticleMode mode = gfx::ParticleMode::Billboard;
        float velocityScale = 0.05f; // Streak: length per m/s
    } renderer;

    glm::vec3 position{0.0f};   // emitter origin, set by the owning component
    glm::vec3 focusPoint{0.0f}; // reference point for subEmitterRange
    bool playing = true;

    explicit ParticleSystem(uint32_t seed = 1234u) : m_rng(seed) {}

    // Spawns `count` particles at `pos`, moving along `dir` (scaled by startSpeed) plus the shape's random velocity.
    void Emit(const glm::vec3 &pos, const glm::vec3 &dir, int count);
    // Advances the simulation by `seconds` in fixed steps so the system starts already filled.
    void Prewarm(float seconds, const GroundQuery &ground);

    void Update(float dt, const GroundQuery &ground);
    void Collect(gfx::ParticleRenderer &out);

    size_t ParticleCount() const { return m_particles.size(); }
    void Clear() { m_particles.clear(); }

private:
    struct Particle {
        glm::vec3 pos, vel;
        float age, lifetime, size;
        bool stuck;
    };

    float Rand(float a, float b) { return std::uniform_real_distribution<float>(a, b)(m_rng); }
    float Rand(const Range &r) { return r.min == r.max ? r.min : Rand(r.min, r.max); }
    void Spawn(const glm::vec3 &pos, const glm::vec3 &dir);
    glm::vec3 Velocity(const Particle &p) const;
    glm::vec4 EvaluateColor(float t) const;

    std::mt19937 m_rng;
    std::vector<Particle> m_particles;
    std::vector<gfx::GpuParticle> m_gpu;
    float m_emitAccumulator = 0.0f;
};

} // namespace eng
