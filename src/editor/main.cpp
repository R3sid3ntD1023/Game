#include <glad/glad.h>

#include "core/Window.h"
#include "editor/Assets.h"
#include "editor/PlaySession.h"
#include "editor/UiEditor.h"
#include "editor/BehaviorEditor.h"
#include "editor/AnimationEditor.h"
#include "editor/PreviewMesh.h"
#include "editor/SceneView3D.h"
#include "eng/AssetMeta.h"
#include "eng/Scene.h"
#include "eng/Weather.h"
#include "game/IslandScene.h"
#include "game/Terrain.h"
#include "gfx/Texture.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
// clang-format off
#include <ImGuizmo.h> // must follow imgui.h
// clang-format on
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

	namespace fs = std::filesystem;

	struct ScriptInfo
	{
		const char *name;
		std::vector<const char *> params;
	};

	// Script names registered by the game, with labels for their float parameters.
	const std::vector<ScriptInfo> &KnownScripts()
	{
		using namespace game;
		static const std::vector<ScriptInfo> scripts = {
			{kScriptPlayer, {"look yaw", "look pitch"}},
			{kScriptDinosaur, {"speed", "bite damage", "pack (1) / solo (0)"}},
			{kScriptCarnivore, {"speed", "bite damage"}},
			{kScriptHerbivore, {"species (0 tri, 1 proto)", "speed"}},
			{kScriptPlant, {"bites", "regrow seconds"}},
			{kScriptEscape, {}},
			{kScriptHealthPickup, {"heal amount"}},
			{kScriptWeaponPickup, {"damage", "cooldown", "magazine", "reserve", "automatic", "reload time", "range"}},
			{kScriptDoor, {}},
			{kScriptLight, {"type (0 pt, 1 spot, 2 dir)", "r", "g", "b", "intensity", "range", "inner deg", "outer deg"}},
			{kScriptWind, {"volume"}},
			{kScriptAmmoCrate, {"rounds"}},
			{kScriptRain, {"amount", "wind x", "wind z"}},
			{kScriptLightPost, {"intensity", "radius", "flicker", "seed", "r", "g", "b"}},
		};
		return scripts;
	}

	// Copies text into a fixed buffer, truncating and always null-terminating.
	void CopyToBuffer(char *dest, size_t capacity, const std::string &text)
	{
		const size_t n = std::min(text.size(), capacity - 1);
		std::memcpy(dest, text.data(), n);
		dest[n] = '\0';
	}

	// Neutral dark grey theme with an amber accent; nothing blue.
	void ApplyTheme()
	{
		ImGui::StyleColorsDark();
		ImGuiStyle &style = ImGui::GetStyle();
		style.WindowRounding = 2.0f;
		style.FrameRounding = 2.0f;
		style.GrabRounding = 2.0f;
		style.TabRounding = 2.0f;
		style.PopupRounding = 2.0f;
		style.ScrollbarRounding = 2.0f;
		style.FramePadding = ImVec2(6.0f, 3.0f);
		style.ItemSpacing = ImVec2(8.0f, 4.0f);

		const auto grey = [](float v, float a = 1.0f) { return ImVec4(v, v, v, a); };
		const ImVec4 accent(0.86f, 0.54f, 0.16f, 1.0f);
		const ImVec4 accentDim(0.60f, 0.38f, 0.12f, 1.0f);
		const ImVec4 accentFaint(0.86f, 0.54f, 0.16f, 0.30f);

		ImVec4 *c = style.Colors;
		c[ImGuiCol_Text] = grey(0.90f);
		c[ImGuiCol_TextDisabled] = grey(0.50f);
		c[ImGuiCol_WindowBg] = grey(0.12f);
		c[ImGuiCol_ChildBg] = grey(0.10f);
		c[ImGuiCol_PopupBg] = grey(0.14f, 0.98f);
		c[ImGuiCol_Border] = grey(0.25f);
		c[ImGuiCol_BorderShadow] = grey(0.0f, 0.0f);
		c[ImGuiCol_FrameBg] = grey(0.18f);
		c[ImGuiCol_FrameBgHovered] = grey(0.24f);
		c[ImGuiCol_FrameBgActive] = grey(0.30f);
		c[ImGuiCol_TitleBg] = grey(0.09f);
		c[ImGuiCol_TitleBgActive] = grey(0.14f);
		c[ImGuiCol_TitleBgCollapsed] = grey(0.09f);
		c[ImGuiCol_MenuBarBg] = grey(0.09f);
		c[ImGuiCol_ScrollbarBg] = grey(0.10f);
		c[ImGuiCol_ScrollbarGrab] = grey(0.30f);
		c[ImGuiCol_ScrollbarGrabHovered] = grey(0.40f);
		c[ImGuiCol_ScrollbarGrabActive] = grey(0.50f);
		c[ImGuiCol_CheckMark] = accent;
		c[ImGuiCol_SliderGrab] = accentDim;
		c[ImGuiCol_SliderGrabActive] = accent;
		c[ImGuiCol_Button] = grey(0.22f);
		c[ImGuiCol_ButtonHovered] = grey(0.30f);
		c[ImGuiCol_ButtonActive] = accentDim;
		c[ImGuiCol_Header] = grey(0.22f);
		c[ImGuiCol_HeaderHovered] = grey(0.28f);
		c[ImGuiCol_HeaderActive] = accentDim;
		c[ImGuiCol_Separator] = grey(0.25f);
		c[ImGuiCol_SeparatorHovered] = accentDim;
		c[ImGuiCol_SeparatorActive] = accent;
		c[ImGuiCol_ResizeGrip] = grey(0.25f);
		c[ImGuiCol_ResizeGripHovered] = accentDim;
		c[ImGuiCol_ResizeGripActive] = accent;
		c[ImGuiCol_Tab] = grey(0.14f);
		c[ImGuiCol_TabHovered] = grey(0.30f);
		c[ImGuiCol_TabSelected] = grey(0.24f);
		c[ImGuiCol_TabSelectedOverline] = accent;
		c[ImGuiCol_TabDimmed] = grey(0.11f);
		c[ImGuiCol_TabDimmedSelected] = grey(0.17f);
		c[ImGuiCol_TabDimmedSelectedOverline] = accentDim;
		c[ImGuiCol_DockingPreview] = accentFaint;
		c[ImGuiCol_DockingEmptyBg] = grey(0.08f);
		c[ImGuiCol_PlotLines] = grey(0.6f);
		c[ImGuiCol_PlotLinesHovered] = accent;
		c[ImGuiCol_PlotHistogram] = accentDim;
		c[ImGuiCol_PlotHistogramHovered] = accent;
		c[ImGuiCol_TableHeaderBg] = grey(0.16f);
		c[ImGuiCol_TableBorderStrong] = grey(0.25f);
		c[ImGuiCol_TableBorderLight] = grey(0.20f);
		c[ImGuiCol_TableRowBg] = grey(0.0f, 0.0f);
		c[ImGuiCol_TableRowBgAlt] = grey(1.0f, 0.03f);
		c[ImGuiCol_TextSelectedBg] = accentFaint;
		c[ImGuiCol_DragDropTarget] = accent;
		c[ImGuiCol_NavCursor] = accent;
		c[ImGuiCol_NavWindowingHighlight] = grey(1.0f, 0.7f);
		c[ImGuiCol_NavWindowingDimBg] = grey(0.2f, 0.2f);
		c[ImGuiCol_ModalWindowDimBg] = grey(0.0f, 0.5f);
	}

	// Combo listing the project's assets of one type (including linked ones); returns true when the choice changed.
	bool AssetCombo(const char *label, std::string &path, editor::AssetType type, const std::vector<editor::AssetEntry> &assets, const char *none = "(none)")
	{
		bool changed = false;
		if (ImGui::BeginCombo(label, path.empty() ? none : path.c_str()))
		{
			if (ImGui::Selectable(none, path.empty()))
			{
				path.clear();
				changed = true;
			}
			for (const editor::AssetEntry &a : assets)
			{
				if (a.type != type)
					continue;
				ImGui::PushID(a.path.c_str());
				const std::string text = a.linked ? a.path + "  [link]" : a.path;
				if (ImGui::Selectable(text.c_str(), a.path == path))
				{
					path = a.path;
					changed = true;
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	struct DinoPreset
	{
		const char *label;
		const char *meshName;
		const char *meshKind;
		const char *materialName;
		glm::vec3 color;
		glm::vec3 scale;
		glm::vec3 halfExtents;
		float health;
		const char *script;
		std::vector<float> params;
	};

	const std::vector<DinoPreset> &DinoPresets()
	{
		using namespace game;
		static const std::vector<DinoPreset> presets = {
			{"Raptor", "raptor", kMeshRaptor, "raptor_hide", {0.35f, 0.3f, 0.2f}, glm::vec3(1.0f), {1.1f, 0.75f, 0.45f}, 80.0f, kScriptDinosaur, {8.0f, 12.0f, 1.0f}},
			{"T-Rex", "raptor", kMeshRaptor, "trex_hide", {0.2f, 0.17f, 0.1f}, glm::vec3(2.4f), {2.6f, 1.8f, 1.1f}, 5000.0f, kScriptCarnivore, {7.5f, 35.0f}},
			{"Triceratops",
			 "triceratops",
			 kMeshTriceratops,
			 "triceratops_hide",
			 {0.28f, 0.33f, 0.24f},
			 glm::vec3(1.0f),
			 {2.5f, 1.3f, 1.1f},
			 300.0f,
			 kScriptHerbivore,
			 {0.0f, 5.5f}},
			{"Protoceratops",
			 "protoceratops",
			 kMeshProtoceratops,
			 "protoceratops_hide",
			 {0.45f, 0.36f, 0.22f},
			 glm::vec3(1.0f),
			 {1.0f, 0.5f, 0.45f},
			 60.0f,
			 kScriptHerbivore,
			 {1.0f, 7.5f}},
		};
		return presets;
	}

	// Edits eng::SceneData in place: dockable world picker, hierarchy, inspector, 3D view and a top-down map.
	class Editor
	{
	public:
		Editor()
		{
			ApplyTerrain();
			RefreshWorlds();
			m_assets = editor::ListAssets();
		}

		void Draw()
		{
			DrawMenu();
			DrawDockspace();
			if (m_show.worlds)
				DrawWorlds();
			if (m_show.assets)
				DrawAssets();
			if (m_show.hierarchy)
				DrawHierarchy();
			if (m_show.inspector)
				DrawInspector();
			if (m_show.materials)
				DrawMaterials();
			if (m_show.environment)
				DrawEnvironment();
			if (m_show.topDown)
				DrawViewport();
			m_uiEditor.Draw();
			m_behaviorEditor.Draw();
			m_animEditor.Draw();

			int picked = m_selected;
			bool moved = false;
			if (m_view.open && m_view.Draw(m_scene, picked, moved, m_terrain))
				Select(picked);
			if (moved)
				m_dirty = true;
		}

		bool WantsQuit() const { return m_quit; }
		void SetWindow(core::Window *window) { m_window = window; }
		void OpenWorld(const std::string &world) { Load(world); }

	private:
		void DrawDockspace()
		{
			const ImGuiID id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
			const ImGuiDockNode *node = ImGui::DockBuilderGetNode(id);
			if (m_layoutBuilt || !node || !node->IsLeafNode() || node->Windows.Size > 0)
			{
				m_layoutBuilt = true;
				return;
			}
			m_layoutBuilt = true;

			ImGuiID centre = id, left = 0, right = 0, leftBottom = 0;
			left = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, 0.2f, nullptr, &centre);
			right = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.25f, nullptr, &centre);
			leftBottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.7f, nullptr, &left);
			ImGui::DockBuilderDockWindow("Worlds", left);
			ImGui::DockBuilderDockWindow("Assets", left);
			ImGui::DockBuilderDockWindow("Hierarchy", leftBottom);
			ImGui::DockBuilderDockWindow("Inspector", right);
			ImGui::DockBuilderDockWindow("Materials", right);
			ImGui::DockBuilderDockWindow("Environment", right);
			ImGui::DockBuilderDockWindow("3D View", centre);
			ImGui::DockBuilderDockWindow("Top-down", centre);
			ImGui::DockBuilderDockWindow("UI Editor", centre);
			ImGui::DockBuilderDockWindow("Behavior Tree", centre);
			ImGui::DockBuilderDockWindow("Animation Graph", centre);
			ImGui::DockBuilderFinish(id);
		}

		void RefreshWorlds()
		{
			m_worlds.clear();
			const fs::path dir = fs::path(ASSETS_DIR) / "worlds";
			if (!fs::exists(dir))
				return;
			const std::string ext = eng::kSceneExtension;
			for (const auto &entry : fs::directory_iterator(dir))
			{
				const std::string name = entry.path().filename().string();
				if (name.size() > ext.size() && name.compare(name.size() - ext.size(), ext.size(), ext) == 0)
					m_worlds.push_back(name.substr(0, name.size() - ext.size()));
			}
			std::sort(m_worlds.begin(), m_worlds.end());
		}

		static std::string PathFor(const std::string &world) { return (fs::path(ASSETS_DIR) / "worlds" / (world + eng::kSceneExtension)).string(); }

		void Load(const std::string &world)
		{
			StopPlayback();
			try
			{
				m_scene = eng::LoadScene(PathFor(world));
				m_world = world;
				ApplyTerrain();
				m_selectedMaterial = -1;
				m_view.Invalidate();
				Select(-1);
				m_dirty = false;
				m_status = "Loaded " + world;
			}
			catch (const std::exception &e)
			{
				m_status = std::string("Load failed: ") + e.what();
			}
		}

		void Save()
		{
			if (m_world.empty())
				return;
			try
			{
				eng::SaveScene(PathFor(m_world), m_scene);
				m_dirty = false;
				m_status = "Saved " + m_world;
			}
			catch (const std::exception &e)
			{
				m_status = std::string("Save failed: ") + e.what();
			}
		}

		bool HasSelection() const { return m_selected >= 0 && m_selected < static_cast<int>(m_scene.objects.size()); }

		static eng::ObjectDesc MakeEmpty(const std::string &name)
		{
			eng::ObjectDesc o;
			o.name = name;
			return o;
		}

		void AddObject(eng::ObjectDesc o)
		{
			m_scene.AddObject(std::move(o));
			Select(static_cast<int>(m_scene.objects.size()) - 1);
			m_dirty = true;
		}

		int FindOrAddMesh(const std::string &name, const std::string &kind, std::vector<float> params)
		{
			for (size_t i = 0; i < m_scene.meshes.size(); ++i)
				if (m_scene.meshes[i].kind == kind && m_scene.meshes[i].params == params)
					return static_cast<int>(i);
			return m_scene.AddMesh({name, kind, std::move(params)});
		}

		int FindOrAddMaterial(const std::string &name, const glm::vec3 &color)
		{
			for (size_t i = 0; i < m_scene.materials.size(); ++i)
				if (m_scene.materials[i].name == name)
					return static_cast<int>(i);
			eng::MaterialDesc m;
			m.name = name;
			m.color = color;
			m.roughness = 0.7f;
			return m_scene.AddMaterial(std::move(m));
		}

		// Placed on the terrain at the world origin; drag it into position afterwards.
		glm::vec3 SpawnPoint(float lift) const { return {0.0f, m_terrain.Height(0.0f, 0.0f) + lift, 0.0f}; }

		void AddPrimitive(const char *name, const char *kind, std::vector<float> params)
		{
			eng::ObjectDesc o = MakeEmpty(name);
			o.mesh = FindOrAddMesh(name, kind, std::move(params));
			o.material = FindOrAddMaterial("default_grey", glm::vec3(0.6f));
			o.position = SpawnPoint(0.5f);
			AddObject(std::move(o));
		}

		void AddLight(const char *name, std::vector<float> params, float height)
		{
			eng::ObjectDesc o = MakeEmpty(name);
			o.script = game::kScriptLight;
			o.scriptParams = std::move(params);
			o.position = SpawnPoint(height);
			if (o.scriptParams[0] != 0.0f)
				o.rotation = glm::quat(glm::radians(glm::vec3(-60.0f, 0.0f, 0.0f))); // tilt down from -Z
			AddObject(std::move(o));
		}

		void AddDino(const DinoPreset &p)
		{
			eng::ObjectDesc o = MakeEmpty(p.label);
			o.mesh = FindOrAddMesh(p.meshName, p.meshKind, {});
			o.material = FindOrAddMaterial(p.materialName, p.color);
			o.scale = p.scale;
			o.hasBody = true;
			o.halfExtents = p.halfExtents;
			o.health = p.health;
			o.script = p.script;
			o.scriptParams = p.params;
			o.position = SpawnPoint(p.halfExtents.y + 0.3f);
			AddObject(std::move(o));
		}

		void Select(int index)
		{
			m_selected = index;
			if (HasSelection())
				m_euler = glm::degrees(glm::eulerAngles(m_scene.objects[index].rotation));
		}

		void DrawMenu()
		{
			if (!ImGui::BeginMainMenuBar())
				return;
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Save", "Ctrl+S", false, !m_world.empty()))
					Save();
				if (ImGui::MenuItem("Reload world list"))
					RefreshWorlds();
				if (ImGui::MenuItem("Quit"))
					m_quit = true;
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Window"))
			{
				ImGui::MenuItem("Worlds", nullptr, &m_show.worlds);
				ImGui::MenuItem("Assets", nullptr, &m_show.assets);
				ImGui::MenuItem("Hierarchy", nullptr, &m_show.hierarchy);
				ImGui::MenuItem("Inspector", nullptr, &m_show.inspector);
				ImGui::MenuItem("Materials", nullptr, &m_show.materials);
				ImGui::MenuItem("Environment", nullptr, &m_show.environment);
				ImGui::MenuItem("Top-down", nullptr, &m_show.topDown);
				ImGui::MenuItem("3D View", nullptr, &m_view.open);
				ImGui::MenuItem("UI Editor", nullptr, &m_uiEditor.open);
				ImGui::MenuItem("Behavior Tree", nullptr, &m_behaviorEditor.open);
				ImGui::MenuItem("Animation Graph", nullptr, &m_animEditor.open);
				ImGui::EndMenu();
			}
			DrawTransport();
			ImGui::TextDisabled("%s%s  |  %s", m_world.empty() ? "(no world)" : m_world.c_str(), m_dirty ? "*" : "", 			m_status.c_str());
			ImGui::EndMainMenuBar();

			if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
				Save();
		}

		enum class Glyph
		{
			Play,
			Pause,
			Stop,
			Simulate,
		};

		// Icon-only button drawn with the draw list; active highlights it in the accent colour.
		static bool GlyphButton(const char *id, Glyph glyph, bool active, bool enabled, const char *tip)
		{
			const float s = ImGui::GetFrameHeight();
			ImGui::BeginDisabled(!enabled);
			ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(0.60f, 0.38f, 0.12f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Button));
			const bool pressed = ImGui::Button(id, ImVec2(s * 1.3f, s));
			ImGui::PopStyleColor();
			const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
			const ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
			const float r = s * 0.28f;
			const ImU32 col = enabled ? IM_COL32(235, 235, 235, 255) : IM_COL32(120, 120, 120, 255);
			ImDrawList *dl = ImGui::GetWindowDrawList();
			switch (glyph)
			{
			case Glyph::Play:
				dl->AddTriangleFilled(ImVec2(c.x - r * 0.7f, c.y - r), ImVec2(c.x - r * 0.7f, c.y + r), ImVec2(c.x + r, c.y), IM_COL32(120, 210, 120, enabled ? 255 : 120));
				break;
			case Glyph::Pause:
				dl->AddRectFilled(ImVec2(c.x - r * 0.9f, c.y - r), ImVec2(c.x - r * 0.25f, c.y + r), col);
				dl->AddRectFilled(ImVec2(c.x + r * 0.25f, c.y - r), ImVec2(c.x + r * 0.9f, c.y + r), col);
				break;
			case Glyph::Stop:
				dl->AddRectFilled(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), IM_COL32(225, 90, 80, enabled ? 255 : 120));
				break;
			case Glyph::Simulate:
				dl->AddTriangleFilled(ImVec2(c.x - r, c.y - r), ImVec2(c.x - r, c.y + r), ImVec2(c.x, c.y), col);
				dl->AddTriangleFilled(ImVec2(c.x, c.y - r), ImVec2(c.x, c.y + r), ImVec2(c.x + r, c.y), col);
				break;
			}
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("%s", tip);
			return pressed;
		}

		void StopPlayback()
		{
			m_view.AttachSession(nullptr, m_window);
			m_session.reset();
			m_paused = false;
		}

		bool Running() const { return m_session != nullptr; }

		void StartSession(editor::PlaySession::Mode mode)
		{
			StopPlayback();
			try
			{
				m_session = std::make_unique<editor::PlaySession>(m_scene, m_terrain, mode);
				m_view.AttachSession(m_session.get(), m_window);
				m_status = mode == editor::PlaySession::Mode::Play ? "Playing " + m_world : "Simulating " + m_world;
			}
			catch (const std::exception &e)
			{
				m_session.reset();
				m_status = std::string("Cannot start: ") + e.what();
			}
		}

		// Play and Simulate run the edited scene in the 3D View; the scene itself is never modified.
		void DrawTransport()
		{
			const bool hasWorld = !m_world.empty();
			const bool playing = m_session && m_session->GetMode() == editor::PlaySession::Mode::Play;
			const bool simulating = m_session && !playing;
			ImGui::Separator();
			if (GlyphButton("##play", Glyph::Play, playing, hasWorld, "Play: run the game in the viewport"))
			{
				if (playing)
					StopPlayback();
				else
					StartSession(editor::PlaySession::Mode::Play);
			}
			if (GlyphButton("##pause", Glyph::Pause, m_paused, Running(), "Pause / resume"))
			{
				m_paused = !m_paused;
				m_view.SetSessionPaused(m_paused);
			}
			if (GlyphButton("##stop", Glyph::Stop, false, Running(), "Stop"))
			{
				StopPlayback();
				m_status = "Stopped";
			}
			if (GlyphButton("##sim", Glyph::Simulate, simulating, hasWorld, "Simulate: physics only, full PBR rendering"))
			{
				if (simulating)
					StopPlayback();
				else
					StartSession(editor::PlaySession::Mode::Simulate);
			}
			ImGui::Separator();
		}

		void DrawWorlds()
		{
			ImGui::Begin("Worlds", &m_show.worlds);
			for (const std::string &w : m_worlds)
				if (ImGui::Selectable(w.c_str(), w == m_world))
					Load(w);
			ImGui::End();
		}

		void DrawHierarchy()
		{
			ImGui::Begin("Hierarchy", &m_show.hierarchy);
			if (m_world.empty())
			{
				ImGui::TextDisabled("Pick a world.");
				ImGui::End();
				return;
			}
			if (ImGui::Button("Add"))
				ImGui::OpenPopup("add_object");
			if (ImGui::BeginPopup("add_object"))
			{
				if (ImGui::MenuItem("Empty"))
					AddObject(MakeEmpty("Object"));
				if (ImGui::MenuItem("Cube"))
					AddPrimitive("Cube", "cube", {0.5f});
				if (ImGui::MenuItem("Sphere"))
					AddPrimitive("Sphere", "sphere", {0.5f});
				ImGui::Separator();
				ImGui::TextDisabled("Lights");
				if (ImGui::MenuItem("Point light"))
					AddLight("Point Light", {0.0f, 1.0f, 0.9f, 0.7f, 10.0f, 20.0f, 12.0f, 25.0f}, 3.0f);
				if (ImGui::MenuItem("Spot light"))
					AddLight("Spot Light", {1.0f, 1.0f, 1.0f, 1.0f, 20.0f, 30.0f, 15.0f, 30.0f}, 5.0f);
				if (ImGui::MenuItem("Directional light"))
					AddLight("Directional Light", {2.0f, 1.0f, 0.95f, 0.85f, 3.0f, 0.0f, 0.0f, 0.0f}, 20.0f);
				ImGui::Separator();
				if (ImGui::MenuItem("Camera"))
				{
					eng::ObjectDesc o = MakeEmpty("Camera");
					o.camera.enabled = true;
					o.position = SpawnPoint(3.0f);
					AddObject(std::move(o));
				}
				ImGui::Separator();
				ImGui::TextDisabled("Dinosaurs");
				for (const DinoPreset &p : DinoPresets())
					if (ImGui::MenuItem(p.label))
						AddDino(p);
				ImGui::EndPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Duplicate") && HasSelection())
			{
				eng::ObjectDesc copy = m_scene.objects[m_selected];
				m_scene.AddObject(copy);
				Select(static_cast<int>(m_scene.objects.size()) - 1);
				m_dirty = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Delete") && HasSelection())
			{
				m_scene.objects.erase(m_scene.objects.begin() + m_selected);
				Select(-1);
				m_dirty = true;
			}
			ImGui::InputTextWithHint("##filter", "filter", m_filter, sizeof(m_filter));
			ImGui::Separator();

			ImGui::BeginChild("list");
			for (int i = 0; i < static_cast<int>(m_scene.objects.size()); ++i)
			{
				const std::string &name = m_scene.objects[i].name;
				if (m_filter[0] != '\0' && name.find(m_filter) == std::string::npos)
					continue;
				ImGui::PushID(i);
				if (ImGui::Selectable(name.empty() ? "(unnamed)" : name.c_str(), i == m_selected))
					Select(i);
				ImGui::PopID();
			}
			ImGui::EndChild();
			ImGui::End();
		}

		static bool InputString(const char *label, std::string &value)
		{
			char buf[256];
			CopyToBuffer(buf, sizeof(buf), value);
			if (!ImGui::InputText(label, buf, sizeof(buf)))
				return false;
			value = buf;
			return true;
		}

		void DrawEnvironment()
		{
			if (!ImGui::Begin("Environment", &m_show.environment))
			{
				ImGui::End();
				return;
			}
			eng::EnvironmentDesc &env = m_scene.environment;
			bool changed = false;

			ImGui::SeparatorText("UI");
			{
				const fs::path uiDir = fs::path(ASSETS_DIR) / "ui";
				if (ImGui::BeginCombo("UI layout", m_scene.ui.empty() ? "(none)" : m_scene.ui.c_str()))
				{
					if (ImGui::Selectable("(none)", m_scene.ui.empty()))
					{
						m_scene.ui.clear();
						changed = true;
					}
					if (fs::exists(uiDir))
						for (const auto &entry : fs::directory_iterator(uiDir))
						{
							const std::string file = entry.path().filename().string();
							const std::string ext = ui::kUiLayoutExtension;
							if (file.size() > ext.size() && file.ends_with(ext))
							{
								const std::string name = file.substr(0, file.size() - ext.size());
								if (ImGui::Selectable(name.c_str(), name == m_scene.ui))
								{
									m_scene.ui = name;
									changed = true;
								}
							}
						}
					ImGui::EndCombo();
				}
				ImGui::TextDisabled("Loaded with the world when playing.");
			}

			ImGui::SeparatorText("Sky");
			std::vector<std::string> skies = {"(procedural)"};
			const fs::path hdrDir = fs::path(ASSETS_DIR) / "hdr";
			if (fs::exists(hdrDir))
				for (const auto &entry : fs::directory_iterator(hdrDir))
					if (entry.path().extension() == ".hdr")
						skies.push_back("hdr/" + entry.path().filename().string());
			if (ImGui::BeginCombo("Sky", env.skyHdr.empty() ? "(procedural)" : env.skyHdr.c_str()))
			{
				for (const std::string &sky : skies)
				{
					const bool isProcedural = sky == "(procedural)";
					const bool selected = isProcedural ? env.skyHdr.empty() : sky == env.skyHdr;
					if (ImGui::Selectable(sky.c_str(), selected))
					{
						env.skyHdr = isProcedural ? std::string() : sky;
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
			if (!env.skyHdr.empty())
				ImGui::TextDisabled("HDR skies ignore time of day and clouds.");

			ImGui::SeparatorText("Time of day");
			changed |= ImGui::Checkbox("Day/night cycle", &env.dayNightCycle);
			changed |= ImGui::SliderFloat("Time (hours)", &env.timeOfDay, 0.0f, 24.0f, "%.2f");
			changed |= ImGui::DragFloat("Day length (s)", &env.dayLength, 1.0f, 10.0f, 3600.0f, "%.0f");
			ImGui::TextDisabled("6 sunrise, 12 noon, 18 sunset, 0 midnight");

			ImGui::SeparatorText("Weather");
			static const char *kWeather[] = {"Sunny", "Cloudy", "Light rain", "Storm"};
			env.weather = std::clamp(env.weather, 0, 3);
			changed |= ImGui::Combo("Starting weather", &env.weather, kWeather, 4);
			changed |= ImGui::Checkbox("Changes automatically", &env.weatherAutomatic);
			changed |= ImGui::DragFloat("Transition (s)", &env.weatherTransition, 0.1f, 0.0f, 60.0f, "%.1f");
			const eng::WeatherProfile profile = eng::ProfileFor(static_cast<eng::WeatherType>(env.weather));
			ImGui::TextDisabled(
				"Cloud %.2f  Dark %.2f  Rain %.2f  Wind x%.1f  Light x%.2f%s",
				profile.cloud,
				profile.dark,
				profile.rain,
				profile.wind,
				profile.light,
				profile.lightning > 0.5f ? "  Lightning" : ""
			);

			ImGui::SeparatorText("Wind");
			changed |= ImGui::DragFloat("Wind X", &env.windX, 0.01f, -2.0f, 2.0f);
			changed |= ImGui::DragFloat("Wind Z", &env.windZ, 0.01f, -2.0f, 2.0f);

			ImGui::SeparatorText("Terrain");
			{
				eng::TerrainDesc &t = m_scene.terrain;
				bool terrainChanged = AssetCombo("Height map", t.heightMap, editor::AssetType::Texture, m_assets, "(procedural)");
				terrainChanged |= ImGui::DragFloat("Height scale", &t.heightScale, 0.1f, 0.0f, 200.0f, "%.1f");
				if (terrainChanged)
				{
					ApplyTerrain();
					changed = true;
				}
			}

			if (changed)
				m_dirty = true;
			ImGui::End();
		}

		void ApplyTerrain()
		{
			m_terrain.SetHeightSource(game::MakeHeightSource(m_scene.terrain));
			m_view.Invalidate();
		}

		void RefreshAssets()
		{
			m_assets = editor::ListAssets();
			m_view.ReloadTextures();
		}

		void DrawAssets()
		{
			ImGui::Begin("Assets", &m_show.assets);
			for (std::string &dropped : editor::TakeDroppedFiles())
				CopyToBuffer(m_importPath, sizeof(m_importPath), dropped);

			if (ImGui::CollapsingHeader("Import"))
			{
				ImGui::InputText("Source", m_importPath, sizeof(m_importPath));
				ImGui::SameLine();
				if (ImGui::Button("Browse..."))
				{
					const std::string picked = editor::BrowseForFile();
					if (!picked.empty())
						CopyToBuffer(m_importPath, sizeof(m_importPath), picked);
				}
				ImGui::TextDisabled("Or drop a file onto the window.");
				int mode = m_import.link ? 1 : 0;
				ImGui::RadioButton("Copy to assets", &mode, 0);
				ImGui::SameLine();
				ImGui::RadioButton("Link (metadata)", &mode, 1);
				m_import.link = mode == 1;
				ImGui::Checkbox("Overwrite", &m_import.overwrite);
				ImGui::SameLine();
				ImGui::Checkbox("sRGB", &m_import.srgb);
				if (ImGui::Button("Import##do") && m_importPath[0])
				{
					editor::AssetEntry entry;
					const std::string err = editor::ImportAsset(m_importPath, m_import, entry);
					if (err.empty())
					{
						m_status = "Imported " + entry.path;
						m_importPath[0] = '\0';
						RefreshAssets();
					}
					else
						m_status = "Import failed: " + err;
				}
				ImGui::SameLine();
				if (ImGui::Button("Refresh"))
					RefreshAssets();
			}

			ImGui::Separator();
			DrawBrowser();
			ImGui::End();
		}

		static void DrawFolderIcon(ImDrawList *dl, ImVec2 p, float s)
		{
			const ImU32 body = IM_COL32(214, 160, 70, 255), back = IM_COL32(170, 120, 45, 255);
			dl->AddRectFilled(ImVec2(p.x + s * 0.08f, p.y + s * 0.2f), ImVec2(p.x + s * 0.42f, p.y + s * 0.32f), back, 2.0f);
			dl->AddRectFilled(ImVec2(p.x + s * 0.08f, p.y + s * 0.28f), ImVec2(p.x + s * 0.92f, p.y + s * 0.8f), body, 3.0f);
		}

		static void DrawFileIcon(ImDrawList *dl, ImVec2 p, float s, ImU32 tint, const char *label, bool linked)
		{
			const ImVec2 a(p.x + s * 0.2f, p.y + s * 0.08f), b(p.x + s * 0.8f, p.y + s * 0.88f);
			const float fold = s * 0.18f;
			dl->AddRectFilled(a, b, IM_COL32(60, 60, 60, 255), 3.0f);
			dl->AddRect(a, b, tint, 3.0f, 0, 1.5f);
			dl->AddTriangleFilled(ImVec2(b.x - fold, a.y), ImVec2(b.x, a.y + fold), ImVec2(b.x - fold, a.y + fold), tint);
			const ImVec2 ts = ImGui::CalcTextSize(label);
			dl->AddText(ImVec2((a.x + b.x - ts.x) * 0.5f, (a.y + b.y - ts.y) * 0.5f + s * 0.08f), tint, label);
			if (linked)
				dl->AddCircleFilled(ImVec2(b.x - s * 0.08f, b.y - s * 0.08f), s * 0.07f, IM_COL32(235, 140, 40, 255));
		}

		static ImU32 TintFor(editor::AssetType type, bool known)
		{
			if (!known)
				return IM_COL32(170, 170, 170, 255);
			switch (type)
			{
			case editor::AssetType::Texture:
				return IM_COL32(110, 190, 120, 255);
			case editor::AssetType::Hdr:
				return IM_COL32(235, 190, 80, 255);
			default:
				return IM_COL32(190, 130, 210, 255);
			}
		}

		void DrawBrowser()
		{
			const fs::path root = fs::path(ASSETS_DIR);
			fs::path dir = root / m_browseDir;
			if (!fs::exists(dir) || !fs::is_directory(dir))
			{
				m_browseDir.clear();
				dir = root;
			}

			if (ImGui::ArrowButton("##up", ImGuiDir_Up) && !m_browseDir.empty())
				m_browseDir = fs::path(m_browseDir).parent_path().generic_string();
			ImGui::SameLine();
			ImGui::TextUnformatted(("assets/" + m_browseDir).c_str());
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100.0f);
			ImGui::SetNextItemWidth(100.0f);
			ImGui::SliderFloat("##icon", &m_iconSize, 40.0f, 120.0f, "%.0f px");

			struct Item
			{
				std::string name, rel;
				bool folder = false, linked = false, known = false;
				editor::AssetType type = editor::AssetType::Texture;
			};
			std::vector<Item> items;
			const std::string suffix = eng::kAssetMetaSuffix;
			std::error_code ec;
			for (const auto &entry : fs::directory_iterator(dir, ec))
			{
				Item it;
				it.name = entry.path().filename().string();
				it.folder = entry.is_directory();
				it.rel = fs::relative(entry.path(), root, ec).generic_string();
				if (!it.folder)
				{
					if (it.name.size() > suffix.size() && it.name.compare(it.name.size() - suffix.size(), suffix.size(), suffix) == 0)
					{
						const std::string base = it.name.substr(0, it.name.size() - suffix.size());
						if (fs::exists(dir / base))
							continue;
						it.name = base;
						it.rel = (fs::path(m_browseDir) / base).generic_string();
						it.linked = true;
					}
					it.known = editor::ClassifyAsset(it.name, it.type);
				}
				items.push_back(std::move(it));
			}
			std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.folder != b.folder ? a.folder : a.name < b.name; });

			ImGui::BeginChild("##browser", ImVec2(0, 0), ImGuiChildFlags_Borders);
			const float cell = m_iconSize + 16.0f;
			const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cell));
			int column = 0;
			for (const Item &it : items)
			{
				ImGui::PushID(it.rel.c_str());
				ImGui::BeginGroup();
				const ImVec2 p = ImGui::GetCursorScreenPos();
				const bool selected = it.rel == m_browseSelected;
				if (ImGui::Selectable("##item", selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(cell - 4.0f, m_iconSize + ImGui::GetTextLineHeightWithSpacing() + 4.0f)))
				{
					m_browseSelected = it.rel;
					if (it.folder && ImGui::IsMouseDoubleClicked(0))
						m_browseDir = it.rel;
					else if (!it.folder && it.known && it.type == editor::AssetType::Model && ImGui::IsMouseDoubleClicked(0))
						AddModelObject(it.rel);
				}
				ImDrawList *dl = ImGui::GetWindowDrawList();
				const ImVec2 iconPos(p.x + (cell - 4.0f - m_iconSize) * 0.5f, p.y + 2.0f);
				if (it.folder)
					DrawFolderIcon(dl, iconPos, m_iconSize);
				else
				{
					std::string ext = fs::path(it.name).extension().string();
					if (!ext.empty())
						ext.erase(0, 1);
					for (char &ch : ext)
						ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
					DrawFileIcon(dl, iconPos, m_iconSize, TintFor(it.type, it.known), ext.c_str(), it.linked);
				}
				std::string label = it.name;
				while (label.size() > 3 && ImGui::CalcTextSize(label.c_str()).x > cell - 8.0f)
					label.resize(label.size() - 2), label.back() = '.';
				dl->AddText(ImVec2(p.x + 2.0f, p.y + m_iconSize + 4.0f), ImGui::GetColorU32(ImGuiCol_Text), label.c_str());

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s%s", it.rel.c_str(), it.linked ? "  (linked)" : "");
				if (!it.folder && it.known && ImGui::BeginPopupContextItem("##ctx"))
				{
					if (it.type == editor::AssetType::Model && ImGui::MenuItem("Add to scene"))
						AddModelObject(it.rel);
					if (it.type == editor::AssetType::Texture && ImGui::MenuItem("Use as terrain height map"))
					{
						m_scene.terrain.heightMap = it.rel;
						ApplyTerrain();
						m_dirty = true;
					}
					if (it.type == editor::AssetType::Hdr && ImGui::MenuItem("Use as sky"))
					{
						m_scene.environment.skyHdr = it.rel;
						m_dirty = true;
					}
					ImGui::EndPopup();
				}
				ImGui::EndGroup();
				ImGui::PopID();

				if (++column < columns)
					ImGui::SameLine();
				else
					column = 0;
			}
			ImGui::EndChild();
		}

		void AddModelObject(const std::string &rel)
		{
			const std::string stem = fs::path(rel).stem().string();
			const int mesh = FindOrAddMesh(stem, "model", {});
			m_scene.meshes[mesh].path = rel;
			eng::ObjectDesc o = MakeEmpty(stem);
			o.mesh = mesh;
			AddObject(std::move(o));
		}

		void DrawMaterials()
		{
			ImGui::Begin("Materials", &m_show.materials);
			auto &mats = m_scene.materials;
			if (ImGui::Button("Add"))
			{
				eng::MaterialDesc m;
				m.name = "Material " + std::to_string(mats.size());
				mats.push_back(m);
				m_selectedMaterial = static_cast<int>(mats.size()) - 1;
				m_dirty = true;
			}
			const bool valid = m_selectedMaterial >= 0 && m_selectedMaterial < static_cast<int>(mats.size());
			if (valid)
			{
				ImGui::SameLine();
				if (ImGui::Button("Duplicate"))
				{
					eng::MaterialDesc copy = mats[m_selectedMaterial];
					copy.name += " copy";
					mats.push_back(copy);
					m_selectedMaterial = static_cast<int>(mats.size()) - 1;
					m_dirty = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("Delete"))
				{
					const int del = m_selectedMaterial;
					for (eng::ObjectDesc &o : m_scene.objects)
					{
						if (o.material == del)
							o.material = -1;
						else if (o.material > del)
							--o.material;
					}
					mats.erase(mats.begin() + del);
					m_selectedMaterial = -1;
					m_dirty = true;
					m_view.Invalidate();
				}
			}
			if (ImGui::BeginListBox("##mats", ImVec2(-FLT_MIN, 110)))
			{
				for (int i = 0; i < static_cast<int>(mats.size()); ++i)
				{
					ImGui::PushID(i);
					if (ImGui::Selectable(mats[i].name.c_str(), i == m_selectedMaterial))
						m_selectedMaterial = i;
					ImGui::PopID();
				}
				ImGui::EndListBox();
			}
			if (m_selectedMaterial < 0 || m_selectedMaterial >= static_cast<int>(mats.size()))
			{
				ImGui::End();
				return;
			}
			eng::MaterialDesc &m = mats[m_selectedMaterial];
			bool changed = false, texChanged = false;
			ImGui::Separator();
			changed |= InputString("Name", m.name);
			changed |= ImGui::ColorEdit3("Color", &m.color.x);
			changed |= ImGui::SliderFloat("Metallic", &m.metallic, 0.0f, 1.0f);
			changed |= ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f);
			changed |= ImGui::ColorEdit3("Emissive", &m.emissive.x, ImGuiColorEditFlags_HDR);
			texChanged |= AssetCombo("Albedo", m.albedoMap, editor::AssetType::Texture, m_assets);
			texChanged |= AssetCombo("Normal", m.normalMap, editor::AssetType::Texture, m_assets);
			texChanged |= AssetCombo("Metal/Rough", m.metalRoughMap, editor::AssetType::Texture, m_assets);
			changed |= ImGui::DragFloat("Wind strength", &m.windStrength, 0.01f, 0.0f, 5.0f);
			changed |= ImGui::Checkbox("Double sided", &m.doubleSided);

			if (ImGui::BeginCombo("Shader", m.shader.empty() ? "Standard" : m.shader.c_str()))
			{
				const fs::path dir = fs::path(SHADER_DIR) / "materials";
				if (ImGui::Selectable("Standard", m.shader.empty()))
				{
					m.shader.clear();
					changed = true;
				}
				if (fs::exists(dir))
					for (const auto &e : fs::directory_iterator(dir))
					{
						const std::string stem = e.path().stem().string();
						if (e.path().extension() == ".frag" && stem != "Standard" && ImGui::Selectable(stem.c_str(), stem == m.shader))
						{
							m.shader = stem;
							changed = true;
						}
					}
				ImGui::EndCombo();
			}
			if (HasSelection() && ImGui::Button("Assign to selected object"))
			{
				m_scene.objects[m_selected].material = m_selectedMaterial;
				changed = true;
			}
			if (texChanged)
				m_view.ReloadTextures();
			if (changed || texChanged)
			{
				m_dirty = true;
				m_view.Invalidate();
			}
			ImGui::End();
		}

		void DrawInspector()
		{
			ImGui::Begin("Inspector", &m_show.inspector);
			if (!HasSelection())
			{
				ImGui::TextDisabled("Nothing selected.");
				ImGui::End();
				return;
			}
			eng::ObjectDesc &o = m_scene.objects[m_selected];
			bool changed = false;
			if (!ImGui::IsAnyItemActive())
				m_euler = glm::degrees(glm::eulerAngles(o.rotation)); // follows gizmo rotation

			changed |= InputString("Name", o.name);
			changed |= ImGui::DragFloat3("Position", &o.position.x, 0.05f);
			if (ImGui::DragFloat3("Rotation", &m_euler.x, 0.5f))
			{
				o.rotation = glm::quat(glm::radians(m_euler));
				changed = true;
			}
			changed |= ImGui::DragFloat3("Scale", &o.scale.x, 0.01f);

			ImGui::SeparatorText("Rendering");
			const int meshCount = static_cast<int>(m_scene.meshes.size());
			const int matCount = static_cast<int>(m_scene.materials.size());
			const auto meshName = [&](int i) { return i >= 0 && i < meshCount ? m_scene.meshes[i].name.c_str() : "(none)"; };
			const auto matName = [&](int i) { return i >= 0 && i < matCount ? m_scene.materials[i].name.c_str() : "(none)"; };
			if (ImGui::BeginCombo("Mesh", meshName(o.mesh)))
			{
				if (ImGui::Selectable("(none)", o.mesh < 0))
				{
					o.mesh = -1;
					changed = true;
				}
				for (int i = 0; i < meshCount; ++i)
				{
					ImGui::PushID(i);
					if (ImGui::Selectable(meshName(i), i == o.mesh))
					{
						o.mesh = i;
						changed = true;
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			if (o.mesh >= 0 && o.mesh < meshCount)
			{
				eng::MeshDesc &md = m_scene.meshes[o.mesh];
				ImGui::TextDisabled("Mesh kind: %s", md.kind.c_str());
				if (ImGui::BeginCombo("Change kind", md.kind.c_str()))
				{
					for (const char *kind : editor::KnownMeshKinds())
						if (ImGui::Selectable(kind, md.kind == kind))
						{
							md.kind = kind;
							m_view.Invalidate();
							changed = true;
						}
					ImGui::EndCombo();
				}
			}
			if (ImGui::BeginCombo("Material", matName(o.material)))
			{
				if (ImGui::Selectable("(none)", o.material < 0))
				{
					o.material = -1;
					changed = true;
				}
				for (int i = 0; i < matCount; ++i)
				{
					ImGui::PushID(i);
					if (ImGui::Selectable(matName(i), i == o.material))
					{
						o.material = i;
						changed = true;
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			if (o.material >= 0 && o.material < matCount)
			{
				eng::MaterialDesc &m = m_scene.materials[o.material];
				ImGui::TextDisabled("Shared material: edits affect every user.");
				changed |= ImGui::ColorEdit3("Color", &m.color.x);
				changed |= ImGui::SliderFloat("Metallic", &m.metallic, 0.0f, 1.0f);
				changed |= ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f);
				changed |= ImGui::ColorEdit3("Emissive", &m.emissive.x);
			}

			ImGui::SeparatorText("Physics / gameplay");
			changed |= ImGui::Checkbox("Has body", &o.hasBody);
			if (o.hasBody)
			{
				changed |= ImGui::Checkbox("Static", &o.bodyStatic);
				changed |= ImGui::DragFloat3("Half extents", &o.halfExtents.x, 0.02f, 0.0f, 1000.0f);
			}
			changed |= ImGui::DragFloat("Health", &o.health, 0.5f, 0.0f, 100000.0f);

			ImGui::SeparatorText("Script");
			const std::string current = o.script.empty() ? "(none)" : o.script;
			if (ImGui::BeginCombo("Script", current.c_str()))
			{
				if (ImGui::Selectable("(none)", o.script.empty()))
				{
					o.script.clear();
					o.scriptParams.clear();
					changed = true;
				}
				for (const ScriptInfo &s : KnownScripts())
					if (ImGui::Selectable(s.name, o.script == s.name))
					{
						o.script = s.name;
						o.scriptParams.resize(s.params.size(), 0.0f);
						changed = true;
					}
				ImGui::EndCombo();
			}
			changed |= InputString("Script name", o.script);
			const ScriptInfo *info = nullptr;
			for (const ScriptInfo &s : KnownScripts())
				if (o.script == s.name)
					info = &s;
			if (info && o.scriptParams.size() < info->params.size())
				o.scriptParams.resize(info->params.size(), 0.0f);
			if (o.script == game::kScriptLight)
			{
				o.scriptParams.resize(8, 0.0f);
				auto &p = o.scriptParams;
				ImGui::SeparatorText("Light");
				int type = std::clamp(static_cast<int>(p[0]), 0, 2);
				if (ImGui::Combo("Type", &type, "Point\0Spot\0Directional\0"))
				{
					p[0] = static_cast<float>(type);
					changed = true;
				}
				changed |= ImGui::ColorEdit3("Light color", &p[1]);
				changed |= ImGui::DragFloat("Intensity", &p[4], 0.1f, 0.0f, 10000.0f);
				if (type != 2)
					changed |= ImGui::DragFloat("Range", &p[5], 0.2f, 0.1f, 5000.0f);
				if (type == 1)
				{
					changed |= ImGui::DragFloat("Inner angle", &p[6], 0.2f, 0.0f, 89.0f);
					changed |= ImGui::DragFloat("Outer angle", &p[7], 0.2f, 0.0f, 89.0f);
					p[6] = std::min(p[6], p[7]);
				}
				if (type != 0)
					ImGui::TextDisabled("Points along -Z; use the rotate gizmo to aim.");
			}
			else
			{
				for (size_t i = 0; i < o.scriptParams.size(); ++i)
				{
					ImGui::PushID(static_cast<int>(i));
					const std::string label = info && i < info->params.size() ? info->params[i] : "param " + std::to_string(i);
					changed |= ImGui::DragFloat(label.c_str(), &o.scriptParams[i], 0.05f);
					ImGui::PopID();
				}
			}

			if (o.camera.enabled)
			{
				ImGui::SeparatorText("Camera");
				if (ImGui::SmallButton("Remove##camera"))
				{
					o.camera.enabled = false;
					changed = true;
				}
				else
				{
					changed |= ImGui::Checkbox("Primary", &o.camera.primary);
					changed |= ImGui::SliderFloat("Field of view", &o.camera.fov, 10.0f, 120.0f, "%.0f deg");
					changed |= ImGui::DragFloat("Near", &o.camera.nearPlane, 0.01f, 0.01f, 10.0f);
					changed |= ImGui::DragFloat("Far", &o.camera.farPlane, 1.0f, 10.0f, 5000.0f);
					if (ImGui::Button("Look through camera"))
						m_view.SetViewFrom(o.position, o.rotation, o.camera.fov);
					ImGui::SameLine();
					if (ImGui::Button("Align to view"))
					{
						m_view.GetView(o.position, o.rotation);
						m_euler = glm::degrees(glm::eulerAngles(o.rotation));
						changed = true;
					}
				}
			}

			ImGui::Separator();
			if (ImGui::Button("Add Component..."))
				ImGui::OpenPopup("add_component");
			if (ImGui::BeginPopup("add_component"))
			{
				if (ImGui::MenuItem("Camera", nullptr, false, !o.camera.enabled))
				{
					o.camera.enabled = true;
					changed = true;
				}
				if (ImGui::MenuItem("Mesh", nullptr, false, o.mesh < 0 && !m_scene.meshes.empty()))
				{
					o.mesh = 0;
					changed = true;
				}
				if (ImGui::MenuItem("Material", nullptr, false, o.material < 0 && !m_scene.materials.empty()))
				{
					o.material = 0;
					changed = true;
				}
				if (ImGui::MenuItem("Rigid body", nullptr, false, !o.hasBody))
				{
					o.hasBody = true;
					changed = true;
				}
				if (ImGui::MenuItem("Health", nullptr, false, o.health <= 0.0f))
				{
					o.health = 100.0f;
					changed = true;
				}
				if (ImGui::MenuItem("Script", nullptr, false, o.script.empty()))
				{
					o.script = KnownScripts().front().name;
					o.scriptParams.assign(KnownScripts().front().params.size(), 0.0f);
					changed = true;
				}
				ImGui::EndPopup();
			}
			ImGui::TextDisabled("Remove: (none) in Mesh/Material/Script, untick Has body, Health 0, or Camera > Remove.");

			if (changed)
				m_dirty = true;
			ImGui::End();
		}

		// Top-down (XZ) view: right/middle-drag pans, wheel zooms, click selects, left-drag moves the selection.
		void DrawViewport()
		{
			ImGui::Begin("Top-down", &m_show.topDown);
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			ImVec2 size = ImGui::GetContentRegionAvail();
			size.x = std::max(size.x, 64.0f);
			size.y = std::max(size.y, 64.0f);
			const ImVec2 end(origin.x + size.x, origin.y + size.y);
			ImGui::InvisibleButton("canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
			const bool hovered = ImGui::IsItemHovered();
			const bool active = ImGui::IsItemActive();
			ImGuiIO &io = ImGui::GetIO();

			if (hovered && io.MouseWheel != 0.0f)
				m_zoom = std::clamp(m_zoom * (io.MouseWheel > 0 ? 1.15f : 1.0f / 1.15f), 0.05f, 200.0f);
			if (active && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || ImGui::IsMouseDragging(ImGuiMouseButton_Right)))
			{
				m_pan.x -= io.MouseDelta.x / m_zoom;
				m_pan.y -= io.MouseDelta.y / m_zoom;
			}

			const ImVec2 centre(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f);
			const auto toScreen = [&](float x, float z) { return ImVec2(centre.x + (x - m_pan.x) * m_zoom, centre.y + (z - m_pan.y) * m_zoom); };

			ImDrawList *dl = ImGui::GetWindowDrawList();
			dl->PushClipRect(origin, end, true);
			dl->AddRectFilled(origin, end, IM_COL32(24, 26, 30, 255));

			float step = 1.0f;
			while (step * m_zoom < 12.0f)
				step *= 10.0f;
			const float x0 = m_pan.x - size.x * 0.5f / m_zoom, x1 = m_pan.x + size.x * 0.5f / m_zoom;
			const float z0 = m_pan.y - size.y * 0.5f / m_zoom, z1 = m_pan.y + size.y * 0.5f / m_zoom;
			for (float x = std::floor(x0 / step) * step; x <= x1; x += step)
				dl->AddLine(toScreen(x, z0), toScreen(x, z1), x == 0.0f ? IM_COL32(110, 50, 50, 255) : IM_COL32(45, 48, 54, 255));
			for (float z = std::floor(z0 / step) * step; z <= z1; z += step)
				dl->AddLine(toScreen(x0, z), toScreen(x1, z), z == 0.0f ? IM_COL32(50, 50, 110, 255) : IM_COL32(45, 48, 54, 255));

			int hit = -1;
			float best = 1e9f;
			for (int i = 0; i < static_cast<int>(m_scene.objects.size()); ++i)
			{
				const eng::ObjectDesc &o = m_scene.objects[i];
				const ImVec2 p = toScreen(o.position.x, o.position.z);
				const float r = std::max(3.0f, 0.5f * std::max(o.scale.x, o.scale.z) * m_zoom);
				ImU32 col = IM_COL32(180, 180, 190, 255);
				if (o.material >= 0 && o.material < static_cast<int>(m_scene.materials.size()))
				{
					const glm::vec3 c = glm::clamp(m_scene.materials[o.material].color, 0.0f, 1.0f) * 255.0f;
					col = IM_COL32(static_cast<int>(c.x), static_cast<int>(c.y), static_cast<int>(c.z), 255);
				}
				dl->AddRect(ImVec2(p.x - r, p.y - r), ImVec2(p.x + r, p.y + r), col, 0.0f, 0, i == m_selected ? 3.0f : 1.0f);
				if (i == m_selected)
					dl->AddRect(ImVec2(p.x - r - 3, p.y - r - 3), ImVec2(p.x + r + 3, p.y + r + 3), IM_COL32(255, 200, 40, 255));

				const float d = std::max(std::abs(io.MousePos.x - p.x), std::abs(io.MousePos.y - p.y));
				if (d <= std::max(r, 6.0f) && d < best)
				{
					best = d;
					hit = i;
				}
			}
			dl->PopClipRect();

			if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hit >= 0)
				Select(hit);
			if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && HasSelection())
			{
				eng::ObjectDesc &o = m_scene.objects[m_selected];
				o.position.x += io.MouseDelta.x / m_zoom;
				o.position.z += io.MouseDelta.y / m_zoom;
				m_dirty = true;
			}
			ImGui::End();
		}

		eng::SceneData m_scene;
		game::Terrain m_terrain;
		editor::SceneView3D m_view;
		editor::UiEditor m_uiEditor;
		editor::BehaviorEditor m_behaviorEditor;
		editor::AnimationEditor m_animEditor;
		struct WindowFlags
		{
			bool worlds = true, assets = true, hierarchy = true, inspector = true, materials = true, environment = true, topDown = true;
		} m_show;
		bool m_layoutBuilt = false;
		std::vector<std::string> m_worlds;
		std::string m_world;
		std::string m_status = "Ready";
		char m_filter[64] = {};
		int m_selected = -1;
		glm::vec3 m_euler{0.0f};
		glm::vec2 m_pan{0.0f};
		float m_zoom = 4.0f;
		bool m_dirty = false;
		bool m_quit = false;
		std::unique_ptr<editor::PlaySession> m_session;
		core::Window *m_window = nullptr;
		bool m_paused = false;
		std::string m_browseDir;
		std::string m_browseSelected;
		float m_iconSize = 64.0f;
		std::vector<editor::AssetEntry> m_assets;
		int m_selectedMaterial = -1;
		char m_importPath[512] = {};
		editor::ImportOptions m_import;
	};

} // namespace

int main(int argc, char **argv)
{
	try
	{
		core::Window window(1600, 900, "Game Editor");
		window.SetVSync(true);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ApplyTheme();
		{
			const std::string fontPath = std::string(ASSETS_DIR) + "fonts/Roboto-Regular.ttf";
			if (fs::exists(fontPath))
			{
				ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
				if (font)
					ImGui::GetIO().FontDefault = font;
			}
			else
				spdlog::warn("Roboto font not found at {}; using the default font", fontPath);
		}
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
		ImGui_ImplGlfw_InitForOpenGL(window.Handle(), true);
		glfwSetDropCallback(
			window.Handle(),
			[](GLFWwindow *, int count, const char **paths)
			{
				for (int i = 0; i < count; ++i)
					editor::QueueDroppedFile(paths[i]);
			}
		);
		ImGui_ImplOpenGL3_Init("#version 460");

		{
			Editor editor;
			editor.SetWindow(&window);
			if (argc > 1)
				editor.OpenWorld(argv[1]);
			while (!window.ShouldClose() && !editor.WantsQuit())
			{
				window.PollEvents();
				ImGui_ImplOpenGL3_NewFrame();
				ImGui_ImplGlfw_NewFrame();
				ImGui::NewFrame();
				ImGuizmo::BeginFrame();
				editor.Draw();
				ImGui::Render();

				const glm::ivec2 fb = window.FramebufferSize();
				glViewport(0, 0, fb.x, fb.y);
				glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT);
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
				window.SwapBuffers();
			}
		}

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
	catch (const std::exception &e)
	{
		spdlog::critical("{}", e.what());
		return 1;
	}
	return 0;
}
