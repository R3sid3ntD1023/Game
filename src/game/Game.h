#pragma once
#include "audio/AudioEngine.h"
#include "core/Window.h"
#include "eng/World.h"
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

	class DebugCameraController
	{
	public:
		void Update(gfx::Camera &camera, const core::Input &input);

	private:
		void Zoom(float delta);
		void Rotate(float yawDelta, float pitchDelta);
		void Pan(float x, float y, float z);

		glm::vec3 GetRight() const;
		glm::vec3 GetUp() const;
		glm::vec3 GetForward() const;

	private:
		float m_sensitivity = 1.0f;
		float m_yaw = -90.0f;
		float m_pitch = 0.0f;

		glm::vec3 m_position = glm::vec3(0.0f);
		float m_distance = 5.0f;
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
		std::unique_ptr<audio::AudioEngine> m_audio; // declared before m_world so sources are released first
		std::unique_ptr<eng::World> m_world;		 // only exists while a world is loaded
		std::shared_ptr<gfx::EnvironmentMap> m_environment;
		GameState m_state;
		eng::GameObject *m_player = nullptr;
		core::IInputReceiver *m_receiver = nullptr; // player's script component
		std::unique_ptr<Hud> m_hud;
		std::unique_ptr<MenuScreens> m_menus;
		Screen m_screen = Screen::Menu;
		Terrain m_terrain;

		gfx::Camera m_debugCamera;
		bool m_debugCameraEnabled = false;
		DebugCameraController m_debugController;
		class FirstPersonController *m_firstPersonController = nullptr;
	};

} // namespace game
