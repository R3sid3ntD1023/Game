#pragma once
#include "gfx/Mesh.h"
#include "gfx/Skeleton.h"
#include <string>

namespace gfx
{

    // Loads every mesh in the file (node transforms baked in) as one MeshData, scaled uniformly. Throws on failure.
    MeshData LoadModel(const std::string &path, float scale = 1.0f);

    // Skinned model: bind-space mesh with bone ids/weights (max 4 per vertex), the node-hierarchy skeleton and animation clips.
    SkinnedModel LoadSkinnedModel(const std::string &path);

    // Returns assets/models/<name>.(glb|gltf|obj|fbx|dae) if one exists, else an empty string.
    std::string FindModelOverride(const std::string &name);

} // namespace gfx
