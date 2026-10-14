#include "eng/World.h"
#include <algorithm>
#include <cmath>

namespace eng
{

    World::World() : m_renderer(m_meshes) {}

    World::~World()
    {
        for (auto &o : m_objects)
            o->Shutdown();
        m_objects.clear();
    }

    GameObject &World::CreateGameObject(const std::string &name, const glm::vec3 &position)
    {
        const entt::entity e = m_registry.create();
        m_registry.emplace<Transform>(e, Transform{position});
        m_objects.push_back(std::make_unique<GameObject>(*this, e, name));
        return *m_objects.back();
    }

    GameObject *World::Find(const std::string &name) const
    {
        for (const auto &o : m_objects)
            if (o->IsAlive() && o->name == name)
                return o.get();
        return nullptr;
    }

    void World::UnregisterEmitter(ParticleEmitter *e)
    {
        m_emitters.erase(std::remove(m_emitters.begin(), m_emitters.end(), e), m_emitters.end());
        if (m_bloodEmitter == e)
            m_bloodEmitter = nullptr;
    }

    void World::EmitBlood(const glm::vec3 &position, const glm::vec3 &direction, int count)
    {
        if (!m_bloodEmitter)
            m_bloodEmitter = &CreateGameObject("BloodFX").AddComponent<ParticleEmitter>(EmitterType::Blood);
        m_bloodEmitter->Burst(position, direction, count);
    }

    void World::Update(float dt)
    {
        for (size_t i = 0; i < m_objects.size(); ++i)
            m_objects[i]->Update(dt);

        m_accumulator = std::min(m_accumulator + dt, 0.25f);
        while (m_accumulator >= kFixedStep)
        {
            for (size_t i = 0; i < m_objects.size(); ++i)
                m_objects[i]->FixedUpdate(kFixedStep);
            m_physics.Step(kFixedStep);
            m_accumulator -= kFixedStep;
        }

        for (auto &o : m_objects)
        {
            if (o->IsAlive())
                continue;
            o->Shutdown();
            m_registry.destroy(o->Entity());
        }
        m_objects.erase(std::remove_if(m_objects.begin(), m_objects.end(), [](const auto &o)
                                       { return !o->IsAlive(); }),
                        m_objects.end());

        if (audio)
            audio->SetListener(camera.position, camera.forward, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void World::Render(float aspect, GLuint depthTexture, int width, int height)
    {
        const glm::mat4 view = camera.View();
        const glm::mat4 proj = camera.Projection(aspect);
        const glm::mat4 viewProj = proj * view;

        m_renderer.Build(m_registry);
        m_renderer.DepthPass(viewProj);

        // Directional lights first, then the nearest local lights up to the Forward+ budget.
        std::vector<gfx::LightData> lights;
        {
            struct Candidate
            {
                float d2;
                gfx::LightData data;
            };
            std::vector<Candidate> found;
            m_registry.view<Transform, Light>().each([&](const Transform &t, const Light &l)
                                                     {
                if (!l.enabled || l.intensity <= 0.001f)
                    return;
                gfx::LightData d;
                d.type = l.type;
                d.position = t.position;
                d.range = l.range;
                d.color = l.color * l.intensity;
                d.direction = t.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                d.innerCos = std::cos(glm::radians(l.innerDegrees));
                d.outerCos = std::cos(glm::radians(l.outerDegrees));

                const glm::vec3 to = t.position - camera.position;
                const float d2 = l.type == gfx::LightType::Directional ? -1.0f : glm::dot(to, to);
                if (d2 < 200.0f * 200.0f)
                    found.push_back({d2, d}); });
            std::sort(found.begin(), found.end(), [](const Candidate &a, const Candidate &b)
                      { return a.d2 < b.d2; });
            for (size_t i = 0; i < found.size() && i < static_cast<size_t>(gfx::kMaxLights); ++i)
                lights.push_back(found[i].data);
        }
        m_forwardPlus.Cull(lights, depthTexture, view, proj, width, height);

        if (environment)
            environment->BindForLighting();
        m_renderer.ShadePass(viewProj, camera.position, m_forwardPlus.TilesX());
        if (environment)
            environment->DrawSky(view, proj, camera.position);

        m_particleRenderer.Begin();
        for (ParticleEmitter *e : m_emitters)
            e->Collect(m_particleRenderer);
        m_particleRenderer.Render(view, proj, camera.position);
    }

} // namespace eng
