#pragma once
#include "gfx/Mesh.h"
#include "gfx/Skeleton.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace gfx
{

    // Where an imported texture comes from: an external file (path) or bytes embedded in the model (encoded).
    struct TextureSource
    {
        std::string key;                    // unique id for caching (absolute path, or "<model>#<embedded name>")
        std::string path;                   // external image file; empty when embedded
        std::vector<unsigned char> encoded; // embedded png/jpg bytes
        bool Empty() const { return path.empty() && encoded.empty(); }
    };

    // Material as authored in the model file. Pure data, so it can be read without a GL context.
    struct ModelMaterial
    {
        std::string name;
        glm::vec4 baseColor{1.0f};
        float metallic = 0.0f;
        float roughness = 0.5f;
        glm::vec3 emissive{0.0f};
        float alphaCutoff = 0.0f; // > 0: texels with alpha below this are discarded (glTF MASK, and BLEND approximated)
        bool doubleSided = false;
        TextureSource albedo, normal, metalRough; // metalRough: glTF packing (G = roughness, B = metallic)
    };

    struct ModelPart
    {
        MeshData mesh;
        int material = -1; // index into ModelAsset::materials
    };

    // One part per source mesh (node transforms baked in, scaled uniformly) plus the materials and overall bounds.
    struct ModelAsset
    {
        std::vector<ModelPart> parts;
        std::vector<ModelMaterial> materials;
        Bounds bounds;
    };

    // Loads every mesh in the file (node transforms baked in) as one MeshData, scaled uniformly. Throws on failure.
    MeshData LoadModel(const std::string &path, float scale = 1.0f);

    // Like LoadModel but keeps meshes separated by material and imports the materials with their textures. Throws on failure.
    ModelAsset LoadModelAsset(const std::string &path, float scale = 1.0f);

    // Bounds of the whole model after `scale`, without building GPU data (usable while generating scenes). Throws on failure.
    Bounds QueryModelBounds(const std::string &path, float scale = 1.0f);

    // Skinned model: bind-space mesh with bone ids/weights (max 4 per vertex), the node-hierarchy skeleton and animation clips.
    SkinnedModel LoadSkinnedModel(const std::string &path);

    // Returns assets/models/<name>.(glb|gltf|obj|fbx|dae) if one exists, else an empty string.
    std::string FindModelOverride(const std::string &name);

} // namespace gfx
