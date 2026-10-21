#pragma once

#include "eng/Scene.h"
#include "game/Terrain.h"

namespace game
{

	// Script names and mesh kinds used by generated scenes; registered in Game.
	inline constexpr const char *kScriptPlayer = "FirstPersonController";
	inline constexpr const char *kScriptDinosaur = "DinosaurAI";
	inline constexpr const char *kScriptHerbivore = "HerbivoreAI"; // params: species (0 triceratops, 1 protoceratops), speed
	inline constexpr const char *kScriptPlant = "Plant";		   // params: bites, regrow seconds
	inline constexpr const char *kScriptEscape = "EscapeZone";
	inline constexpr const char *kScriptHealthPickup = "HealthPickup";
	inline constexpr const char *kScriptWeaponPickup = "WeaponPickup";
	inline constexpr const char *kScriptDoor = "Door";
	inline constexpr const char *kScriptLight = "Light";		 // params: type (0 point, 1 spot, 2 directional), r, g, b, intensity, range, inner deg, outer deg
	inline constexpr const char *kScriptWind = "Wind";			 // params: volume
	inline constexpr const char *kScriptAmmoCrate = "AmmoCrate"; // params: rounds
	inline constexpr const char *kScriptRain = "Rain";			 // params: amount, wind x, wind z
	inline constexpr const char *kScriptLightPost = "LightPost"; // params: intensity, radius, flicker, seed, r, g, b
	inline constexpr const char *kMeshTerrain = "terrain";
	inline constexpr const char *kMeshRaptor = "raptor";
	inline constexpr const char *kMeshTriceratops = "triceratops";
	inline constexpr const char *kMeshProtoceratops = "protoceratops";
	inline constexpr const char *kMeshTreeTrunk = "tree_trunk";	  // params: seed
	inline constexpr const char *kMeshTreeCanopy = "tree_canopy"; // params: seed
	inline constexpr const char *kMeshFern = "fern";			  // params: seed
	inline constexpr const char *kMeshGrass = "grass_tuft";		  // params: seed

	struct IslandParams
	{

		int trees = 20;
		int ferns = 50;
		int grassClumps = 400; // each clump scatters several tufts
		int rocks = 20;
		int raptors = 10;
		int triceratops = 3;
		int protoceratops = 5;
		int buildings = 10;
		int posts = 24;
	};

	// Deterministic night-time rainforest: terrain, sea, player, escape compound, trees, ferns, rocks,
	// placeholder buildings, flickering light posts, pickups and raptors.
	eng::SceneData MakeIslandScene(const IslandParams &params = {}, const Terrain &heightSource = {});

	// Same layout with far fewer objects; handy for quick iteration and for testing the loader.
	eng::SceneData MakeTestScene();

} // namespace game
