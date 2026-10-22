#include "eng/AssetMeta.h"
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace eng
{

    std::string AssetMetaPath(const std::string &relativePath)
    {
        return (std::filesystem::path(ASSETS_DIR) / (relativePath + kAssetMetaSuffix)).string();
    }

    bool ReadAssetMeta(const std::string &relativePath, AssetMeta &out)
    {
        std::ifstream file(AssetMetaPath(relativePath));
        if (!file)
            return false;
        try
        {
            cereal::JSONInputArchive ar(file);
            ar(out);
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    void WriteAssetMeta(const std::string &relativePath, const AssetMeta &meta)
    {
        const std::filesystem::path p = AssetMetaPath(relativePath);
        std::filesystem::create_directories(p.parent_path());
        std::ofstream file(p);
        if (!file)
            throw std::runtime_error("Cannot write asset meta: " + p.string());
        cereal::JSONOutputArchive ar(file);
        ar(meta);
    }

    std::string ResolveAssetPath(const std::string &relativePath)
    {
        const std::filesystem::path direct = std::filesystem::path(ASSETS_DIR) / relativePath;
        if (!std::filesystem::exists(direct))
        {
            AssetMeta meta;
            if (ReadAssetMeta(relativePath, meta) && std::filesystem::exists(meta.source))
                return meta.source;
        }
        return direct.string();
    }

} // namespace eng
