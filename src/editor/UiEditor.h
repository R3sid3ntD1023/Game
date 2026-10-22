#pragma once
#include "ui/UiLayout.h"
#include <string>
#include <vector>

namespace editor
{

	// Dockable "UI Editor" window: edits screen layouts stored in assets/ui/*.ui.json on a 1280x720 canvas.
	class UiEditor
	{
	public:
		UiEditor();
		void Draw();
		bool open = true;
		const std::string &Status() const { return m_status; }

	private:
		void Refresh();
		void Open(const std::string &name);
		void Save();
		void DrawList();
		void DrawProperties(ui::UiElement &e);
		void DrawCanvas();

		ui::UiLayout m_layout;
		std::string m_name;
		std::vector<std::string> m_layouts;
		char m_newName[64] = {};
		int m_selected = -1;
		bool m_dirty = false;
		bool m_snap = true;
		float m_zoom = 1.0f;
		std::string m_status;
	};

} // namespace editor
