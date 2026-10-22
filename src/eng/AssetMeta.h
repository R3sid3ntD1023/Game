#pragma once
#include <cereal/cereal.hpp>
#include <string>

namespace eng
{

    // Sidecar file `<asset>.meta.json` next to an asset path inside the assets folder. It lets the editor "link" an
    // external file instead of copying it: the asset keeps its assets-relative name and the meta points at the source.
    struct AssetMeta
    {
        std::string type;   // texture, hdr or model
        std::string source; // absolute path of the real file
        bool srgb = true;   // textures: colour data (true) or linear data such as normal maps (false)

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::make_nvp("type", type), cereal::make_nvp("source", source), cereal::make_nvp("srgb", srgb));
        }
    };

    inline constexpr const char *kAssetMetaSuffix = ".meta.json";

    std::string AssetMetaPath(const std::string &relativePath);
    bool ReadAssetMeta(const std::string &relativePath, AssetMeta &out);
    void WriteAssetMeta(const std::string &relativePath, const AssetMeta &meta);

    // assets/<relativePath> when that file exists, else the source named by its meta file, else assets/<relativePath>.
    std::string ResolveAssetPath(const std::string &relativePath);

} // namespace eng
