#include <glad/glad.h>

#include "game/Game.h"
#include "eng/Animator.h"
#include "eng/BuiltinComponents.h"
#include "eng/RigidBody.h"
#include "eng/Scene.h"
#include "eng/SceneLoader.h"
#include "eng/Script.h"
#include "game/DinosaurAI.h"
#include "game/EscapeZone.h"
#include "game/FirstPersonController.h"
#include "game/Interaction.h"
#include "game/IslandScene.h"
#include "game/RaptorModel.h"
#include "game/Terrain.h"
#include "game/Weapon.h"
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

        constexpr int kTrees = 140;
        constexpr int kRocks = 40;
        constexpr int kRaptors = 14;

        float Param(const std::vector<float> &p, size_t i, float fallback) { return i < p.size() ? p[i] : fallback; }

    } // namespace

    Game::Game(std::string startWorld, bool regenerate)
        : m_window(1280, 720, "Escape the Island"),
          m_post(m_window.FramebufferSize().x, m_window.FramebufferSize().y)
    {
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
        m_menus = std::make_unique<MenuScreens>(m_canvas.Root(), std::move(callbacks), m_post);

        SetScreen(Screen::Menu);
        if (!startWorld.empty())
            StartGame(startWorld, regenerate);
    }

    void Game::SetScreen(Screen screen)
    {
        m_screen = screen;
        const bool playing = screen == Screen::Playing && m_world && m_state.status == Status::Playing;
        m_window.SetCursorCaptured(playing);
        m_window.GetInput().SetReceiver(playing ? m_receiver : nullptr);

        m_menus->ShowMain(screen == Screen::Menu);
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
        const std::string path = std::string(ASSETS_DIR) + "worlds/" + worldName + ".world";

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
            scene = worldName == "test" ? MakeTestScene() : MakeIslandScene(kTrees, kRocks, kRaptors);
            eng::SaveScene(path, scene);
            spdlog::info("Generated world '{}' -> {}", worldName, path);
        }

        auto shader = std::make_shared<gfx::Shader>(SHADER_DIR "brdf.vert", SHADER_DIR "brdf.frag");
        eng::SceneLoader loader(shader);

        loader.RegisterMesh(kMeshTerrain, [](const eng::MeshDesc &d)
                            { return Terrain::BuildMesh(static_cast<int>(Param(d.params, 0, 160.0f))); });
        loader.RegisterMesh(kMeshRaptor, [](const eng::MeshDesc &)
                            { return RaptorMesh(); });

        loader.RegisterScript(kScriptPlayer, [this](eng::GameObject &go, const std::vector<float> &p)
                              {
            go.AddComponent<WeaponComponent>();
            auto &scripts = go.AddComponent<eng::ScriptComponent>();
            auto &fps = scripts.Add<FirstPersonController>();
            fps.SetLook(Param(p, 0, 0.0f), Param(p, 1, 0.0f));
            scripts.Add<InteractionScript>(m_state);
            m_receiver = &scripts;
            m_player = &go; });

        loader.RegisterScript(kScriptDinosaur, [this](eng::GameObject &go, const std::vector<float> &p)
                              {
            eng::GameObject *player = m_world->Find("Player");
            if (!player)
                throw std::runtime_error("Raptor needs a Player object");
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

        if (!m_environment)
            m_environment = std::make_shared<gfx::EnvironmentMap>(ASSETS_DIR "hdr/sky.hdr");
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
                m_post.BeginScene();
                m_world->Render(static_cast<float>(size.x) / static_cast<float>(size.y));
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
