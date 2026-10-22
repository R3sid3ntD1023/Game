#include "editor/AnimationEditor.h"
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

	template <class Archive>
	void serialize(Archive &ar, AnimParam &p)
	{
		ar(cereal::make_nvp("name", p.name), cereal::make_nvp("isBool", p.isBool), cereal::make_nvp("default", p.defaultValue));
	}

	template <class Archive>
	void serialize(Archive &ar, AnimState &s)
	{
		ar(cereal::make_nvp("name", s.name), cereal::make_nvp("clip", s.clip), cereal::make_nvp("speed", s.speed), cereal::make_nvp("loop", s.loop),
		   cereal::make_nvp("x", s.x), cereal::make_nvp("y", s.y));
	}

	template <class Archive>
	void serialize(Archive &ar, AnimTransition &t)
	{
		ar(cereal::make_nvp("from", t.from), cereal::make_nvp("to", t.to), cereal::make_nvp("param", t.param), cereal::make_nvp("op", t.op),
		   cereal::make_nvp("value", t.value), cereal::make_nvp("blendTime", t.blendTime));
	}

	template <class Archive>
	void serialize(Archive &ar, AnimGraph &g)
	{
		ar(cereal::make_nvp("params", g.params), cereal::make_nvp("states", g.states), cereal::make_nvp("transitions", g.transitions),
		   cereal::make_nvp("entry", g.entry));
	}

	namespace
	{
		constexpr const char *kExt = ".anim.json";
		constexpr ImVec2 kBox(140.0f, 46.0f);

		fs::path Dir() { return fs::path(ASSETS_DIR) / "animation"; }
		std::string PathFor(const std::string &name) { return (Dir() / (name + kExt)).string(); }

		ImVec2 Center(const ImVec2 &origin, const AnimState &s) { return ImVec2(origin.x + s.x + kBox.x * 0.5f, origin.y + s.y + kBox.y * 0.5f); }

		// Point where the line from the box centre towards `to` leaves the box.
		ImVec2 EdgePoint(const ImVec2 &c, const ImVec2 &to)
		{
			float dx = to.x - c.x, dy = to.y - c.y;
			const float len = std::sqrt(dx * dx + dy * dy);
			if (len < 1e-3f)
				return c;
			dx /= len;
			dy /= len;
			const float tx = std::abs(dx) > 1e-4f ? kBox.x * 0.5f / std::abs(dx) : 1e9f;
			const float ty = std::abs(dy) > 1e-4f ? kBox.y * 0.5f / std::abs(dy) : 1e9f;
			const float t = std::min(tx, ty);
			return ImVec2(c.x + dx * t, c.y + dy * t);
		}
	} // namespace

	AnimationEditor::AnimationEditor()
	{
		Refresh();
	}

	void AnimationEditor::Refresh()
	{
		m_graphs.clear();
		std::error_code ec;
		const std::string ext = kExt;
		for (const auto &entry : fs::directory_iterator(Dir(), ec))
		{
			const std::string file = entry.path().filename().string();
			if (file.size() > ext.size() && file.compare(file.size() - ext.size(), ext.size(), ext) == 0)
				m_graphs.push_back(file.substr(0, file.size() - ext.size()));
		}
		std::sort(m_graphs.begin(), m_graphs.end());
	}

	void AnimationEditor::Open(const std::string &name)
	{
		try
		{
			std::ifstream in(PathFor(name));
			if (!in)
				throw std::runtime_error("cannot open file");
			cereal::JSONInputArchive ar(in);
			AnimGraph graph;
			ar(cereal::make_nvp("graph", graph));
			m_graph = std::move(graph);
			m_name = name;
			m_selectedState = m_selectedTransition = -1;
			m_dirty = false;
			m_status = "Opened " + name;
		}
		catch (const std::exception &e)
		{
			m_status = std::string("Open failed: ") + e.what();
		}
	}

	void AnimationEditor::Save()
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
				ar(cereal::make_nvp("graph", m_graph));
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

	void AnimationEditor::DrawCanvas()
	{
		const ImVec2 size = ImGui::GetContentRegionAvail();
		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		ImDrawList *dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(p0, ImVec2(p0.x + size.x, p0.y + size.y), IM_COL32(24, 24, 24, 255));
		dl->PushClipRect(p0, ImVec2(p0.x + size.x, p0.y + size.y), true);

		ImGui::InvisibleButton("##canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
		const ImGuiIO &io = ImGui::GetIO();
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
		{
			m_panX += io.MouseDelta.x;
			m_panY += io.MouseDelta.y;
		}
		const bool leftClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		const bool rightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
		const ImVec2 mouse = io.MousePos;
		const ImVec2 origin(p0.x + m_panX, p0.y + m_panY);

		if ((ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) && !io.WantTextInput &&
			ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		{
			if (m_selectedState >= 0)
				DeleteState(m_selectedState);
			else if (m_selectedTransition >= 0 && m_selectedTransition < static_cast<int>(m_graph.transitions.size()))
			{
				m_graph.transitions.erase(m_graph.transitions.begin() + m_selectedTransition);
				m_selectedTransition = -1;
				m_dirty = true;
			}
		}

		// Topmost state under the cursor, and whether the cursor is on its output handle (right edge).
		int hitState = -1;
		bool hitHandle = false;
		for (int i = static_cast<int>(m_graph.states.size()) - 1; i >= 0 && hitState < 0; --i)
		{
			const AnimState &s = m_graph.states[i];
			const float hx = origin.x + s.x + kBox.x, hy = origin.y + s.y + kBox.y * 0.5f;
			if ((mouse.x - hx) * (mouse.x - hx) + (mouse.y - hy) * (mouse.y - hy) <= 100.0f)
			{
				hitState = i;
				hitHandle = true;
			}
			else if (mouse.x >= origin.x + s.x && mouse.x <= origin.x + s.x + kBox.x && mouse.y >= origin.y + s.y && mouse.y <= origin.y + s.y + kBox.y)
				hitState = i;
		}
		if (leftClicked)
		{
			m_selectedTransition = -1;
			m_selectedState = hitState;
			if (hitState >= 0)
			{
				if (hitHandle)
					m_linkFrom = hitState;
				else
					m_dragging = hitState;
			}
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
			m_dragging = -1;
		if (m_dragging >= 0 && m_dragging < static_cast<int>(m_graph.states.size()) && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
		{
			m_graph.states[m_dragging].x += io.MouseDelta.x;
			m_graph.states[m_dragging].y += io.MouseDelta.y;
			m_dirty = true;
		}
		if (rightClicked)
		{
			m_contextState = hitState;
			m_contextPos = {mouse.x - origin.x, mouse.y - origin.y};
			if (hitState >= 0)
				m_selectedState = hitState;
			ImGui::OpenPopup("anim_menu");
		}

		for (int gx = static_cast<int>(std::fmod(m_panX, 40.0f)); gx < size.x; gx += 40)
			dl->AddLine(ImVec2(p0.x + gx, p0.y), ImVec2(p0.x + gx, p0.y + size.y), IM_COL32(255, 255, 255, 12));
		for (int gy = static_cast<int>(std::fmod(m_panY, 40.0f)); gy < size.y; gy += 40)
			dl->AddLine(ImVec2(p0.x, p0.y + gy), ImVec2(p0.x + size.x, p0.y + gy), IM_COL32(255, 255, 255, 12));

		const int stateCount = static_cast<int>(m_graph.states.size());
		for (int i = 0; i < static_cast<int>(m_graph.transitions.size()); ++i)
		{
			const AnimTransition &t = m_graph.transitions[i];
			if (t.from < 0 || t.to < 0 || t.from >= stateCount || t.to >= stateCount)
				continue;
			ImVec2 a = Center(origin, m_graph.states[t.from]), b = Center(origin, m_graph.states[t.to]);
			if (t.from == t.to)
			{
				const ImVec2 c(a.x, a.y - kBox.y * 0.5f - 12.0f);
				const bool selectedLoop = i == m_selectedTransition;
				dl->AddCircle(c, 12.0f, selectedLoop ? IM_COL32(255, 190, 60, 255) : IM_COL32(200, 200, 200, 255), 0, 2.0f);
				if (leftClicked && hitState < 0 && std::abs(std::hypot(mouse.x - c.x, mouse.y - c.y) - 12.0f) < 6.0f)
					m_selectedTransition = i;
				continue;
			}
			// Offset opposite directions so a pair of transitions does not overlap.
			const float dx = b.x - a.x, dy = b.y - a.y, len = std::max(std::sqrt(dx * dx + dy * dy), 1.0f);
			const ImVec2 n(-dy / len * 6.0f, dx / len * 6.0f);
			a = ImVec2(a.x + n.x, a.y + n.y);
			b = ImVec2(b.x + n.x, b.y + n.y);
			const ImVec2 s = EdgePoint(a, b), e = EdgePoint(b, a);
			if (leftClicked && hitState < 0)
			{
				const float vx = e.x - s.x, vy = e.y - s.y, l2 = std::max(vx * vx + vy * vy, 1.0f);
				const float u = std::clamp(((mouse.x - s.x) * vx + (mouse.y - s.y) * vy) / l2, 0.0f, 1.0f);
				if (std::hypot(mouse.x - (s.x + vx * u), mouse.y - (s.y + vy * u)) < 6.0f)
					m_selectedTransition = i;
			}
			const ImU32 col = i == m_selectedTransition ? IM_COL32(255, 190, 60, 255) : IM_COL32(200, 200, 200, 255);
			dl->AddLine(s, e, col, 2.0f);
			const ImVec2 d((e.x - s.x) / len, (e.y - s.y) / len);
			dl->AddTriangleFilled(e, ImVec2(e.x - d.x * 10 - d.y * 5, e.y - d.y * 10 + d.x * 5), ImVec2(e.x - d.x * 10 + d.y * 5, e.y - d.y * 10 - d.x * 5), col);
		}

		for (int i = 0; i < stateCount; ++i)
		{
			AnimState &s = m_graph.states[i];
			const ImVec2 min(origin.x + s.x, origin.y + s.y), max(min.x + kBox.x, min.y + kBox.y);
			const ImVec2 handle(max.x, min.y + kBox.y * 0.5f);
			const bool handleHot = (hitState == i && hitHandle) || m_linkFrom == i;
			const bool selected = i == m_selectedState;
			dl->AddRectFilled(min, ImVec2(min.x + kBox.x, min.y + kBox.y), i == m_graph.entry ? IM_COL32(120, 70, 20, 255) : IM_COL32(60, 60, 60, 255), 6.0f);
			dl->AddRect(min, ImVec2(min.x + kBox.x, min.y + kBox.y), selected ? IM_COL32(255, 190, 60, 255) : IM_COL32(110, 110, 110, 255), 6.0f, 0, selected ? 2.5f : 1.0f);
			dl->AddText(ImVec2(min.x + 8, min.y + 6), IM_COL32(235, 235, 235, 255), s.name.c_str());
			dl->AddText(ImVec2(min.x + 8, min.y + 26), IM_COL32(150, 150, 150, 255), s.clip.empty() ? "(no clip)" : s.clip.c_str());
			dl->AddCircleFilled(handle, handleHot ? 7.0f : 5.0f, handleHot ? IM_COL32(255, 190, 60, 255) : IM_COL32(200, 200, 200, 255));
		}

		if (m_linkFrom >= 0 && m_linkFrom < stateCount)
		{
			const AnimState &from = m_graph.states[m_linkFrom];
			dl->AddLine(ImVec2(origin.x + from.x + kBox.x, origin.y + from.y + kBox.y * 0.5f), mouse, IM_COL32(255, 190, 60, 255), 2.0f);
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			{
				int target = hitState;
				if (target < 0)
					target = AddState(mouse.x - origin.x - kBox.x * 0.5f, mouse.y - origin.y - kBox.y * 0.5f);
				AnimTransition t;
				t.from = m_linkFrom;
				t.to = target;
				m_graph.transitions.push_back(t);
				m_selectedState = -1;
				m_selectedTransition = static_cast<int>(m_graph.transitions.size()) - 1;
				m_dirty = true;
				m_linkFrom = -1;
			}
		}
		else
			m_linkFrom = -1;
		dl->PopClipRect();

		if (ImGui::BeginPopup("anim_menu"))
		{
			if (m_contextState >= 0 && m_contextState < static_cast<int>(m_graph.states.size()))
			{
				if (ImGui::MenuItem("Set as entry"))
				{
					m_graph.entry = m_contextState;
					m_dirty = true;
				}
				if (ImGui::MenuItem("Add transition to self"))
				{
					AnimTransition t;
					t.from = t.to = m_contextState;
					m_graph.transitions.push_back(t);
					m_dirty = true;
				}
				if (ImGui::MenuItem("Delete", "Del"))
					DeleteState(m_contextState);
			}
			else
			{
				if (ImGui::MenuItem("Add state here"))
					m_selectedState = AddState(m_contextPos.x - kBox.x * 0.5f, m_contextPos.y - kBox.y * 0.5f);
				if (ImGui::MenuItem("Reset view"))
					m_panX = m_panY = 0.0f;
			}
			ImGui::EndPopup();
		}
	}

	int AnimationEditor::AddState(float x, float y)
	{
		AnimState s;
		s.name = "State " + std::to_string(m_graph.states.size());
		s.x = x;
		s.y = y;
		m_graph.states.push_back(s);
		m_dirty = true;
		return static_cast<int>(m_graph.states.size()) - 1;
	}

	void AnimationEditor::DeleteState(int removed)
	{
		if (removed < 0 || removed >= static_cast<int>(m_graph.states.size()))
			return;
		m_graph.states.erase(m_graph.states.begin() + removed);
		std::erase_if(m_graph.transitions, [&](const AnimTransition &t) { return t.from == removed || t.to == removed; });
		for (AnimTransition &t : m_graph.transitions)
		{
			t.from -= t.from > removed;
			t.to -= t.to > removed;
		}
		if (m_graph.entry == removed)
			m_graph.entry = 0;
		else if (m_graph.entry > removed)
			--m_graph.entry;
		m_selectedState = m_selectedTransition = -1;
		m_dirty = true;
	}

	void AnimationEditor::DrawSide()
	{
		const int stateCount = static_cast<int>(m_graph.states.size());
		const auto stateName = [&](int i) { return i >= 0 && i < stateCount ? m_graph.states[i].name.c_str() : "?"; };

		if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen))
		{
			int remove = -1;
			for (int i = 0; i < static_cast<int>(m_graph.params.size()); ++i)
			{
				AnimParam &p = m_graph.params[i];
				ImGui::PushID(i);
				ImGui::SetNextItemWidth(110.0f);
				m_dirty |= InputText("##name", p.name);
				ImGui::SameLine();
				m_dirty |= ImGui::Checkbox("bool", &p.isBool);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(60.0f);
				m_dirty |= ImGui::DragFloat("##def", &p.defaultValue, 0.05f);
				ImGui::SameLine();
				if (ImGui::SmallButton("X"))
					remove = i;
				ImGui::PopID();
			}
			if (remove >= 0)
			{
				m_graph.params.erase(m_graph.params.begin() + remove);
				m_dirty = true;
			}
			if (ImGui::Button("Add parameter"))
			{
				m_graph.params.push_back({});
				m_dirty = true;
			}
		}

		if (ImGui::CollapsingHeader("States", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button("Add state"))
			{
				AnimState s;
				s.name = "State " + std::to_string(stateCount);
				s.x = 40.0f + 30.0f * static_cast<float>(stateCount);
				s.y = 40.0f + 30.0f * static_cast<float>(stateCount);
				m_graph.states.push_back(s);
				m_selectedState = stateCount;
				m_dirty = true;
			}
			if (m_selectedState >= 0 && m_selectedState < stateCount)
			{
				AnimState &s = m_graph.states[m_selectedState];
				m_dirty |= InputText("Name", s.name);
				m_dirty |= InputText("Clip", s.clip);
				m_dirty |= ImGui::DragFloat("Speed", &s.speed, 0.01f, 0.0f, 10.0f);
				m_dirty |= ImGui::Checkbox("Loop", &s.loop);
				if (ImGui::Button("Set as entry"))
				{
					m_graph.entry = m_selectedState;
					m_dirty = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("Delete state"))
					DeleteState(m_selectedState);
			}
		}

		if (ImGui::CollapsingHeader("Transitions", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button("Add transition") && stateCount > 0)
			{
				AnimTransition t;
				t.from = std::clamp(m_selectedState, 0, stateCount - 1);
				t.to = (t.from + 1) % stateCount;
				m_graph.transitions.push_back(t);
				m_selectedTransition = static_cast<int>(m_graph.transitions.size()) - 1;
				m_dirty = true;
			}
			int remove = -1;
			for (int i = 0; i < static_cast<int>(m_graph.transitions.size()); ++i)
			{
				AnimTransition &t = m_graph.transitions[i];
				ImGui::PushID(i);
				const std::string label = std::string(stateName(t.from)) + " -> " + stateName(t.to);
				if (ImGui::Selectable(label.c_str(), i == m_selectedTransition))
					m_selectedTransition = i;
				ImGui::PopID();
			}
			if (m_selectedTransition >= 0 && m_selectedTransition < static_cast<int>(m_graph.transitions.size()))
			{
				AnimTransition &t = m_graph.transitions[m_selectedTransition];
				ImGui::Separator();
				const auto stateCombo = [&](const char *label, int &index)
				{
					if (ImGui::BeginCombo(label, stateName(index)))
					{
						for (int i = 0; i < stateCount; ++i)
							if (ImGui::Selectable(m_graph.states[i].name.c_str(), i == index))
							{
								index = i;
								m_dirty = true;
							}
						ImGui::EndCombo();
					}
				};
				stateCombo("From", t.from);
				stateCombo("To", t.to);
				if (ImGui::BeginCombo("Parameter", t.param.empty() ? "(on finish)" : t.param.c_str()))
				{
					if (ImGui::Selectable("(on finish)", t.param.empty()))
					{
						t.param.clear();
						m_dirty = true;
					}
					for (const AnimParam &p : m_graph.params)
						if (ImGui::Selectable(p.name.c_str(), p.name == t.param))
						{
							t.param = p.name;
							m_dirty = true;
						}
					ImGui::EndCombo();
				}
				if (!t.param.empty())
				{
					m_dirty |= ImGui::Combo("Condition", &t.op, "greater than\0less than\0equals\0is true\0is false\0");
					if (t.op <= 2)
						m_dirty |= ImGui::DragFloat("Value", &t.value, 0.05f);
				}
				m_dirty |= ImGui::DragFloat("Blend time", &t.blendTime, 0.01f, 0.0f, 5.0f);
				if (ImGui::Button("Delete transition"))
					remove = m_selectedTransition;
			}
			if (remove >= 0)
			{
				m_graph.transitions.erase(m_graph.transitions.begin() + remove);
				m_selectedTransition = -1;
				m_dirty = true;
			}
		}
	}

	void AnimationEditor::Draw()
	{
		if (!open)
			return;
		if (!ImGui::Begin("Animation Graph", &open))
		{
			ImGui::End();
			return;
		}
		ImGui::SetNextItemWidth(200.0f);
		if (ImGui::BeginCombo("##graph", m_name.empty() ? "(no graph)" : (m_name + (m_dirty ? "*" : "")).c_str()))
		{
			for (const std::string &g : m_graphs)
				if (ImGui::Selectable(g.c_str(), g == m_name))
					Open(g);
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
			m_graph = AnimGraph();
			m_selectedState = m_selectedTransition = -1;
			Save();
		}
		ImGui::SameLine();
		if (ImGui::Button("Save") && !m_name.empty())
			Save();
		ImGui::SameLine();
		ImGui::TextDisabled("%s  (drag boxes, middle-drag pans)", m_status.c_str());
		ImGui::Separator();

		ImGui::BeginChild("##anim_canvas", ImVec2(ImGui::GetContentRegionAvail().x * 0.62f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		DrawCanvas();
		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::BeginChild("##anim_side");
		DrawSide();
		ImGui::EndChild();
		ImGui::End();
	}

} // namespace editor
