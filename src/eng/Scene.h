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

    // Camera component of an object. Stored after the terrain (version 14+), one entry per object.
    struct CameraDesc
    {
        bool enabled = false;
        bool primary = true; // the camera used when the world has no player
        float fov = 60.0f;
        float nearPlane = 0.1f;
        float farPlane = 800.0f;

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::make_nvp("enabled", enabled), cereal::make_nvp("primary", primary), cereal::make_nvp("fov", fov),
               cereal::make_nvp("nearPlane", nearPlane), cereal::make_nvp("farPlane", farPlane));
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
        CameraDesc camera;   // serialized separately by SaveScene/LoadScene

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

    // Sky, time of day and weather a world starts with. Stored after the scene (version 12+).
    struct EnvironmentDesc
    {
        bool dayNightCycle = true;
        float timeOfDay = 20.0f; // hours, 0-24
        float dayLength = 180.0f; // real seconds per day
        std::string skyHdr;      // empty = procedural sky; else a path relative to the assets directory
        bool weatherAutomatic = true;
        int32_t weather = 2; // WeatherType: 0 sunny, 1 cloudy, 2 light rain, 3 storm
        float weatherTransition = 8.0f;
        float windX = 0.85f;
        float windZ = 0.35f;

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::make_nvp("dayNightCycle", dayNightCycle), cereal::make_nvp("timeOfDay", timeOfDay),
               cereal::make_nvp("dayLength", dayLength), cereal::make_nvp("skyHdr", skyHdr),
               cereal::make_nvp("weatherAutomatic", weatherAutomatic), cereal::make_nvp("weather", weather),
               cereal::make_nvp("weatherTransition", weatherTransition), cereal::make_nvp("windX", windX),
               cereal::make_nvp("windZ", windZ));
        }
    };

    // Terrain heights for the world's terrain mesh. Stored after the environment (version 13+).
    struct TerrainDesc
    {
        std::string heightMap = "textures/hawaii_height_map.png"; // assets-relative image; empty = procedural hills
        float heightScale = 20.0f;                                 // metres above and below sea level at full white/black

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::make_nvp("heightMap", heightMap), cereal::make_nvp("heightScale", heightScale));
        }
    };

    struct SceneData
    {
        static constexpr uint32_t kMagic = 0x57444C47; // "GLDW"
        static constexpr uint32_t kVersion = 14;

        std::vector<MaterialDesc> materials;
        std::vector<MeshDesc> meshes;
        std::vector<ObjectDesc> objects;
        EnvironmentDesc environment; // serialized by SaveScene/LoadScene, not by serialize()
        TerrainDesc terrain;         // likewise
        std::string ui;              // UI layout name (assets/ui/<name>.ui.json) shown while playing; empty = none

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
