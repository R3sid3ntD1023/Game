#pragma once
#include <string>
#include <vector>

namespace editor
{

	enum class AssetType
	{
		Texture,
		Hdr,
		Model
	};

	struct AssetEntry
	{
		std::string path; // relative to the assets folder, forward slashes (what scene files store)
		AssetType type = AssetType::Texture;
		bool linked = false; // only a .meta.json sidecar exists; the real file lives outside the assets folder
	};

	struct ImportOptions
	{
		bool link = false;      // write a metadata file pointing at the source instead of copying it
		bool overwrite = false; // replace an existing asset of the same name
		bool srgb = true;       // textures: colour (true) or linear data (false)
	};

	const char *AssetTypeName(AssetType type);
	// Detects the asset type from the file extension. Returns false for unsupported files.
	bool ClassifyAsset(const std::string &filename, AssetType &type);
	// Scans assets/textures, assets/hdr and assets/models (including linked assets).
	std::vector<AssetEntry> ListAssets();
	// Copies or links `source` into the assets folder. On success returns an empty string and sets `imported`
	// to the assets-relative path; otherwise returns an error message.
	std::string ImportAsset(const std::string &source, const ImportOptions &options, AssetEntry &imported);
	// Native open-file dialog; empty when cancelled or unsupported.
	std::string BrowseForFile();

	// Files dropped on the window (GLFW drop callback fills this).
	void QueueDroppedFile(const std::string &path);
	std::vector<std::string> TakeDroppedFiles();

} // namespace editor
