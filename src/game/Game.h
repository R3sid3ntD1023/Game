#pragma once
#include "audio/AudioEngine.h"
#include "core/Window.h"
#include "eng/World.h"
#include "game/DebugCameraController.h"
#include "game/GameState.h"
#include "game/Hud.h"
#include "game/MenuScreens.h"
#include "game/Terrain.h"
#include "gfx/EnvironmentMap.h"
#include "gfx/PostProcess.h"
#include "ui/Canvas.h"
#include <memory>
#include <string>

namespace game
{

	enum class Screen
	{
		Menu,
		Playing,
		Paused,
		Settings
	};

	class Game
	{
	public:
		// A non-empty startWorld skips the main menu and starts that world directly.
		explicit Game(std::string startWorld = "", bool regenerate = false);
		void Run();

	private:
		void StartGame(const std::string &world, bool regenerate);
		void ReturnToMenu();
		void SetScreen(Screen screen);
		void BuildWorld(const std::string &worldName, bool regenerate);
		const gfx::Camera &GetCamera() const;

		core::Window m_window;
		ui::Canvas m_canvas;
		gfx::PostProcess m_post;
		float m_underwaterBlend = 0.0f;
		std::unique_ptr<audio::AudioEngine> m_audio; // declared before m_world so sources are released first
		std::unique_ptr<eng::World> m_world;		 // only exists while a world is loaded
		std::shared_ptr<gfx::EnvironmentMap> m_environment;
		std::string m_environmentPath;
		GameState m_state;
		eng::GameObject *m_player = nullptr;
		core::IInputReceiver *m_receiver = nullptr; // player's script component
		std::unique_ptr<Hud> m_hud;
		ui::Widget *m_sceneUi = nullptr; // widgets of the world's UI layout (owned by the canvas)
		std::unique_ptr<MenuScreens> m_menus;
		Screen m_screen = Screen::Menu;
		Terrain m_terrain;

		gfx::Camera m_debugCamera;
		bool m_debugCameraEnabled = false;
		DebugCameraController m_debugController;
		class FirstPersonController *m_firstPersonController = nullptr;
	};

} // namespace game
