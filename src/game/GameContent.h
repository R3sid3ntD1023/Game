#pragma once
#include "core/Input.h"
#include "eng/GameObject.h"
#include "eng/Scene.h"
#include "eng/SceneLoader.h"
#include "eng/World.h"
#include "game/GameState.h"
#include "game/Terrain.h"
#include <memory>

namespace game
{

	class FirstPersonController;

	// Everything the game's scene scripts and meshes need, shared by Game.exe and the editor's play/simulate modes.
	struct GameContent
	{
		eng::World *world = nullptr;
		GameState *state = nullptr;
		Terrain *terrain = nullptr;
		// false: AI, player, pickups and other behaviour scripts are skipped; lights, rain and wind are kept (physics simulation).
		bool gameplay = true;

		// Filled in while the scene is instantiated.
		eng::GameObject *player = nullptr;
		core::IInputReceiver *receiver = nullptr;
		FirstPersonController *firstPerson = nullptr;
	};

	// Registers the game's meshes and scripts on the loader. `content` must outlive Instantiate.
	void RegisterGameContent(eng::SceneLoader &loader, GameContent &content);

	// Applies the scene's environment (sky, clock, weather, wind) and terrain physics to a world.
	void ConfigureWorld(eng::World &world, const eng::SceneData &scene, std::shared_ptr<gfx::EnvironmentMap> environment, Terrain &terrain);

} // namespace game
