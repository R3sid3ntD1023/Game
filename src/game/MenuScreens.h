#pragma once
#include "gfx/PostProcess.h"
#include "gfx/Texture.h"
#include "ui/Widget.h"
#include <functional>
#include <string>

namespace game
{

	struct MenuCallbacks
	{
		std::function<void(const std::string &world)> play;
		std::function<void()> resume;
		std::function<void()> toMenu;
		std::function<void()> quit;
		std::function<void()> toSettings;
	};

	// Main menu and pause menu, built once as retained widgets and toggled with ShowMain/ShowPause.
	class MenuScreens
	{
	public:
		MenuScreens(ui::Widget &parent, MenuCallbacks callbacks, gfx::PostProcess &post);

		void ShowMain(bool visible) { m_main->visible = visible; }
		void ShowPause(bool visible) { m_pause->visible = visible; }
		void ShowSettings(bool visible) { m_settings->visible = visible; }
		void SetMessage(const std::string &message) { m_message->text = message; }
		bool Regenerate() const { return m_regenerate->checked; }
		float MasterVolume() const { return m_masterVolume; }

	private:
		void MakeMainMenu(ui::Widget &parent, MenuCallbacks cb);
		void MakeSettingsMenu(ui::Widget &parent, MenuCallbacks cb, gfx::PostProcess &post);
		void MakePauseMenu(ui::Widget &parent, MenuCallbacks cb);

	private:
		ui::Widget *m_main = nullptr;
		ui::Widget *m_pause = nullptr;
		ui::Widget *m_settings = nullptr;
		ui::Label *m_message = nullptr;
		ui::Checkbox *m_regenerate = nullptr;
		ui::Slider *m_volume = nullptr;
		float m_masterVolume = 0.2f;

		std::shared_ptr<gfx::Texture> m_texture;
	};

} // namespace game
