#include "editor/BehaviorEditor.h"
#include "editor/ImGuiUtil.h"
#include <algorithm>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace editor
{

	namespace
	{
		int NextId()
		{
			static int counter = 0;
			return ++counter;
		}
	} // namespace

	BtNode::BtNode() : id(NextId()) {}

	template <class Archive>
	void serialize(Archive &ar, BtNode &n)
	{
		ar(cereal::make_nvp("type", n.type), cereal::make_nvp("name", n.name), cereal::make_nvp("a", n.a), cereal::make_nvp("b", n.b),
		   cereal::make_nvp("key", n.key), cereal::make_nvp("children", n.children));
	}

	namespace
	{
		constexpr const char *kExt = ".bt.json";
		constexpr float kBoxW = 150.0f, kBoxH = 46.0f, kGapX = 24.0f, kGapY = 56.0f;

		struct TypeInfo
		{
			const char *name;
			int maxChildren; // -1 = unlimited
			const char *aLabel;
			const char *bLabel;
			ImU32 color;
		};

		constexpr TypeInfo kTypes[] = {
			{"Root", 1, nullptr, nullptr, IM_COL32(110, 80, 30, 255)},
			{"Selector", -1, nullptr, nullptr, IM_COL32(120, 60, 60, 255)},
			{"Sequence", -1, nullptr, nullptr, IM_COL32(60, 100, 70, 255)},
			{"MemSequence", -1, nullptr, nullptr, IM_COL32(60, 110, 90, 255)},
			{"Inverter", 1, nullptr, nullptr, IM_COL32(100, 70, 110, 255)},
			{"Repeat", 1, "Count (0 = forever)", nullptr, IM_COL32(100, 70, 110, 255)},
			{"Wait", 0, "Duration", "Deviation", IM_COL32(80, 80, 80, 255)},
			{"MoveTo", 0, "Speed", "Stop distance", IM_COL32(110, 95, 50, 255)},
			{"FindRandomReachablePoint", 0, "Radius", nullptr, IM_COL32(110, 95, 50, 255)},
			{"Condition", 0, nullptr, nullptr, IM_COL32(120, 100, 40, 255)},
			{"Action", 0, nullptr, nullptr, IM_COL32(120, 100, 40, 255)},
		};

		const TypeInfo &Info(const std::string &type)
		{
			for (const TypeInfo &t : kTypes)
				if (type == t.name)
					return t;
			return kTypes[2];
		}

		fs::path Dir() { return fs::path(ASSETS_DIR) / "behavior"; }
		std::string PathFor(const std::string &name) { return (Dir() / (name + kExt)).string(); }

		bool CanAdd(const BtNode &n)
		{
			const int max = Info(n.type).maxChildren;
			return max < 0 || static_cast<int>(n.children.size()) < max;
		}
	} // namespace

	BehaviorEditor::BehaviorEditor()
	{
		m_root.type = "Root";
		Refresh();
	}

	void BehaviorEditor::Refresh()
	{
		m_trees.clear();
		std::error_code ec;
		const std::string ext = kExt;
		for (const auto &entry : fs::directory_iterator(Dir(), ec))
		{
			const std::string file = entry.path().filename().string();
			if (file.size() > ext.size() && file.compare(file.size() - ext.size(), ext.size(), ext) == 0)
				m_trees.push_back(file.substr(0, file.size() - ext.size()));
		}
		std::sort(m_trees.begin(), m_trees.end());
	}

	void BehaviorEditor::Open(const std::string &name)
	{
		try
		{
			std::ifstream in(PathFor(name));
			if (!in)
				throw std::runtime_error("cannot open file");
			cereal::JSONInputArchive ar(in);
			BtNode root;
			ar(cereal::make_nvp("root", root));
			m_root = std::move(root);
			m_name = name;
			m_selectedId = -1;
			m_dirty = false;
			m_status = "Opened " + name;
		}
		catch (const std::exception &e)
		{
			m_status = std::string("Open failed: ") + e.what();
		}
	}

	void BehaviorEditor::Save()
	{
		if (m_name.empty())
			return;
		try
		{
			fs::create_directories(Dir());
			std::ofstream out(PathFor(m_name));
			if (!out)
				throw std::runtime_error("cannot write file");
			{
				cereal::JSONOutputArchive ar(out);
				ar(cereal::make_nvp("root", m_root));
			}
			m_dirty = false;
			m_status = "Saved " + m_name;
			Refresh();
		}
		catch (const std::exception &e)
		{
			m_status = std::string("Save failed: ") + e.what();
		}
	}

	bool BehaviorEditor::Find(BtNode &node, BtNode *parent, int index, int id, BtNode *&outNode, BtNode *&outParent, int &outIndex)
	{
		if (node.id == id)
		{
			outNode = &node;
			outParent = parent;
			outIndex = index;
			return true;
		}
		for (size_t i = 0; i < node.children.size(); ++i)
			if (Find(node.children[i], &node, static_cast<int>(i), id, outNode, outParent, outIndex))
				return true;
		return false;
	}

	// Tidy top-down layout: leaves take consecutive slots, parents centre over their children.
	void BehaviorEditor::Layout(const BtNode &node, int depth, float &nextX)
	{
		float x;
		if (node.children.empty())
		{
			x = nextX;
			nextX += kBoxW + kGapX;
		}
		else
		{
			for (const BtNode &c : node.children)
				Layout(c, depth + 1, nextX);
			x = (m_pos[node.children.front().id].x + m_pos[node.children.back().id].x) * 0.5f;
		}
		m_pos[node.id] = {x, static_cast<float>(depth) * (kBoxH + kGapY)};
	}

	void BehaviorEditor::DrawCanvas()
	{
		const ImVec2 size = ImGui::GetContentRegionAvail();
		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		ImDrawList *dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(p0, ImVec2(p0.x + size.x, p0.y + size.y), IM_COL32(24, 24, 24, 255));
		dl->PushClipRect(p0, ImVec2(p0.x + size.x, p0.y + size.y), true);

		ImGui::InvisibleButton("##bt_canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
		{
			m_panX += ImGui::GetIO().MouseDelta.x;
			m_panY += ImGui::GetIO().MouseDelta.y;
		}
		const bool canvasClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		const bool canvasRightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
		const ImVec2 mouse = ImGui::GetIO().MousePos;

		if ((ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) && !ImGui::GetIO().WantTextInput &&
			ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
			DeleteSelected();

		constexpr float kGrid = 32.0f;
		for (float gx = std::fmod(m_panX, kGrid); gx < size.x; gx += kGrid)
			dl->AddLine(ImVec2(p0.x + gx, p0.y), ImVec2(p0.x + gx, p0.y + size.y), IM_COL32(255, 255, 255, 14));
		for (float gy = std::fmod(m_panY, kGrid); gy < size.y; gy += kGrid)
			dl->AddLine(ImVec2(p0.x, p0.y + gy), ImVec2(p0.x + size.x, p0.y + gy), IM_COL32(255, 255, 255, 14));

		m_pos.clear();
		float nextX = 0.0f;
		Layout(m_root, 0, nextX);
		const ImVec2 origin(p0.x + m_panX, p0.y + m_panY);

		const auto drawLinks = [&](auto &&self, const BtNode &n) -> void
		{
			const Pos a = m_pos[n.id];
			for (const BtNode &c : n.children)
			{
				const Pos b = m_pos[c.id];
				const ImVec2 s(origin.x + a.x + kBoxW * 0.5f, origin.y + a.y + kBoxH);
				const ImVec2 e(origin.x + b.x + kBoxW * 0.5f, origin.y + b.y);
				dl->AddBezierCubic(s, ImVec2(s.x, s.y + kGapY * 0.5f), ImVec2(e.x, e.y - kGapY * 0.5f), e, IM_COL32(190, 190, 190, 255), 2.0f);
				self(self, c);
			}
		};
		drawLinks(drawLinks, m_root);

		int clicked = -1, rightClicked = -1, handleHit = -1, hovered = -1;
		const auto drawNodes = [&](auto &&self, BtNode &n, int order) -> void
		{
			const Pos p = m_pos[n.id];
			const ImVec2 min(origin.x + p.x, origin.y + p.y), max(min.x + kBoxW, min.y + kBoxH);
			const ImVec2 handle(min.x + kBoxW * 0.5f, max.y);
			const bool inBox = mouse.x >= min.x && mouse.x <= max.x && mouse.y >= min.y && mouse.y <= max.y;
			const float hdx = mouse.x - handle.x, hdy = mouse.y - handle.y;
			const bool onHandle = CanAdd(n) && hdx * hdx + hdy * hdy <= 9.0f * 9.0f;
			if (inBox || onHandle)
				hovered = n.id;
			if (canvasClicked && onHandle)
				handleHit = n.id;
			else if (canvasClicked && inBox)
				clicked = n.id;
			if (canvasRightClicked && inBox)
				rightClicked = n.id;
			const bool selected = n.id == m_selectedId;
			dl->AddRectFilled(min, max, Info(n.type).color, 6.0f);
			dl->AddRect(min, max, selected ? IM_COL32(255, 190, 60, 255) : IM_COL32(120, 120, 120, 255), 6.0f, 0, selected ? 2.5f : 1.0f);
			dl->AddText(ImVec2(min.x + 8, min.y + 6), IM_COL32(240, 240, 240, 255), n.type.c_str());
			if (!n.name.empty() || !n.key.empty())
				dl->AddText(ImVec2(min.x + 8, min.y + 26), IM_COL32(170, 170, 170, 255), (n.name.empty() ? n.key : n.name).c_str());
			if (n.type != "Root" && order >= 0)
				dl->AddText(ImVec2(max.x - 18, min.y + 4), IM_COL32(150, 150, 150, 255), std::to_string(order + 1).c_str());
			if (CanAdd(n))
				dl->AddCircleFilled(handle, onHandle || m_linkFrom == n.id ? 7.0f : 5.0f, onHandle || m_linkFrom == n.id ? IM_COL32(255, 190, 60, 255) : IM_COL32(200, 200, 200, 255));
			for (size_t i = 0; i < n.children.size(); ++i)
				self(self, n.children[i], static_cast<int>(i));
		};
		drawNodes(drawNodes, m_root, -1);

		if (handleHit >= 0)
		{
			m_linkFrom = handleHit;
			m_selectedId = handleHit;
		}
		else if (clicked >= 0)
		{
			m_selectedId = clicked;
			m_dragId = clicked;
			m_dragStart = {mouse.x, mouse.y};
			m_dragActive = false;
		}
		else if (canvasClicked)
			m_selectedId = -1;
		if (rightClicked >= 0)
		{
			m_selectedId = rightClicked;
			ImGui::OpenPopup("bt_node_menu");
		}
		else if (canvasRightClicked)
			ImGui::OpenPopup("bt_canvas_menu");

		// Dragging a node reorders it among its siblings by horizontal position.
		if (m_dragId >= 0)
		{
			BtNode *dragged = nullptr, *dragParent = nullptr;
			int dragIndex = 0;
			if (!Find(m_root, nullptr, 0, m_dragId, dragged, dragParent, dragIndex) || !dragParent)
				m_dragId = -1;
			else
			{
				if (!m_dragActive && std::hypot(mouse.x - m_dragStart.x, mouse.y - m_dragStart.y) > 6.0f)
					m_dragActive = true;
				int target = 0;
				if (m_dragActive)
				{
					for (const BtNode &c : dragParent->children)
						if (c.id != m_dragId && origin.x + m_pos[c.id].x + kBoxW * 0.5f < mouse.x)
							++target;
					const ImVec2 ghost(mouse.x - kBoxW * 0.5f, mouse.y - kBoxH * 0.5f);
					dl->AddRectFilled(ghost, ImVec2(ghost.x + kBoxW, ghost.y + kBoxH), IM_COL32(255, 190, 60, 70), 6.0f);
					dl->AddRect(ghost, ImVec2(ghost.x + kBoxW, ghost.y + kBoxH), IM_COL32(255, 190, 60, 255), 6.0f);
					// Marker at the insertion slot among the other siblings.
					float markX = origin.x;
					int slot = 0;
					for (const BtNode &c : dragParent->children)
					{
						if (c.id == m_dragId)
							continue;
						const float left = origin.x + m_pos[c.id].x;
						markX = slot < target ? left + kBoxW + kGapX * 0.5f : (slot == target ? left - kGapX * 0.5f : markX);
						++slot;
					}
					const float py = origin.y + m_pos[dragParent->children.front().id].y;
					dl->AddLine(ImVec2(markX, py), ImVec2(markX, py + kBoxH), IM_COL32(255, 190, 60, 255), 3.0f);
				}
				if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					if (m_dragActive)
					{
						BtNode moved = std::move(*dragged);
						auto &siblings = dragParent->children;
						siblings.erase(siblings.begin() + dragIndex);
						siblings.insert(siblings.begin() + std::min<size_t>(target, siblings.size()), std::move(moved));
						m_dirty = true;
					}
					m_dragId = -1;
					m_dragActive = false;
				}
			}
		}

		if (m_linkFrom >= 0)
		{
			const Pos a = m_pos[m_linkFrom];
			const ImVec2 s(origin.x + a.x + kBoxW * 0.5f, origin.y + a.y + kBoxH);
			dl->AddBezierCubic(s, ImVec2(s.x, s.y + kGapY * 0.5f), ImVec2(mouse.x, mouse.y - kGapY * 0.5f), mouse, IM_COL32(255, 190, 60, 255), 2.0f);
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			{
				if (hovered < 0 || hovered == m_linkFrom)
				{
					m_pendingParent = m_linkFrom;
					ImGui::OpenPopup("bt_add_menu");
				}
				m_linkFrom = -1;
			}
		}
		dl->PopClipRect();

		const auto typeMenu = [&](int parentId)
		{
			for (const TypeInfo &t : kTypes)
				if (std::string(t.name) != "Root" && ImGui::MenuItem(t.name))
					AddChild(parentId, t.name);
		};
		if (ImGui::BeginPopup("bt_add_menu"))
		{
			ImGui::TextDisabled("Add child node");
			ImGui::Separator();
			typeMenu(m_pendingParent);
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("bt_node_menu"))
		{
			BtNode *node = nullptr, *parent = nullptr;
			int index = 0;
			if (Find(m_root, nullptr, 0, m_selectedId, node, parent, index))
			{
				if (ImGui::BeginMenu("Add child", CanAdd(*node)))
				{
					typeMenu(node->id);
					ImGui::EndMenu();
				}
				if (parent)
				{
					auto &siblings = parent->children;
					if (ImGui::MenuItem("Move left", nullptr, false, index > 0))
					{
						std::swap(siblings[index], siblings[index - 1]);
						m_dirty = true;
					}
					if (ImGui::MenuItem("Move right", nullptr, false, index + 1 < static_cast<int>(siblings.size())))
					{
						std::swap(siblings[index], siblings[index + 1]);
						m_dirty = true;
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Delete", "Del"))
						DeleteSelected();
				}
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("bt_canvas_menu"))
		{
			if (ImGui::MenuItem("Reset view"))
				m_panX = m_panY = 20.0f;
			ImGui::EndPopup();
		}
	}

	void BehaviorEditor::DeleteSelected()
	{
		BtNode *node = nullptr, *parent = nullptr;
		int index = 0;
		if (m_selectedId >= 0 && Find(m_root, nullptr, 0, m_selectedId, node, parent, index) && parent)
		{
			parent->children.erase(parent->children.begin() + index);
			m_selectedId = -1;
			m_dirty = true;
		}
	}

	void BehaviorEditor::AddChild(int parentId, const char *type)
	{
		BtNode *node = nullptr, *parent = nullptr;
		int index = 0;
		if (!Find(m_root, nullptr, 0, parentId, node, parent, index) || !CanAdd(*node))
			return;
		BtNode child;
		child.type = type;
		m_selectedId = child.id;
		node->children.push_back(std::move(child));
		m_dirty = true;
	}

	void BehaviorEditor::DrawProperties(BtNode &node, BtNode *parent, int index)
	{
		const TypeInfo &info = Info(node.type);
		if (ImGui::BeginCombo("Type", node.type.c_str()))
		{
			for (const TypeInfo &t : kTypes)
				if (ImGui::Selectable(t.name, node.type == t.name))
				{
					node.type = t.name;
					const int max = Info(node.type).maxChildren;
					if (max >= 0 && static_cast<int>(node.children.size()) > max)
						node.children.resize(max);
					m_dirty = true;
				}
			ImGui::EndCombo();
		}
		m_dirty |= InputText("Name", node.name);
		if (info.aLabel)
			m_dirty |= ImGui::DragFloat(info.aLabel, &node.a, 0.05f);
		if (info.bLabel)
			m_dirty |= ImGui::DragFloat(info.bLabel, &node.b, 0.05f);
		if (node.type == "Condition" || node.type == "Action" || node.type == "MoveTo" || node.type == "FindRandomReachablePoint")
			m_dirty |= InputText(node.type == "Condition" || node.type == "Action" ? "Callback" : "Blackboard key", node.key);

		if (CanAdd(node))
		{
			ImGui::SeparatorText("Add child");
			for (const TypeInfo &t : kTypes)
			{
				if (std::string(t.name) == "Root")
					continue;
				if (ImGui::SmallButton(t.name))
				{
					BtNode child;
					child.type = t.name;
					m_selectedId = child.id;
					node.children.push_back(std::move(child));
					m_dirty = true;
					break;
				}
			}
		}

		if (parent)
		{
			ImGui::SeparatorText("Arrange");
			auto &siblings = parent->children;
			const int n = static_cast<int>(siblings.size());
			if (ImGui::Button("Move left") && index > 0)
			{
				std::swap(siblings[index], siblings[index - 1]);
				m_dirty = true;
			}
			else if (ImGui::SameLine(), ImGui::Button("Move right") && index + 1 < n)
			{
				std::swap(siblings[index], siblings[index + 1]);
				m_dirty = true;
			}
			else if (ImGui::SameLine(), ImGui::Button("Delete"))
			{
				siblings.erase(siblings.begin() + index);
				m_selectedId = -1;
				m_dirty = true;
			}
		}
	}

	void BehaviorEditor::Draw()
	{
		if (!open)
			return;
		if (!ImGui::Begin("Behavior Tree", &open))
		{
			ImGui::End();
			return;
		}

		ImGui::SetNextItemWidth(200.0f);
		if (ImGui::BeginCombo("##tree", m_name.empty() ? "(no tree)" : (m_name + (m_dirty ? "*" : "")).c_str()))
		{
			for (const std::string &t : m_trees)
				if (ImGui::Selectable(t.c_str(), t == m_name))
					Open(t);
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(140.0f);
		InputText("##newname", m_newName);
		ImGui::SameLine();
		if (ImGui::Button("New") && !m_newName.empty())
		{
			m_name = m_newName;
			m_newName.clear();
			m_root = BtNode();
			m_root.type = "Root";
			m_selectedId = -1;
			Save();
		}
		ImGui::SameLine();
		if (ImGui::Button("Save") && !m_name.empty())
			Save();
		ImGui::SameLine();
		ImGui::TextDisabled("%s  (click nodes, middle-drag pans)", m_status.c_str());
		ImGui::Separator();

		ImGui::BeginChild("##bt_canvas_child", ImVec2(ImGui::GetContentRegionAvail().x * 0.68f, 0.0f), ImGuiChildFlags_Borders,
						  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		DrawCanvas();
		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::BeginChild("##bt_props");
		BtNode *node = nullptr, *parent = nullptr;
		int index = 0;
		if (m_selectedId >= 0 && Find(m_root, nullptr, 0, m_selectedId, node, parent, index))
			DrawProperties(*node, parent, index);
		else
			ImGui::TextDisabled("Select a node.");
		ImGui::EndChild();
		ImGui::End();
	}

} // namespace editor
