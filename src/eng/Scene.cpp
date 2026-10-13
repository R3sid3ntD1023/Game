#include "eng/Scene.h"
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace eng
{

    void SaveScene(const std::string &path, const SceneData &scene)
    {
        const std::filesystem::path p(path);
        if (p.has_parent_path())
            std::filesystem::create_directories(p.parent_path());

        std::ofstream file(p, std::ios::binary);
        if (!file)
            throw std::runtime_error("Cannot write scene file: " + path);
        cereal::BinaryOutputArchive ar(file);
        uint32_t magic = SceneData::kMagic, version = SceneData::kVersion;
        ar(magic, version, scene);
    }

    SceneData LoadScene(const std::string &path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("Cannot open scene file: " + path);
        cereal::BinaryInputArchive ar(file);

        uint32_t magic = 0, version = 0;
        ar(magic, version);
        if (magic != SceneData::kMagic)
            throw std::runtime_error("Not a world file: " + path);
        if (version != SceneData::kVersion)
            throw std::runtime_error("Unsupported world file version in: " + path);

        SceneData scene;
        ar(scene);
        return scene;
    }

} // namespace eng
