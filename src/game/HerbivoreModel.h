#pragma once
#include "gfx/Skeleton.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace game
{

	enum class HerbivoreSpecies
	{
		Triceratops,   // large: ~6 m long, hunted by raptor packs, fights back
		Protoceratops, // small: ~2.4 m long, ignored by raptors, runs from them
	};

	// Procedural ceratopsians: face +X, origin at the center of the hit box (HerbivoreHalfExtents), four legs on separate bones.
	// Both species share one bone layout, so HerbivorePose drives either.
	gfx::MeshData HerbivoreMesh(HerbivoreSpecies species);
	std::shared_ptr<const gfx::Skeleton> HerbivoreSkeleton(HerbivoreSpecies species);
	glm::vec3 HerbivoreHalfExtents(HerbivoreSpecies species);

	// Diagonal-gait walk/run with tail sway; time in seconds (already speed-scaled).
	// blend: 0 idle .. 1 run, and 0 .. -1 lowers the head to graze with a chewing bob.
	void HerbivorePose(float time, float blend, std::vector<glm::mat4> &locals);

} // namespace game
