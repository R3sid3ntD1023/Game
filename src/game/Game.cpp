#include <glad/glad.h>

#include "eng/Animator.h"
#include "eng/AudioSource.h"
#include "eng/BuiltinComponents.h"
#include "eng/Lighting.h"
#include "eng/ParticleEmitter.h"
#include "eng/RigidBody.h"
#include "eng/Scene.h"
#include "eng/SceneLoader.h"
#include "eng/Script.h"
#include "game/DinosaurAI.h"
#include "game/EscapeZone.h"
#include "game/FirstPersonController.h"
#include "game/Game.h"
#include "game/Interaction.h"
#include "game/IslandScene.h"
#include "game/RaptorModel.h"
#include "game/Terrain.h"
#include "game/TreeGen.h"
#include "game/Weapon.h"
#include "gfx/ModelLoader.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <filesystem>
#include <format>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace game
{

    namespace
    {

        float Param(const std::vector<float> &p, size_t i, float fallback) { return i < p.size() ? p[i] : fallback; }

    } // namespace

    Game::Game(std::string startWorld, bool regenerate)
        : m_window(1280, 720, "Escape the Island"),
          m_post(m_window.FramebufferSize().x, m_window.FramebufferSize().y)
    {
        m_audio = std::make_unique<audio::AudioEngine>();
        m_hud = std::make_unique<Hud>(m_canvas.Root());

        MenuCallbacks callbacks;
        callbacks.play = [this](const std::string &world)
        { StartGame(world, m_menus->Regenerate()); };
        callbacks.resume = [this]
        { SetScreen(Screen::Playing); };
        callbacks.toMenu = [this]
        { ReturnToMenu(); };
        callbacks.quit = [this]
        { m_window.SetShouldClose(); };
        callbacks.toSettings = [this]
        { SetScreen(Screen::Settings); };

        m_menus = std::make_unique<MenuScreens>(m_canvas.Root(), std::move(callbacks), m_post);

        SetScreen(Screen::Menu);
        if (!startWorld.empty())
            StartGame(startWorld, regenerate);
    }

    void Game::SetScreen(Screen screen)
    {
        float masterVolume = m_menus->MasterVolume();

        m_screen = screen;
        const bool playing = screen == Screen::Playing && m_world && m_state.status == Status::Playing;
        m_window.SetCursorCaptured(playing);
        m_window.GetInput().SetReceiver(playing ? m_receiver : nullptr);

        m_menus->ShowMain(screen == Screen::Menu);
        m_menus->ShowSettings(screen == Screen::Settings);
        m_audio->SetMasterVolume(screen == Screen::Playing ? masterVolume : masterVolume * 0.1f);
        m_menus->ShowPause(screen == Screen::Paused);
        m_hud->SetVisible(screen != Screen::Menu && m_world != nullptr);
    }

    void Game::ReturnToMenu()
    {
        m_window.GetInput().SetReceiver(nullptr);
        m_receiver = nullptr;
        m_player = nullptr;
        m_world.reset();
        m_state = GameState{};
        SetScreen(Screen::Menu);
    }

    void Game::StartGame(const std::string &world, bool regenerate)
    {
        m_menus->SetMessage("");
        m_window.GetInput().SetReceiver(nullptr);
        m_receiver = nullptr;
        m_player = nullptr;
        m_world.reset();
        m_state = GameState{};

        try
        {
            m_world = std::make_unique<eng::World>();
            m_world->audio = m_audio.get();
            BuildWorld(world, regenerate);
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to start '{}': {}", world, e.what());
            m_world.reset();
            m_player = nullptr;
            m_receiver = nullptr;
            m_menus->SetMessage(e.what());
            SetScreen(Screen::Menu);
            return;
        }
        SetScreen(Screen::Playing);
    }

    void Game::BuildWorld(const std::string &worldName, bool regenerate)
    {
        const std::string path = std::string(ASSETS_DIR) + "worlds/" + worldName + eng::kSceneExtension;

        eng::SceneData scene;
        bool loaded = false;
        if (!regenerate && std::filesystem::exists(path))
        {
            try
            {
                scene = eng::LoadScene(path);
                loaded = true;
            }
            catch (const std::exception &e)
            {
                spdlog::warn("Regenerating world: {}", e.what());
            }
        }
        if (!loaded)
        {
            scene = worldName == "test" ? MakeTestScene() : MakeIslandScene();
            eng::SaveScene(path, scene);
            spdlog::info("Generated world '{}' -> {}", worldName, path);
        }

        auto shader = std::make_shared<gfx::Shader>(SHADER_DIR "brdf.vert", SHADER_DIR "brdf.frag");
        eng::SceneLoader loader(shader);

        loader.RegisterMesh(kMeshTerrain, [](const eng::MeshDesc &d)
                            { return Terrain::BuildMesh(static_cast<int>(Param(d.params, 0, 160.0f))); }, false);
        // assets/models/raptor.* (rigged, faces +X, origin at the hit-box center) replaces the procedural raptor.
        std::shared_ptr<gfx::SkinnedModel> raptorModel;
        std::shared_ptr<const gfx::Skeleton> raptorSkeleton;
        std::shared_ptr<const std::vector<gfx::AnimationClip>> raptorClips;
        if (const std::string external = gfx::FindModelOverride("raptor"); !external.empty())
        {
            try
            {
                raptorModel = std::make_shared<gfx::SkinnedModel>(gfx::LoadSkinnedModel(external));
                raptorSkeleton = std::shared_ptr<const gfx::Skeleton>(raptorModel, &raptorModel->skeleton);
                raptorClips = std::make_shared<const std::vector<gfx::AnimationClip>>(raptorModel->clips);
                spdlog::info("Raptor model loaded from {}", external);
            }
            catch (const std::exception &e)
            {
                spdlog::warn("Raptor model override ignored: {}", e.what());
                raptorModel.reset();
            }
        }
        loader.RegisterMesh(
            kMeshRaptor, [raptorModel](const eng::MeshDesc &)
            { return raptorModel ? raptorModel->mesh : RaptorMesh(); },
            false);
        loader.RegisterMesh(kMeshTreeTrunk, [](const eng::MeshDesc &d)
                            { return GenerateRainforestTree(static_cast<uint32_t>(Param(d.params, 0, 100.0f))).trunk; });
        loader.RegisterMesh(kMeshTreeCanopy, [](const eng::MeshDesc &d)
                            { return GenerateRainforestTree(static_cast<uint32_t>(Param(d.params, 0, 100.0f))).canopy; });
        loader.RegisterMesh(kMeshFern, [](const eng::MeshDesc &d)
                            { return GenerateFern(static_cast<uint32_t>(Param(d.params, 0, 200.0f))); });

        loader.RegisterScript(kScriptPlayer, [this](eng::GameObject &go, const std::vector<float> &p)
                              {
            go.AddComponent<WeaponComponent>();

            auto &scripts = go.AddComponent<eng::ScriptComponent>();
            auto &fps = scripts.Add<FirstPersonController>();
            fps.SetLook(Param(p, 0, 0.0f), Param(p, 1, 0.0f));
            scripts.Add<InteractionScript>(m_state);
            m_receiver = &scripts;
            m_player = &go;

            // The flashlight is a spot light entity that follows the camera; F toggles it.
            auto &flashlight = m_world->CreateGameObject("Flashlight");
            flashlight.AddComponent<eng::SpotLightComponent>(glm::vec3(1.0f, 0.95f, 0.85f), 220.0f, 50.0f, 11.0f, 21.0f);
            flashlight.AddComponent<eng::CameraFollower>(glm::vec3(0.15f, -0.12f, 0.3f)); });

        loader.RegisterScript(kScriptDinosaur, [this, raptorModel, raptorSkeleton, raptorClips](eng::GameObject &go, const std::vector<float> &p)
                              {
            eng::GameObject *player = m_world->Find("Player");
            if (!player)
                throw std::runtime_error("Raptor needs a Player object");
            if (raptorModel)
            {
                auto &animator = go.AddComponent<eng::Animator>(raptorSkeleton, raptorClips);
                if (!raptorModel->clips.empty())
                    animator.Play(raptorModel->clips.front().name);
            }
            else
                go.AddComponent<eng::Animator>(RaptorSkeleton()).hook = RaptorPose;
            go.AddComponent<eng::ScriptComponent>().Add<DinosaurAI>(*player, m_state, Param(p, 0, 8.0f), Param(p, 1, 12.0f)); });

        loader.RegisterScript(kScriptEscape, [this](eng::GameObject &go, const std::vector<float> &p)
                              {
            eng::GameObject *player = m_world->Find("Player");
            if (!player)
                throw std::runtime_error("Escape zone needs a Player object");
            go.AddComponent<eng::ScriptComponent>().Add<EscapeZone>(*player, m_state, Param(p, 0, 6.0f)); });

        loader.RegisterScript(kScriptHealthPickup, [](eng::GameObject &go, const std::vector<float> &p)
                              { go.AddComponent<HealthPickup>(Param(p, 0, 35.0f)); });

        loader.RegisterScript(kScriptWeaponPickup, [](eng::GameObject &go, const std::vector<float> &p)
                              {
            WeaponStats s;
            s.name = go.name;
            s.damage = Param(p, 0, s.damage);
            s.cooldown = Param(p, 1, s.cooldown);
            s.magSize = static_cast<int>(Param(p, 2, static_cast<float>(s.magSize)));
            s.reserve = static_cast<int>(Param(p, 3, static_cast<float>(s.reserve)));
            s.automatic = Param(p, 4, 0.0f) > 0.5f;
            s.reloadTime = Param(p, 5, s.reloadTime);
            s.range = Param(p, 6, s.range);
            go.AddComponent<WeaponPickup>(s); });

        loader.RegisterScript(kScriptDoor, [](eng::GameObject &go, const std::vector<float> &p)
                              { go.AddComponent<Door>(go.name, glm::vec3(Param(p, 0, 0.0f), Param(p, 1, 0.0f), Param(p, 2, 0.0f)), Param(p, 3, 1.5f)); });

        loader.RegisterScript(kScriptLightPost, [](eng::GameObject &go, const std::vector<float> &p)
                              {
            go.AddComponent<eng::PointLightComponent>(glm::vec3(Param(p, 4, 1.0f), Param(p, 5, 0.7f), Param(p, 6, 0.4f)),
                                                      Param(p, 0, 120.0f), Param(p, 1, 28.0f));
            go.AddComponent<eng::LightFlicker>(Param(p, 2, 0.5f), static_cast<uint32_t>(Param(p, 3, 1.0f))); });

        // params: type (0 point, 1 spot, 2 directional), r, g, b, intensity, range, inner degrees, outer degrees
        loader.RegisterScript(kScriptLight, [](eng::GameObject &go, const std::vector<float> &p)
                              { go.AddComponent<eng::LightComponent>(static_cast<gfx::LightType>(static_cast<int>(Param(p, 0, 0.0f))),
                                                                     glm::vec3(Param(p, 1, 1.0f), Param(p, 2, 1.0f), Param(p, 3, 1.0f)),
                                                                     Param(p, 4, 10.0f), Param(p, 5, 20.0f), Param(p, 6, 12.0f), Param(p, 7, 22.0f)); });

        // params: amount (0..1), wind x, wind z
        loader.RegisterScript(kScriptRain, [](eng::GameObject &go, const std::vector<float> &p)
                              {
            auto &rain = go.AddComponent<eng::ParticleEmitter>(eng::EmitterType::Rain);
            rain.amount = Param(p, 0, 1.0f);
            rain.wind = glm::vec3(Param(p, 1, 2.5f), 0.0f, Param(p, 2, 1.0f));
            go.AddComponent<eng::AudioSource>("rain_loop", true, 0.55f, false); });

        // params: volume
        loader.RegisterScript(kScriptWind, [](eng::GameObject &go, const std::vector<float> &p)
                              { go.AddComponent<eng::AudioSource>("wind_loop", true, Param(p, 0, 0.4f), false); });

        // params: rounds
        loader.RegisterScript(kScriptAmmoCrate, [](eng::GameObject &go, const std::vector<float> &p)
                              { go.AddComponent<AmmoCrate>(static_cast<int>(Param(p, 0, 24.0f))); });

        if (!m_environment)
            m_environment = std::make_shared<gfx::EnvironmentMap>(""); // empty path = procedural night sky
        m_world->environment = m_environment;
        m_world->Physics().SetHeightField(&Terrain::Height);
        m_world->Physics().SetBoundsRadius(Terrain::kWalkRadius);

        loader.Instantiate(*m_world, scene);
        if (!m_player)
            throw std::runtime_error("World '" + worldName + "' has no player object");

        spdlog::info("World '{}' ready: {} objects", worldName, m_world->ObjectCount());
    }

    void Game::Run()
    {
        core::Input &input = m_window.GetInput();

        double last = m_window.Time();
        double titleTimer = 0.0;
        int frames = 0;
        bool prevEsc = false, prevBloomKey = false, prevRawKey = false;
        Status prevStatus = Status::Playing;

        while (!m_window.ShouldClose())
        {
            const double now = m_window.Time();
            const float dt = std::min(static_cast<float>(now - last), 0.05f);
            last = now;
            m_window.PollEvents();
            m_audio->Update();

            const bool esc = input.IsKeyDown(GLFW_KEY_ESCAPE);
            if (esc && !prevEsc)
            {
                if (m_screen == Screen::Playing)
                {
                    if (m_state.status == Status::Playing)
                        SetScreen(Screen::Paused);
                    else
                        ReturnToMenu();
                }
                else if (m_screen == Screen::Paused)
                {
                    SetScreen(Screen::Playing);
                }
            }
            prevEsc = esc;

            const bool bloomKey = input.IsKeyDown(GLFW_KEY_B);
            const bool rawKey = input.IsKeyDown(GLFW_KEY_F2);
            if (m_screen == Screen::Playing)
            {
                if (bloomKey && !prevBloomKey)
                    m_post.bloomEnabled = !m_post.bloomEnabled;
                if (rawKey && !prevRawKey)
                    m_post.showRaw = !m_post.showRaw;
            }
            prevBloomKey = bloomKey;
            prevRawKey = rawKey;

            if (m_world && m_screen == Screen::Playing && m_state.status == Status::Playing)
            {
                m_world->Update(dt);
                if (m_player->GetComponent<eng::Health>()->Dead())
                    m_state.status = Status::Lost;
            }
            if (m_state.status != prevStatus)
            {
                prevStatus = m_state.status;
                m_state.prompt.clear();
                SetScreen(m_screen); // releases the cursor once the game is over
            }

            const glm::ivec2 size = m_window.FramebufferSize();
            if (size.x == 0 || size.y == 0)
                continue;

            if (m_world)
            {
                m_post.Resize(size.x, size.y);
                m_post.time = static_cast<float>(now);
                m_post.BeginScene();
                m_world->Render(static_cast<float>(size.x) / static_cast<float>(size.y), m_post.DepthTexture(), size.x, size.y);
                m_post.EndScene(size.x, size.y);
            }
            else
            {
                gfx::Framebuffer::BindScreen(size.x, size.y);
                glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
            }

            if (m_world && m_screen != Screen::Menu)
                m_hud->Update(*m_player, m_state, static_cast<float>(now));

            const bool cursorFree = m_screen != Screen::Playing || m_state.status != Status::Playing;
            const glm::vec2 mouse = cursorFree ? m_window.CursorPosition() : glm::vec2(-1.0f);
            m_canvas.Frame(glm::vec2(size), mouse, input.IsMouseDown(0));
            m_window.SwapBuffers();

            ++frames;
            titleTimer += dt;
            if (titleTimer >= 0.25)
            {
                m_window.SetTitle(std::format("Escape the Island | {} fps | {} draws",
                                              static_cast<int>(frames / titleTimer), m_world ? m_world->DrawCalls() : 0));
                frames = 0;
                titleTimer = 0.0;
            }
        }
    }

} // namespace game
