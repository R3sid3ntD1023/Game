#include "editor/Assets.h"
#include "eng/AssetMeta.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <mutex>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif

namespace editor
{

	namespace fs = std::filesystem;

	namespace
	{

		std::string Lower(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		const char *FolderFor(AssetType type)
		{
			switch (type)
			{
			case AssetType::Hdr:
				return "hdr";
			case AssetType::Model:
				return "models";
			default:
				return "textures";
			}
		}

		std::mutex g_dropMutex;
		std::vector<std::string> g_dropped;

	} // namespace

	const char *AssetTypeName(AssetType type)
	{
		switch (type)
		{
		case AssetType::Hdr:
			return "HDR sky";
		case AssetType::Model:
			return "Model";
		default:
			return "Texture";
		}
	}

	bool ClassifyAsset(const std::string &filename, AssetType &type)
	{
		const std::string ext = Lower(fs::path(filename).extension().string());
		if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp")
			type = AssetType::Texture;
		else if (ext == ".hdr")
			type = AssetType::Hdr;
		else if (ext == ".glb" || ext == ".gltf" || ext == ".obj" || ext == ".fbx")
			type = AssetType::Model;
		else
			return false;
		return true;
	}

	std::vector<AssetEntry> ListAssets()
	{
		std::vector<AssetEntry> result;
		const fs::path root = ASSETS_DIR;
		const std::string suffix = eng::kAssetMetaSuffix;
		for (const char *folder : {"textures", "hdr", "models"})
		{
			const fs::path dir = root / folder;
			std::error_code ec;
			if (!fs::exists(dir, ec))
				continue;
			for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator(); it.increment(ec))
			{
				if (ec || !it->is_regular_file())
					continue;
				std::string name = it->path().filename().string();
				bool linked = false;
				fs::path file = it->path();
				if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
				{
					file = it->path().parent_path() / name.substr(0, name.size() - suffix.size());
					if (fs::exists(file))
						continue; // a real copy exists next to the sidecar
					linked = true;
				}
				AssetEntry entry;
				if (!ClassifyAsset(file.string(), entry.type))
					continue;
				entry.path = fs::relative(file, root).generic_string();
				entry.linked = linked;
				result.push_back(std::move(entry));
			}
		}
		std::sort(result.begin(), result.end(), [](const AssetEntry &a, const AssetEntry &b) { return a.path < b.path; });
		return result;
	}

	std::string ImportAsset(const std::string &source, const ImportOptions &options, AssetEntry &imported)
	{
		const fs::path src = fs::path(source);
		std::error_code ec;
		if (!fs::is_regular_file(src, ec))
			return "File not found: " + source;
		AssetType type;
		if (!ClassifyAsset(src.string(), type))
			return "Unsupported file type: " + src.extension().string();

		const std::string rel = std::string(FolderFor(type)) + "/" + src.filename().string();
		const fs::path dest = fs::path(ASSETS_DIR) / rel;
		const fs::path meta = eng::AssetMetaPath(rel);
		if (!options.overwrite && (fs::exists(dest) || fs::exists(meta)))
			return "'" + rel + "' already exists (enable Overwrite)";

		fs::create_directories(dest.parent_path(), ec);
		if (options.link)
		{
			fs::remove(dest, ec);
			eng::AssetMeta m;
			m.type = AssetTypeName(type);
			m.source = fs::absolute(src).string();
			m.srgb = options.srgb;
			try
			{
				eng::WriteAssetMeta(rel, m);
			}
			catch (const std::exception &e)
			{
				return e.what();
			}
		}
		else
		{
			fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
			if (ec)
				return "Copy failed: " + ec.message();
			fs::remove(meta, ec);
		}
		imported.path = rel;
		imported.type = type;
		imported.linked = options.link;
		return {};
	}

	std::string BrowseForFile()
	{
#ifdef _WIN32
		char buffer[MAX_PATH] = {};
		OPENFILENAMEA ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFile = buffer;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = "Assets\0*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.hdr;*.glb;*.gltf;*.obj;*.fbx\0All files\0*.*\0";
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetOpenFileNameA(&ofn))
			return buffer;
#endif
		return {};
	}

	void QueueDroppedFile(const std::string &path)
	{
		std::lock_guard lock(g_dropMutex);
		g_dropped.push_back(path);
	}

	std::vector<std::string> TakeDroppedFiles()
	{
		std::lock_guard lock(g_dropMutex);
		return std::exchange(g_dropped, {});
	}

} // namespace editor
