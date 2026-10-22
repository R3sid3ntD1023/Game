#include <glad/glad.h>

#include <filesystem>

#include "eng/Animator.h"
#include "eng/AudioSource.h"
#include "eng/BuiltinComponents.h"
#include "eng/Lighting.h"
#include "eng/ParticleSystemComponent.h"
#include "eng/RigidBody.h"
#include "eng/Scene.h"
#include "eng/SceneLoader.h"
#include "eng/Script.h"
#include "game/CarnivoreAI.h"
#include "game/DinosaurAI.h"
#include "game/EscapeZone.h"
#include "game/FirstPersonController.h"
#include "game/Game.h"
#include "game/GameContent.h"
#include "game/HerbivoreAI.h"
#include "game/HerbivoreModel.h"
#include "game/Plant.h"
#include "game/GrassGen.h"
#include "game/Interaction.h"
#include "game/IslandScene.h"
#include "game/RaptorModel.h"
#include "game/Terrain.h"
#include "game/TreeGen.h"
#include "game/Weapon.h"
#include "gfx/ModelLoader.h"
#include "ui/UiLayout.h"
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

		float Param(const std::vector<float> &p, size_t i, float fallback)
		{
			return i < p.size() ? p[i] : fallback;
		}

	} // namespace

	Game::Game(std::string startWorld, bool regenerate)
		: m_window(1280, 720, "Escape the Island"),
		  m_post(m_window.FramebufferSize().x, m_window.FramebufferSize().y)
	{
		m_audio = std::make_unique<audio::AudioEngine>();
		m_hud = std::make_unique<Hud>(m_canvas.Root());

		MenuCallbacks callbacks;
		callbacks.play = [this](const std::string &world) { StartGame(world, m_menus->Regenerate()); };
		callbacks.resume = [this] { SetScreen(Screen::Playing); };
		callbacks.toMenu = [this] { ReturnToMenu(); };
		callbacks.quit = [this] { m_window.SetShouldClose(); };
		callbacks.toSettings = [this] { SetScreen(Screen::Settings); };

		m_menus = std::make_unique<MenuScreens>(m_canvas.Root(), std::move(callbacks), m_post);

		m_terrain.SetHeightSource(MakeHeightSource(eng::TerrainDesc{}));

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
		if (m_sceneUi)
			m_sceneUi->visible = screen == Screen::Playing || screen == Screen::Paused;
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
			m_world->SetWorldQuery(&m_terrain);
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
			scene = worldName == "test" ? MakeTestScene() : MakeIslandScene({}, m_terrain);

			eng::SaveScene(path, scene);
			spdlog::info("Generated world '{}' -> {}", worldName, path);
		}
		m_terrain.SetHeightSource(MakeHeightSource(scene.terrain));

		auto shaders = std::make_shared<gfx::ShaderLibrary>(SHADER_DIR "materials/");
		eng::SceneLoader loader(shaders);

		GameContent content;
		content.world = m_world.get();
		content.state = &m_state;
		content.terrain = &m_terrain;
		RegisterGameContent(loader, content);

		const eng::EnvironmentDesc &env = scene.environment;
		const std::string skyPath = env.skyHdr.empty() ? std::string() : (std::filesystem::path(ASSETS_DIR) / env.skyHdr).string();
		if (!m_environment || skyPath != m_environmentPath)
		{
			m_environment = std::make_shared<gfx::EnvironmentMap>(skyPath); // empty path = procedural sky
			m_environmentPath = skyPath;
		}
		ConfigureWorld(*m_world, scene, m_environment, m_terrain);

		loader.Instantiate(*m_world, scene);
		m_player = content.player;
		m_receiver = content.receiver;
		m_firstPersonController = content.firstPerson;
		if (!m_player)
			throw std::runtime_error("World '" + worldName + "' has no player object");

		if (m_sceneUi)
			m_sceneUi->visible = false;
		m_sceneUi = nullptr;
		if (!scene.ui.empty())
		{
			try
			{
				const ui::UiLayout layout = ui::LoadUiLayout(std::string(ASSETS_DIR) + "ui/" + scene.ui + ui::kUiLayoutExtension);
				m_sceneUi = &m_canvas.Root().Add<ui::Widget>();
				m_sceneUi->Stretch();
				ui::BuildWidgets(*m_sceneUi, layout);
			}
			catch (const std::exception &e)
			{
				spdlog::warn("UI layout '{}' not loaded: {}", scene.ui, e.what());
			}
		}
		spdlog::info("World '{}' ready: {} objects", worldName, m_world->ObjectCount());
	}

	const gfx::Camera &Game::GetCamera() const
	{
		auto &camera = m_world && !m_debugCameraEnabled ? m_world->GetCamera() : m_debugCamera;
		return camera;
	}

	void Game::Run()
	{
		core::Input &input = m_window.GetInput();

		double last = m_window.Time();
		double titleTimer = 0.0;
		int frames = 0;
		bool prevEsc = false, prevBloomKey = false, prevRawKey = false, prevTabKey = false;
		Status prevStatus = Status::Playing;

		while (!m_window.ShouldClose())
		{
			const double now = m_window.Time();
			const float dt = std::min(static_cast<float>(now - last), 0.05f);
			last = now;

			input.BeginFrame();
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
			const bool tabKey = input.IsKeyDown(GLFW_KEY_TAB);
			if (m_screen == Screen::Playing)
			{
				if (bloomKey && !prevBloomKey)
					m_post.bloomEnabled = !m_post.bloomEnabled;
				if (rawKey && !prevRawKey)
					m_post.showRaw = !m_post.showRaw;
				if (tabKey && !prevTabKey)
				{
					m_debugCameraEnabled = !m_debugCameraEnabled;
					if (m_firstPersonController)
						m_firstPersonController->enabled = !m_debugCameraEnabled;
				}
				if (m_world)
				{
					// [ and ] scrub the time of day (3 game hours per second); F3 pauses the clock.
					auto &clock = m_world->DayNight();
					const float scrub = static_cast<float>(dt) * 3.0f;
					if (input.IsKeyDown(GLFW_KEY_LEFT_BRACKET))
						clock.SetTime(clock.timeOfDay - scrub);
					if (input.IsKeyDown(GLFW_KEY_RIGHT_BRACKET))
						clock.SetTime(clock.timeOfDay + scrub);
					// F5 toggles the AI perception overlay (sight cones, touch range, awareness bars, noises).
					static bool prevF5 = false;
					const bool f5 = input.IsKeyDown(GLFW_KEY_F5);
					if (f5 && !prevF5)
						m_world->debugPerception = !m_world->debugPerception;
					prevF5 = f5;
					static bool prevF6 = false;
					const bool f6 = input.IsKeyDown(GLFW_KEY_F6);
					if (f6 && !prevF6)
						m_world->Weather().Cycle();
					prevF6 = f6;
					static bool prevF3 = false;
					const bool f3 = input.IsKeyDown(GLFW_KEY_F3);
					if (f3 && !prevF3)
						clock.paused = !clock.paused;
					prevF3 = f3;

					// F4 cycles the nav overlay: off -> walkable mesh -> slopes -> grid lines.
					static bool prevF4 = false;
					const bool f4 = input.IsKeyDown(GLFW_KEY_F4);
					if (f4 && !prevF4)
						m_world->SetNavDebugMode(m_world->NavDebugMode() + 1);
					prevF4 = f4;
				}
			}
			prevBloomKey = bloomKey;
			prevRawKey = rawKey;
			prevTabKey = tabKey;

			if (m_world && m_screen == Screen::Playing && m_state.status == Status::Playing)
			{
				m_state.ecology->Update(dt);
				m_world->Update(dt, GetCamera());
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

			if (m_debugCameraEnabled && m_screen == Screen::Playing)
				m_debugController.Update(m_debugCamera, input);

			if (m_world)
			{
				m_post.Resize(size.x, size.y);
				m_post.time = static_cast<float>(now);
				m_post.exposure = glm::mix(1.6f, 0.85f, m_world->DayNight().Daylight());
				{
					// Submerged when the camera is below the sea surface (a little slack for wave height).
					const gfx::Camera &cam = GetCamera();
					const float target = cam.position.y < Terrain::kSeaLevel - 0.05f && m_world->GetTerrainHeight(cam.position.x, cam.position.z) < cam.position.y ? 1.0f : 0.0f;
					m_underwaterBlend += std::clamp(target - m_underwaterBlend, -1.0f, 1.0f) * std::min(1.0f, static_cast<float>(dt) * 8.0f);
					m_world->underwater = target > 0.5f;
					m_audio->SetMuffle(m_underwaterBlend);
					m_post.underwater = m_underwaterBlend;
					m_post.nearPlane = cam.nearPlane;
					m_post.farPlane = cam.farPlane;
				}
				m_post.BeginScene();
				m_world->Render(static_cast<float>(size.x) / static_cast<float>(size.y), m_post.DepthTexture(), size.x, size.y, GetCamera());
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
				m_window.SetTitle(std::format("Escape the Island | {} fps | {} draws | {:02d}:{:02d} | {}", static_cast<int>(frames / titleTimer), m_world ? m_world->DrawCalls() : 0,
											  m_world ? static_cast<int>(m_world->DayNight().timeOfDay) : 0,
											  m_world ? static_cast<int>(std::fmod(m_world->DayNight().timeOfDay, 1.0f) 											  * 60.0f) : 0,
											  											  m_world ? WeatherName(m_world->Weather().Current()) : ""));
											  				frames = 0;
				titleTimer = 0.0;
			}
		}
	}

} // namespace game
