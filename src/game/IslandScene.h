#pragma once
#include "eng/Scene.h"

namespace game {

// Script names and mesh kinds used by generated scenes; registered in Game.
inline constexpr const char* kScriptPlayer = "FirstPersonController";
inline constexpr const char* kScriptDinosaur = "DinosaurAI";
inline constexpr const char* kScriptEscape = "EscapeZone";
inline constexpr const char* kScriptHealthPickup = "HealthPickup";
inline constexpr const char* kScriptWeaponPickup = "WeaponPickup";
inline constexpr const char* kScriptDoor = "Door";
inline constexpr const char* kMeshTerrain = "terrain";
inline constexpr const char* kMeshRaptor = "raptor";

// Deterministic island: terrain, sea, player, escape beacon + boat, trees, rocks and dinosaurs.
eng::SceneData MakeIslandScene(int trees, int rocks, int dinosaurs);

// Few objects, same layout; handy for quick iteration and for testing the loader.
eng::SceneData MakeTestScene();

} // namespace game
