#pragma once
#include "core/CerealGlm.h"
#include <cereal/cereal.hpp>
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
        // Foliage sway in metres, shader type (file name in shaders/materials; empty = Standard), and two-sided rendering.
        float windStrength = 0.0f;
        std::string shader;
        bool doubleSided = false;

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::make_nvp("name", name), cereal::make_nvp("color", color), cereal::make_nvp("metallic", metallic),
               cereal::make_nvp("roughness", roughness), cereal::make_nvp("emissive", emissive),
               cereal::make_nvp("albedoMap", albedoMap), cereal::make_nvp("normalMap", normalMap),
               cereal::make_nvp("metalRoughMap", metalRoughMap), cereal::make_nvp("windStrength", windStrength),
               cereal::make_nvp("shader", shader), cereal::make_nvp("doubleSided", doubleSided));
        }
    };

    // `kind` selects a registered mesh generator (cube, sphere, plane, ...); `params` are its arguments.
    struct MeshDesc
    {
        std::string name;
        std::string kind;
        std::vector<float> params;
        std::string path; // kind "model": mesh file relative to the assets folder
        // kind "model": use the materials and textures stored in the file. Objects with material -1 get them;
        // objects that name a scene material override them.
        bool importMaterials = false;

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::make_nvp("name", name), cereal::make_nvp("kind", kind), cereal::make_nvp("params", params),
               cereal::make_nvp("path", path), cereal::make_nvp("importMaterials", importMaterials));
        }
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
            ar(cereal::make_nvp("name", name), cereal::make_nvp("position", position), cereal::make_nvp("rotation", rotation),
               cereal::make_nvp("scale", scale), cereal::make_nvp("mesh", mesh), cereal::make_nvp("material", material),
               cereal::make_nvp("hasBody", hasBody), cereal::make_nvp("bodyStatic", bodyStatic),
               cereal::make_nvp("halfExtents", halfExtents), cereal::make_nvp("health", health),
               cereal::make_nvp("script", script), cereal::make_nvp("scriptParams", scriptParams));
        }
    };

    struct SceneData
    {
        static constexpr uint32_t kMagic = 0x57444C47; // "GLDW"
        static constexpr uint32_t kVersion = 11;

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
        void serialize(Archive &ar)
        {
            ar(cereal::make_nvp("materials", materials), cereal::make_nvp("meshes", meshes), cereal::make_nvp("objects", objects));
        }
    };

    // Debug and RelWithDebInfo builds write readable JSON; Release builds write compact binary (see CMakeLists.txt).
#ifdef GAME_SCENE_JSON
    inline constexpr const char *kSceneExtension = ".world.json";
#else
    inline constexpr const char *kSceneExtension = ".world";
#endif

    // Throw std::runtime_error on I/O failure or a bad/foreign file.
    void SaveScene(const std::string &path, const SceneData &scene);
    SceneData LoadScene(const std::string &path);

} // namespace eng
