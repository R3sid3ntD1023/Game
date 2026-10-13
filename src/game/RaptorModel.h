#pragma once
#include "gfx/Skeleton.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace game
{

    // Procedural velociraptor: ~3.5 m long, faces +X, origin at the center of its 1.5 m tall hit box.
    // Every box is rigidly weighted to one bone, so it works with the standard skinning path.
    gfx::MeshData RaptorMesh();
    std::shared_ptr<const gfx::Skeleton> RaptorSkeleton();

    // Walk/run cycle with tail sway; time in seconds (already speed-scaled), blend 0 = idle .. 1 = run.
    void RaptorPose(float time, float blend, std::vector<glm::mat4> &locals);

} // namespace game
