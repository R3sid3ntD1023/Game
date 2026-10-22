#include "eng/Scene.h"
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace eng
{

    namespace
    {

        template <class Archive>
        void Write(Archive &ar, const SceneData &scene)
        {
            uint32_t magic = SceneData::kMagic, version = SceneData::kVersion;
            std::vector<CameraDesc> cameras;
            cameras.reserve(scene.objects.size());
            for (const ObjectDesc &o : scene.objects)
                cameras.push_back(o.camera);
            ar(cereal::make_nvp("magic", magic), cereal::make_nvp("version", version), cereal::make_nvp("scene", scene),
               cereal::make_nvp("environment", scene.environment), cereal::make_nvp("terrain", scene.terrain),
               cereal::make_nvp("cameras", cameras), cereal::make_nvp("ui", scene.ui));
        }

        template <class Archive>
        SceneData Read(Archive &ar, const std::string &path)
        {
            uint32_t magic = 0, version = 0;
            ar(cereal::make_nvp("magic", magic), cereal::make_nvp("version", version));
            if (magic != SceneData::kMagic)
                throw std::runtime_error("Not a world file: " + path);
            // 11: scene only; 12: + environment; 13: + terrain; 14: + cameras and UI layout.
            if (version < 11 || version > SceneData::kVersion)
                throw std::runtime_error("Unsupported world file version in: " + path);

            SceneData scene;
            ar(cereal::make_nvp("scene", scene));
            if (version >= 12)
                ar(cereal::make_nvp("environment", scene.environment));
            if (version >= 13)
                ar(cereal::make_nvp("terrain", scene.terrain));
            if (version >= 14)
            {
                std::vector<CameraDesc> cameras;
                ar(cereal::make_nvp("cameras", cameras), cereal::make_nvp("ui", scene.ui));
                for (size_t i = 0; i < cameras.size() && i < scene.objects.size(); ++i)
                    scene.objects[i].camera = cameras[i];
            }
            return scene;
        }

    } // namespace

    void SaveScene(const std::string &path, const SceneData &scene)
    {
        const std::filesystem::path p(path);
        if (p.has_parent_path())
            std::filesystem::create_directories(p.parent_path());

        std::ofstream file(p, std::ios::binary);
        if (!file)
            throw std::runtime_error("Cannot write scene file: " + path);

#ifdef GAME_SCENE_JSON
        cereal::JSONOutputArchive ar(file, cereal::JSONOutputArchive::Options(9)); // 9 digits round-trip a float
        Write(ar, scene);
#else
        cereal::BinaryOutputArchive ar(file);
        Write(ar, scene);
#endif
    }

    SceneData LoadScene(const std::string &path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("Cannot open scene file: " + path);

#ifdef GAME_SCENE_JSON
        cereal::JSONInputArchive ar(file);
#else
        cereal::BinaryInputArchive ar(file);
#endif
        return Read(ar, path);
    }

} // namespace eng
