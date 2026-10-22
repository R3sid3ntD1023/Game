#pragma once
#include "audio/AudioEngine.h"
#include "core/Input.h"
#include "eng/Scene.h"
#include "eng/World.h"
#include "game/GameContent.h"
#include "game/GameState.h"
#include "game/Hud.h"
#include "game/Terrain.h"
#include "gfx/EnvironmentMap.h"
#include "gfx/PostProcess.h"
#include "ui/Canvas.h"
#include <glad/glad.h>
#include <memory>

namespace editor
{

	// A world instantiated from the editor's scene and rendered with the engine's full PBR path into a texture
	// shown in the 3D View. Play runs the game (player, AI, pickups); Simulate runs physics only.
	class PlaySession
	{
	public:
		enum class Mode
		{
			Play,
			Simulate
		};

		// Throws std::exception when the scene cannot be instantiated (for example Play without a player).
		PlaySession(const eng::SceneData &scene, game::Terrain &terrain, Mode mode);
		~PlaySession();
		PlaySession(const PlaySession &) = delete;
		PlaySession &operator=(const PlaySession &) = delete;

		Mode GetMode() const { return m_mode; }
		core::IInputReceiver *Receiver() const { return m_content.receiver; }
		game::Status Status() const { return m_state.status; }

		// Advances the world. `flyCamera` drives the view in Simulate; Play uses the player's camera.
		void Update(float dt, const gfx::Camera &flyCamera);
		// Renders at the given size and returns the colour texture (bottom-up).
		GLuint Render(int width, int height, const gfx::Camera &flyCamera, float time);

	private:
		// Play: the player's camera, or the scene's camera object when there is no player.
		const gfx::Camera &ActiveCamera(const gfx::Camera &flyCamera)
		{
			if (m_mode != Mode::Play)
				return flyCamera;
			return !m_content.player && m_hasSceneCamera ? m_sceneCamera : m_world.GetCamera();
		}
		void EnsureTarget(int width, int height);

		Mode m_mode;
		game::Terrain &m_terrain;
		std::unique_ptr<audio::AudioEngine> m_audio; // declared before m_world so sources are released first
		game::GameState m_state;
		eng::World m_world;
		game::GameContent m_content;
		std::unique_ptr<game::Hud> m_hud;
		std::shared_ptr<gfx::EnvironmentMap> m_environment;
		std::unique_ptr<gfx::PostProcess> m_post;
		GLuint m_fbo = 0, m_color = 0;
		int m_width = 0, m_height = 0;
		gfx::Camera m_sceneCamera;
		bool m_hasSceneCamera = false;
		ui::Canvas m_canvas; // the world's UI layout, drawn over the session texture
		bool m_hasUi = false;
	};

} // namespace editor
