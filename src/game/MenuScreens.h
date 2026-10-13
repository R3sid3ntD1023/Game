#pragma once
#include "gfx/PostProcess.h"
#include "ui/Widget.h"
#include <functional>
#include <string>

namespace game {

struct MenuCallbacks {
    std::function<void(const std::string& world)> play;
    std::function<void()> resume;
    std::function<void()> toMenu;
    std::function<void()> quit;
};

// Main menu and pause menu, built once as retained widgets and toggled with ShowMain/ShowPause.
class MenuScreens {
public:
    MenuScreens(ui::Widget& parent, MenuCallbacks callbacks, gfx::PostProcess& post);

    void ShowMain(bool visible) { m_main->visible = visible; }
    void ShowPause(bool visible) { m_pause->visible = visible; }
    void SetMessage(const std::string& message) { m_message->text = message; }
    bool Regenerate() const { return m_regenerate->checked; }

private:
    ui::Widget* m_main = nullptr;
    ui::Widget* m_pause = nullptr;
    ui::Label* m_message = nullptr;
    ui::Checkbox* m_regenerate = nullptr;
};

} // namespace game
