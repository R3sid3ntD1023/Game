#pragma once
#include "core/CerealGlm.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

namespace eng
{

    // Plain-data description of a world; serialized with cereal (binary) and turned into GameObjects by SceneLoader.
    struct MaterialDesc
    {
        std::string name;
        glm::vec3 color{1.0f};
        float metallic = 0.0f;
        float roughness = 0.5f;
        glm::vec3 emissive{0.0f};
        // Image paths relative to the assets folder; empty keeps the flat default.
        std::string albedoMap;
        std::string normalMap;
        std::string metalRoughMap;

        template <class Archive>
        void serialize(Archive &ar) { ar(name, color, metallic, roughness, emissive, albedoMap, normalMap, metalRoughMap); }
    };

    // `kind` selects a registered mesh generator (cube, sphere, plane, ...); `params` are its arguments.
    struct MeshDesc
    {
        std::string name;
        std::string kind;
        std::vector<float> params;
        std::string path; // kind "model": mesh file relative to the assets folder

        template <class Archive>
        void serialize(Archive &ar) { ar(name, kind, params, path); }
    };

    struct ObjectDesc
    {
        std::string name;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
        int32_t mesh = -1;     // index into SceneData::meshes
        int32_t material = -1; // index into SceneData::materials
        bool hasBody = false;
        bool bodyStatic = false;
        glm::vec3 halfExtents{0.5f};
        float health = 0.0f; // <= 0: no Health component
        std::string script;  // name registered with SceneLoader::RegisterScript
        std::vector<float> scriptParams;

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(name, position, rotation, scale, mesh, material, hasBody, bodyStatic, halfExtents, health, script, scriptParams);
        }
    };

    struct SceneData
    {
        static constexpr uint32_t kMagic = 0x57444C47; // "GLDW"
        static constexpr uint32_t kVersion = 2;

        std::vector<MaterialDesc> materials;
        std::vector<MeshDesc> meshes;
        std::vector<ObjectDesc> objects;

        int32_t AddMaterial(MaterialDesc m)
        {
            materials.push_back(std::move(m));
            return static_cast<int32_t>(materials.size() - 1);
        }
        int32_t AddMesh(MeshDesc m)
        {
            meshes.push_back(std::move(m));
            return static_cast<int32_t>(meshes.size() - 1);
        }
        ObjectDesc &AddObject(ObjectDesc o)
        {
            objects.push_back(std::move(o));
            return objects.back();
        }

        template <class Archive>
        void serialize(Archive &ar) { ar(materials, meshes, objects); }
    };

    // Throw std::runtime_error on I/O failure or a bad/foreign file.
    void SaveScene(const std::string &path, const SceneData &scene);
    SceneData LoadScene(const std::string &path);

} // namespace eng
