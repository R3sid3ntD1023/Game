#include "editor/UiEditor.h"
#include <algorithm>
#include <filesystem>
#include <imgui.h>

namespace fs = std::filesystem;

namespace editor
{

	namespace
	{
		constexpr float kRefWidth = 1280.0f;
		constexpr const char *kTypeNames = "Panel\0Label\0Progress bar\0Button\0";
		constexpr const char *kAnchorNames = "Top left\0Top\0Top right\0Left\0Center\0Right\0Bottom left\0Bottom\0Bottom right\0";
		constexpr const char *kAlignNames = "Left\0Center\0Right\0";

		fs::path Dir() { return fs::path(ASSETS_DIR) / "ui"; }
		std::string PathFor(const std::string &name) { return (Dir() / (name + ui::kUiLayoutExtension)).string(); }

		ImU32 ToU32(const glm::vec4 &c) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w)); }

		bool ColorEdit(const char *label, glm::vec4 &c) { return ImGui::ColorEdit4(label, &c.x, ImGuiColorEditFlags_AlphaBar); }
	} // namespace

	UiEditor::UiEditor()
	{
		Refresh();
	}

	void UiEditor::Refresh()
	{
		m_layouts.clear();
		std::error_code ec;
		const std::string ext = ui::kUiLayoutExtension;
		for (const auto &entry : fs::directory_iterator(Dir(), ec))
		{
			const std::string file = entry.path().filename().string();
			if (file.size() > ext.size() && file.compare(file.size() - ext.size(), ext.size(), ext) == 0)
				m_layouts.push_back(file.substr(0, file.size() - ext.size()));
		}
		std::sort(m_layouts.begin(), m_layouts.end());
	}

	void UiEditor::Open(const std::string &name)
	{
		try
		{
			m_layout = ui::LoadUiLayout(PathFor(name));
			m_name = name;
			m_selected = -1;
			m_dirty = false;
			m_status = "Opened " + name;
		}
		catch (const std::exception &e)
		{
			m_status = std::string("Open failed: ") + e.what();
		}
	}

	void UiEditor::Save()
	{
		if (m_name.empty())
			return;
		try
		{
			ui::SaveUiLayout(PathFor(m_name), m_layout);
			m_dirty = false;
			m_status = "Saved " + m_name;
			Refresh();
		}
		catch (const std::exception &e)
		{
			m_status = std::string("Save failed: ") + e.what();
		}
	}

	void UiEditor::DrawList()
	{
		if (ImGui::BeginCombo("##layout", m_name.empty() ? "(no layout)" : (m_name + (m_dirty ? "*" : "")).c_str()))
		{
			for (const std::string &n : m_layouts)
				if (ImGui::Selectable(n.c_str(), n == m_name))
					Open(n);
			ImGui::EndCombo();
		}
		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputTextWithHint("##newname", "new layout", m_newName, sizeof(m_newName));
		ImGui::SameLine();
		if (ImGui::Button("Create") && m_newName[0])
		{
			m_name = m_newName;
			m_layout = {};
			m_selected = -1;
			m_newName[0] = '\0';
			Save();
		}
		ImGui::BeginDisabled(m_name.empty());
		if (ImGui::Button("Save"))
			Save();
		ImGui::EndDisabled();

		ImGui::SeparatorText("Elements");
		if (ImGui::Button("Add"))
			ImGui::OpenPopup("addelement");
		if (ImGui::BeginPopup("addelement"))
		{
			static const char *names[] = {"Panel", "Label", "Progress bar", "Button"};
			for (int t = 0; t < 4; ++t)
				if (ImGui::MenuItem(names[t]))
				{
					ui::UiElement e;
					e.type = static_cast<ui::ElementType>(t);
					e.name = names[t];
					e.text = t == 1 || t == 3 ? names[t] : "";
					if (t == 1)
						e.color = {1.0f, 1.0f, 1.0f, 1.0f};
					if (t == 2)
						e.color = {0.2f, 0.8f, 0.3f, 0.9f};
					if (t == 3)
					{
						e.color = {0.12f, 0.16f, 0.22f, 0.92f};
						e.secondary = {1.0f, 1.0f, 1.0f, 0.9f};
					}
					m_layout.elements.push_back(e);
					m_selected = static_cast<int>(m_layout.elements.size()) - 1;
					m_dirty = true;
				}
			ImGui::EndPopup();
		}
		const bool has = m_selected >= 0 && m_selected < static_cast<int>(m_layout.elements.size());
		ImGui::SameLine();
		if (ImGui::Button("Duplicate") && has)
		{
			m_layout.elements.push_back(m_layout.elements[m_selected]);
			m_selected = static_cast<int>(m_layout.elements.size()) - 1;
			m_dirty = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete") && has)
		{
			m_layout.elements.erase(m_layout.elements.begin() + m_selected);
			m_selected = -1;
			m_dirty = true;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!has || m_selected == 0);
		if (ImGui::ArrowButton("##up", ImGuiDir_Up))
		{
			std::swap(m_layout.elements[m_selected], m_layout.elements[m_selected - 1]);
			--m_selected;
			m_dirty = true;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!has || m_selected + 1 >= static_cast<int>(m_layout.elements.size()));
		if (ImGui::ArrowButton("##down", ImGuiDir_Down))
		{
			std::swap(m_layout.elements[m_selected], m_layout.elements[m_selected + 1]);
			++m_selected;
			m_dirty = true;
		}
		ImGui::EndDisabled();

		ImGui::BeginChild("##elements", ImVec2(0, 150.0f), ImGuiChildFlags_Borders);
		for (int i = 0; i < static_cast<int>(m_layout.elements.size()); ++i)
		{
			ImGui::PushID(i);
			if (ImGui::Selectable(m_layout.elements[i].name.c_str(), i == m_selected))
				m_selected = i;
			ImGui::PopID();
		}
		ImGui::EndChild();
	}

	void UiEditor::DrawProperties(ui::UiElement &e)
	{
		bool changed = false;
		char name[64];
		std::snprintf(name, sizeof(name), "%s", e.name.c_str());
		if (ImGui::InputText("Name", name, sizeof(name)))
		{
			e.name = name;
			changed = true;
		}
		int type = static_cast<int>(e.type);
		if (ImGui::Combo("Type", &type, kTypeNames))
		{
			e.type = static_cast<ui::ElementType>(type);
			changed = true;
		}
		changed |= ImGui::Checkbox("Visible", &e.visible);
		int anchor = static_cast<int>(e.anchor);
		if (ImGui::Combo("Anchor", &anchor, kAnchorNames))
		{
			e.anchor = static_cast<ui::Anchor>(anchor);
			changed = true;
		}
		changed |= ImGui::DragFloat2("Offset", &e.offset.x, 1.0f);
		changed |= ImGui::DragFloat2("Size", &e.size.x, 1.0f, 1.0f, 4000.0f);
		changed |= ImGui::Checkbox("Stretch X", &e.stretchX);
		ImGui::SameLine();
		changed |= ImGui::Checkbox("Stretch Y", &e.stretchY);

		switch (e.type)
		{
		case ui::ElementType::Panel:
			changed |= ColorEdit("Color", e.color);
			break;
		case ui::ElementType::Label:
		{
			char text[256];
			std::snprintf(text, sizeof(text), "%s", e.text.c_str());
			if (ImGui::InputText("Text", text, sizeof(text)))
			{
				e.text = text;
				changed = true;
			}
			changed |= ImGui::DragFloat("Text size", &e.textSize, 0.5f, 6.0f, 200.0f);
			int align = static_cast<int>(e.align);
			if (ImGui::Combo("Align", &align, kAlignNames))
			{
				e.align = static_cast<ui::Align>(align);
				changed = true;
			}
			changed |= ColorEdit("Color", e.color);
			break;
		}
		case ui::ElementType::ProgressBar:
			changed |= ImGui::SliderFloat("Value", &e.value, 0.0f, 1.0f);
			changed |= ColorEdit("Fill", e.color);
			changed |= ColorEdit("Background", e.secondary);
			break;
		case ui::ElementType::Button:
		{
			char text[256];
			std::snprintf(text, sizeof(text), "%s", e.text.c_str());
			if (ImGui::InputText("Text", text, sizeof(text)))
			{
				e.text = text;
				changed = true;
			}
			changed |= ImGui::DragFloat("Text size", &e.textSize, 0.5f, 6.0f, 200.0f);
			changed |= ColorEdit("Background", e.color);
			changed |= ColorEdit("Text color", e.secondary);
			break;
		}
		}
		if (changed)
			m_dirty = true;
	}

	void UiEditor::DrawCanvas()
	{
		ImGui::Checkbox("Snap 4px", &m_snap);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderFloat("Zoom", &m_zoom, 0.25f, 2.0f, "%.2f");

		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const float fit = std::min(avail.x / kRefWidth, avail.y / ui::kUiReferenceHeight);
		const float scale = std::max(0.05f, fit * m_zoom);
		ImGui::BeginChild("##canvas", avail, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const ImVec2 canvasSize(kRefWidth * scale, ui::kUiReferenceHeight * scale);
		ImGui::InvisibleButton("##surface", canvasSize);
		const bool surfaceClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		ImDrawList *dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(origin, ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y), IM_COL32(28, 30, 28, 255));

		const ui::Rect screen{{origin.x, origin.y}, {canvasSize.x, canvasSize.y}};
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		int hit = -1;
		for (int i = 0; i < static_cast<int>(m_layout.elements.size()); ++i)
		{
			const ui::UiElement &e = m_layout.elements[i];
			if (!e.visible)
				continue;
			const ui::Rect r = ui::ElementRect(e, screen, scale);
			const ImVec2 a(r.pos.x, r.pos.y), b(r.pos.x + r.size.x, r.pos.y + r.size.y);
			switch (e.type)
			{
			case ui::ElementType::Panel:
				dl->AddRectFilled(a, b, ToU32(e.color));
				break;
			case ui::ElementType::Label:
			{
				const float s = e.textSize * scale;
				const ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(s, FLT_MAX, 0.0f, e.text.c_str());
				float x = a.x;
				if (e.align == ui::Align::Center)
					x += (r.size.x - ts.x) * 0.5f;
				else if (e.align == ui::Align::Right)
					x += r.size.x - ts.x;
				dl->AddText(ImGui::GetFont(), s, ImVec2(x, a.y + (r.size.y - s) * 0.5f), ToU32(e.color), e.text.c_str());
				break;
			}
			case ui::ElementType::ProgressBar:
				dl->AddRectFilled(a, b, ToU32(e.secondary));
				dl->AddRectFilled(a, ImVec2(a.x + r.size.x * std::clamp(e.value, 0.0f, 1.0f), b.y), ToU32(e.color));
				dl->AddRect(a, b, IM_COL32(255, 255, 255, 115));
				break;
			case ui::ElementType::Button:
			{
				dl->AddRectFilled(a, b, ToU32(e.color));
				dl->AddRect(a, b, IM_COL32(255, 255, 255, 80));
				const float s = e.textSize * scale;
				const ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(s, FLT_MAX, 0.0f, e.text.c_str());
				dl->AddText(ImGui::GetFont(), s, ImVec2(a.x + (r.size.x - ts.x) * 0.5f, a.y + (r.size.y - s) * 0.5f), ToU32(e.secondary), e.text.c_str());
				break;
			}
			}
			if (mouse.x >= a.x && mouse.x < b.x && mouse.y >= a.y && mouse.y < b.y)
				hit = i;
			if (i == m_selected)
				dl->AddRect(a, b, IM_COL32(220, 138, 41, 255), 0.0f, 0, 2.0f);
		}
		dl->AddRect(origin, ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y), IM_COL32(120, 120, 120, 255));

		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			m_selected = hit;
		(void)surfaceClicked;

		if (m_selected >= 0 && m_selected < static_cast<int>(m_layout.elements.size()) && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsWindowFocused())
		{
			ui::UiElement &e = m_layout.elements[m_selected];
			const ImVec2 delta = ImGui::GetIO().MouseDelta;
			if (delta.x != 0.0f || delta.y != 0.0f)
			{
				const int index = static_cast<int>(e.anchor);
				const int h = index % 3, v = index / 3;
				e.offset.x += (h == 2 ? -delta.x : delta.x) / scale;
				e.offset.y += (v == 2 ? -delta.y : delta.y) / scale;
				if (m_snap)
				{
					e.offset.x = std::round(e.offset.x / 4.0f) * 4.0f;
					e.offset.y = std::round(e.offset.y / 4.0f) * 4.0f;
				}
				m_dirty = true;
			}
		}
		ImGui::EndChild();
	}

	void UiEditor::Draw()
	{
		if (!open)
			return;
		if (!ImGui::Begin("UI Editor", &open))
		{
			ImGui::End();
			return;
		}
		ImGui::BeginChild("##left", ImVec2(300.0f, 0), ImGuiChildFlags_ResizeX);
		DrawList();
		if (m_selected >= 0 && m_selected < static_cast<int>(m_layout.elements.size()))
		{
			ImGui::SeparatorText("Properties");
			DrawProperties(m_layout.elements[m_selected]);
		}
		if (!m_status.empty())
			ImGui::TextDisabled("%s", m_status.c_str());
		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::BeginGroup();
		DrawCanvas();
		ImGui::EndGroup();
		ImGui::End();
	}

} // namespace editor
