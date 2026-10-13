#include <glad/glad.h>

#include "eng/BuiltinComponents.h"
#include "eng/RigidBody.h"
#include "eng/Script.h"
#include "game/DinosaurAI.h"
#include "game/EscapeZone.h"
#include "game/FirstPersonController.h"
#include "game/Game.h"
#include "game/Terrain.h"
#include "gfx/Primitives.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <format>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <spdlog/spdlog.h>

namespace game
{

    namespace
    {

        constexpr int kDinosaurCount = 14;
        constexpr int kTreeCount = 140;
        constexpr int kRockCount = 40;

    } // namespace

    Game::Game()
        : m_window(1280, 720, "Escape the Island"),
          m_post(m_window.FramebufferSize().x, m_window.FramebufferSize().y)
    {
        BuildWorld();
    }

    void Game::BuildWorld()
    {
        auto &meshes = m_world.Meshes();
        auto shader = std::make_shared<gfx::Shader>(SHADER_DIR "brdf.vert", SHADER_DIR "brdf.frag");

        auto makeMaterial = [&](glm::vec3 color, float metallic, float roughness)
        {
            auto m = std::make_shared<gfx::Material>(shader);
            m->baseColor = glm::vec4(color, 1.0f);
            m->metallic = metallic;
            m->roughness = roughness;
            return m;
        };

        const gfx::MeshHandle cube = meshes.Create(gfx::primitives::Cube(0.5f));
        const gfx::MeshHandle sphere = meshes.Create(gfx::primitives::Sphere(0.5f));
        const gfx::MeshHandle terrainMesh = meshes.Create(Terrain::BuildMesh());
        const gfx::MeshHandle seaMesh = meshes.Create(gfx::primitives::Plane(300.0f, 1.0f));

        const auto grass = makeMaterial({0.22f, 0.36f, 0.12f}, 0.0f, 0.9f);
        const auto sea = makeMaterial({0.05f, 0.25f, 0.4f}, 0.0f, 0.05f);
        const auto bark = makeMaterial({0.3f, 0.2f, 0.1f}, 0.0f, 0.8f);
        const auto leaves = makeMaterial({0.1f, 0.3f, 0.08f}, 0.0f, 0.7f);
        const auto rock = makeMaterial({0.4f, 0.4f, 0.4f}, 0.0f, 0.6f);
        const auto boat = makeMaterial({0.5f, 0.35f, 0.2f}, 0.0f, 0.5f);
        const auto beaconMat = makeMaterial({0.1f, 0.1f, 0.1f}, 0.0f, 0.3f);
        const std::shared_ptr<gfx::Material> dinoMats[2] = {makeMaterial({0.2f, 0.5f, 0.15f}, 0.0f, 0.6f),
                                                            makeMaterial({0.5f, 0.45f, 0.15f}, 0.0f, 0.6f)};

        m_world.Physics().SetHeightField(&Terrain::Height);
        m_world.Physics().SetBoundsRadius(Terrain::kWalkRadius);
        m_world.environment = std::make_shared<gfx::EnvironmentMap>(ASSETS_DIR "hdr/sky.hdr");

        m_world.CreateGameObject("Terrain").AddComponent<eng::MeshRendererComponent>(terrainMesh, grass);
        {
            auto &s = m_world.CreateGameObject("Sea", {0.0f, Terrain::kSeaLevel, 0.0f});
            s.AddComponent<eng::MeshRendererComponent>(seaMesh, sea);
        }

        const glm::vec2 startXZ{-62.0f, 40.0f};
        const glm::vec2 beaconXZ{65.0f, -45.0f};

        // Player
        {
            const glm::vec3 pos{startXZ.x, Terrain::Height(startXZ.x, startXZ.y) + 2.0f, startXZ.y};
            auto &p = m_world.CreateGameObject("Player", pos);
            p.AddComponent<eng::RigidBody>(glm::vec3(0.35f, 0.9f, 0.35f));
            p.AddComponent<eng::Health>(100.0f);
            auto &scripts = p.AddComponent<eng::ScriptComponent>();
            auto &fps = scripts.Add<FirstPersonController>();
            fps.SetLook(glm::degrees(std::atan2(beaconXZ.y - startXZ.y, beaconXZ.x - startXZ.x)), 0.0f);
            m_window.GetInput().SetReceiver(&scripts);
            m_player = &p;
        }

        // Extraction beacon and boat
        {
            const float h = Terrain::Height(beaconXZ.x, beaconXZ.y);
            auto &b = m_world.CreateGameObject("Beacon", {beaconXZ.x, h + 7.0f, beaconXZ.y});
            b.GetTransform().scale = {0.8f, 14.0f, 0.8f};
            b.AddComponent<eng::MeshRendererComponent>(cube, beaconMat);
            b.AddComponent<eng::ScriptComponent>().Add<EscapeZone>(*m_player, m_state, 6.0f);

            const glm::vec2 dir = glm::normalize(beaconXZ);
            const glm::vec2 boatXZ = dir * 95.0f;
            auto &bt = m_world.CreateGameObject("Boat", {boatXZ.x, 0.0f, boatXZ.y});
            bt.GetTransform().scale = {9.0f, 1.2f, 3.5f};
            bt.GetTransform().rotation = glm::angleAxis(std::atan2(-dir.y, dir.x), glm::vec3(0, 1, 0));
            bt.AddComponent<eng::MeshRendererComponent>(cube, boat);
        }

        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        auto randomLandPoint = [&](float minHeight, float minFromStart)
        {
            for (;;)
            {
                const float a = unit(rng) * glm::two_pi<float>();
                const float r = std::sqrt(unit(rng)) * 85.0f;
                const glm::vec2 p{std::cos(a) * r, std::sin(a) * r};
                if (Terrain::Height(p.x, p.y) < minHeight)
                    continue;
                if (glm::distance(p, startXZ) < minFromStart || glm::distance(p, beaconXZ) < 8.0f)
                    continue;
                return p;
            }
        };

        for (int i = 0; i < kTreeCount; ++i)
        {
            const glm::vec2 p = randomLandPoint(1.0f, 12.0f);
            const float h = Terrain::Height(p.x, p.y);
            const float trunk = 4.0f + unit(rng) * 3.0f;

            auto &t = m_world.CreateGameObject("Trunk", {p.x, h + trunk * 0.5f, p.y});
            t.GetTransform().scale = {0.6f, trunk, 0.6f};
            t.AddComponent<eng::MeshRendererComponent>(cube, bark);
            t.AddComponent<eng::RigidBody>(glm::vec3(0.3f, trunk * 0.5f, 0.3f), true);

            const float crown = 3.0f + unit(rng) * 2.0f;
            auto &c = m_world.CreateGameObject("Crown", {p.x, h + trunk + crown * 0.3f, p.y});
            c.GetTransform().scale = glm::vec3(crown);
            c.AddComponent<eng::MeshRendererComponent>(sphere, leaves);
        }

        for (int i = 0; i < kRockCount; ++i)
        {
            const glm::vec2 p = randomLandPoint(0.5f, 10.0f);
            const float s = 1.2f + unit(rng) * 2.5f;
            auto &r = m_world.CreateGameObject("Rock", {p.x, Terrain::Height(p.x, p.y) + s * 0.25f, p.y});
            r.GetTransform().scale = {s, s * 0.7f, s};
            r.AddComponent<eng::MeshRendererComponent>(sphere, rock);
            r.AddComponent<eng::RigidBody>(glm::vec3(s * 0.35f, s * 0.25f, s * 0.35f), true);
        }

        for (int i = 0; i < kDinosaurCount; ++i)
        {
            const glm::vec2 p = randomLandPoint(1.0f, 30.0f);
            auto &d = m_world.CreateGameObject("Dinosaur", {p.x, Terrain::Height(p.x, p.y) + 1.0f, p.y});
            d.GetTransform().scale = {3.0f, 1.5f, 1.2f}; // placeholder body
            d.AddComponent<eng::MeshRendererComponent>(cube, dinoMats[i % 2]);
            d.AddComponent<eng::RigidBody>(glm::vec3(1.2f, 0.75f, 1.0f));
            d.AddComponent<eng::Health>(100.0f);
            d.AddComponent<eng::ScriptComponent>().Add<DinosaurAI>(*m_player, m_state, 7.0f, 15.0f);
        }

        spdlog::info("World built: {} objects", m_world.ObjectCount());
    }

    void Game::Run()
    {
        core::Input &input = m_window.GetInput();
        m_window.SetCursorCaptured(true);

        double last = m_window.Time();
        double titleTimer = 0.0;
        int frames = 0;
        bool prevBloomKey = false, prevRawKey = false;

        while (!m_window.ShouldClose())
        {
            const double now = m_window.Time();
            const float dt = std::min(static_cast<float>(now - last), 0.05f);
            last = now;
            m_window.PollEvents();

            if (input.IsKeyDown(GLFW_KEY_ESCAPE))
                m_window.SetShouldClose();
            const bool bloomKey = input.IsKeyDown(GLFW_KEY_B);
            const bool rawKey = input.IsKeyDown(GLFW_KEY_F2);
            if (bloomKey && !prevBloomKey)
                m_post.bloomEnabled = !m_post.bloomEnabled;
            if (rawKey && !prevRawKey)
                m_post.showRaw = !m_post.showRaw;
            prevBloomKey = bloomKey;
            prevRawKey = rawKey;

            if (m_state.status == Status::Playing)
            {
                m_world.Update(dt);
                if (m_player->GetComponent<eng::Health>()->Dead())
                    m_state.status = Status::Lost;
                if (m_state.status != Status::Playing)
                {
                    m_window.SetCursorCaptured(false);
                    input.SetReceiver(nullptr);
                }
            }

            const glm::ivec2 size = m_window.FramebufferSize();
            if (size.x == 0 || size.y == 0)
                continue;

            m_post.Resize(size.x, size.y);
            m_post.BeginScene();
            m_world.Render(static_cast<float>(size.x) / static_cast<float>(size.y));
            m_post.EndScene(size.x, size.y);
            m_window.SwapBuffers();

            ++frames;
            titleTimer += dt;
            if (titleTimer >= 0.25)
            {
                const auto *hp = m_player->GetComponent<eng::Health>();
                const char *banner = m_state.status == Status::Won    ? " | YOU ESCAPED!"
                                     : m_state.status == Status::Lost ? " | YOU WERE EATEN"
                                                                      : "";
                m_window.SetTitle(std::format("Escape the Island | HP {:.0f} | Kills {} | {} fps | {} draw calls{}",
                                              hp->current, m_state.kills, static_cast<int>(frames / titleTimer),
                                              m_world.DrawCalls(), banner));
                frames = 0;
                titleTimer = 0.0;
            }
        }
    }

} // namespace game
