#include "eng/ParticleEmitter.h"
#include "eng/GameObject.h"
#include "eng/World.h"
#include <algorithm>
#include <cmath>

namespace eng
{

    namespace
    {
        constexpr size_t kRainCount = 4500;
        constexpr size_t kMaxBlood = 1500;
        constexpr size_t kMaxSplashes = 500;
        constexpr float kRadius = 26.0f;      // rain volume half-width around the camera
        constexpr float kTop = 26.0f;         // rain volume height above the camera
        constexpr float kSplashRange = 22.0f; // splashes are only simulated this close to the camera
        constexpr float kSplashLife = 0.35f;  // seconds
    } // namespace

    void ParticleEmitter::OnAttach() { gameObject->GetWorld().RegisterEmitter(this); }
    void ParticleEmitter::OnDestroy() { gameObject->GetWorld().UnregisterEmitter(this); }

    void ParticleEmitter::SpawnDrop(Drop &d, const glm::vec3 &cam, bool fullVolume)
    {
        d.pos = {cam.x + Rand(-kRadius, kRadius), cam.y + (fullVolume ? Rand(-8.0f, kTop) : Rand(kTop - 14.0f, kTop)),
                 cam.z + Rand(-kRadius, kRadius)};
        d.speed = Rand(16.0f, 24.0f);
    }

    void ParticleEmitter::Burst(const glm::vec3 &position, const glm::vec3 &direction, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const glm::vec3 scatter(Rand(-1.0f, 1.0f), Rand(0.0f, 1.0f), Rand(-1.0f, 1.0f));
            Blood b;
            b.pos = position;
            b.vel = direction * Rand(1.5f, 5.0f) + scatter * Rand(0.8f, 2.5f) + glm::vec3(0.0f, Rand(0.5f, 2.5f), 0.0f);
            b.life = b.maxLife = Rand(1.2f, 2.5f);
            b.size = Rand(0.04f, 0.1f);
            b.landed = false;
            if (m_blood.size() >= kMaxBlood)
                m_blood.erase(m_blood.begin());
            m_blood.push_back(b);
        }
    }

    void ParticleEmitter::OnUpdate(float dt)
    {
        const PhysicsWorld &physics = gameObject->GetWorld().Physics();

        if (m_type == EmitterType::Rain)
        {
            if (!enabled)
                return;
            const glm::vec3 cam = gameObject->GetWorld().camera.position;
            if (!m_initialised)
            {
                m_rain.resize(kRainCount);
                for (Drop &d : m_rain)
                    SpawnDrop(d, cam, true);
                m_initialised = true;
            }

            for (Drop &d : m_rain)
            {
                d.pos += (wind + glm::vec3(0.0f, -d.speed, 0.0f)) * dt;

                // Keep the volume centered on the camera by wrapping drops that drift out horizontally.
                const float dx = d.pos.x - cam.x, dz = d.pos.z - cam.z;
                if (dx > kRadius)
                    d.pos.x -= 2.0f * kRadius;
                else if (dx < -kRadius)
                    d.pos.x += 2.0f * kRadius;
                if (dz > kRadius)
                    d.pos.z -= 2.0f * kRadius;
                else if (dz < -kRadius)
                    d.pos.z += 2.0f * kRadius;

                const float surface = std::max(physics.GroundHeight(d.pos.x, d.pos.z, -1e9f), waterLevel);
                if (d.pos.y < surface)
                {
                    // Splash on impact, thinned out and limited to nearby drops.
                    if (m_splashes.size() < kMaxSplashes && dx * dx + dz * dz < kSplashRange * kSplashRange && Rand(0.0f, 1.0f) < 0.35f)
                        m_splashes.push_back({{d.pos.x, surface + 0.03f, d.pos.z}, 0.0f});
                    SpawnDrop(d, cam, false);
                }
                else if (d.pos.y < cam.y - 14.0f)
                {
                    SpawnDrop(d, cam, false); // fell far below the camera over a drop-off
                }
            }

            for (size_t i = 0; i < m_splashes.size();)
            {
                m_splashes[i].age += dt / kSplashLife;
                if (m_splashes[i].age >= 1.0f)
                {
                    m_splashes[i] = m_splashes.back();
                    m_splashes.pop_back();
                }
                else
                {
                    ++i;
                }
            }
            return;
        }

        for (size_t i = 0; i < m_blood.size();)
        {
            Blood &b = m_blood[i];
            if (!b.landed)
            {
                b.vel.y -= 9.8f * dt;
                b.pos += b.vel * dt;
                const float ground = physics.GroundHeight(b.pos.x, b.pos.z, -1e9f) + 0.03f;
                if (b.pos.y <= ground)
                {
                    b.pos.y = ground;
                    b.vel = glm::vec3(0.0f);
                    b.landed = true;
                    b.size *= 2.2f; // splat
                    b.life = b.maxLife = std::max(b.life, 4.0f);
                }
            }
            b.life -= dt;
            if (b.life <= 0.0f)
            {
                b = m_blood.back();
                m_blood.pop_back();
            }
            else
            {
                ++i;
            }
        }
    }

    void ParticleEmitter::Collect(gfx::ParticleRenderer &renderer)
    {
        m_gpu.clear();
        if (m_type == EmitterType::Rain)
        {
            if (!enabled)
                return;
            const size_t count = static_cast<size_t>(static_cast<float>(m_rain.size()) * std::clamp(amount, 0.0f, 1.0f));
            for (size_t i = 0; i < count && i < m_rain.size(); ++i)
            {
                const Drop &d = m_rain[i];
                const glm::vec3 v = wind + glm::vec3(0.0f, -d.speed, 0.0f);
                const float speed = glm::length(v);
                m_gpu.push_back({{d.pos, 0.001f}, {v / speed, speed * 0.05f}, {0.55f, 0.62f, 0.75f, 0.22f}});
            }
            renderer.Submit(m_gpu, gfx::ParticleMode::Streak);

            m_gpu.clear();
            for (const Splash &s : m_splashes)
                m_gpu.push_back({{s.pos, 0.06f + 0.3f * s.age}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.7f, 0.78f, 0.9f, 0.4f * (1.0f - s.age)}});
            renderer.Submit(m_gpu, gfx::ParticleMode::Ring);
        }
        else
        {
            for (const Blood &b : m_blood)
            {
                const float fade = std::clamp(b.life / std::min(b.maxLife, 1.5f), 0.0f, 1.0f);
                m_gpu.push_back({{b.pos, b.size}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.32f, 0.01f, 0.01f, 0.9f * fade}});
            }
            renderer.Submit(m_gpu, gfx::ParticleMode::Billboard);
        }
    }

} // namespace eng
