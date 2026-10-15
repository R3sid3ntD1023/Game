#include "game/MenuScreens.h"

namespace game
{

    namespace
    {

        ui::Button &MakeButton(ui::Widget &parent, const char *text, std::function<void()> onClick)
        {
            auto &b = parent.Add<ui::Button>();
            b.size = {360.0f, 50.0f};
            b.text = text;
            b.onClick = std::move(onClick);
            return b;
        }

        ui::Image &MakeImage(ui::Widget &parent, GLuint texture)
        {
            auto &i = parent.Add<ui::Image>();
            i.texture = texture;
            return i;
        }

        ui::Label &MakeLabel(ui::Widget &parent, ui::Anchor anchor, glm::vec2 offset, glm::vec2 size, float textSize, ui::Color color,
                             const char *text)
        {
            auto &l = parent.Add<ui::Label>();
            l.anchor = anchor;
            l.offset = offset;
            l.size = size;
            l.textSize = textSize;
            l.align = ui::Align::Center;
            l.color = color;
            l.text = text;
            return l;
        }

        ui::Slider &MakeSlider(ui::Widget &parent, const char *text, const ui::WidgetBinding<float> &binding)
        {
            auto &s = parent.Add<ui::Slider>();
            s.size = {360.0f, 32.0f};
            s.text = text;
            s.binding = binding;
            return s;
        }

    } // namespace

    MenuScreens::MenuScreens(ui::Widget &parent, MenuCallbacks cb, gfx::PostProcess &post)
    {
        m_texture = gfx::Texture::FromFile(ASSETS_DIR "textures/gear.png", false);

        using ui::Anchor;

        // Main menu
        auto &main = parent.Add<ui::Panel>();
        main.Stretch();
        main.gradient = true;
        main.color = {0.05f, 0.09f, 0.14f, 1.0f};
        main.colorBottom = {0.02f, 0.03f, 0.05f, 1.0f};
        m_main = &main;

        MakeLabel(main, Anchor::Top, {0.0f, 80.0f}, {1100.0f, 100.0f}, 80.0f, {1.0f, 0.86f, 0.47f, 1.0f}, "ESCAPE THE ISLAND");
        MakeLabel(main, Anchor::Top, {0.0f, 180.0f}, {1100.0f, 34.0f}, 24.0f, {0.7f, 0.75f, 0.8f, 1.0f},
                  "Find a weapon. Outrun the raptors. Reach the boat.");

        auto &to_settings = MakeButton(main, "", [settings = cb.toSettings]
                                       { if (settings) settings(); });
        to_settings.size = {50.0f, 50.0f};
        to_settings.offset = {20.0f, 20.0f};
        to_settings.anchor = Anchor::BottomRight;

        auto &image = to_settings.Add<ui::Image>();
        image.texture = m_texture->Id();
        image.anchor = Anchor::Center;
        image.size = {45.0f, 45.0f};
        image.color = {1.0f, 1.0f, 1.0f, 1.0f};

        auto &stack = main.Add<ui::StackPanel>();
        stack.anchor = Anchor::Center;
        stack.offset = {0.0f, 50.0f};
        stack.size = {360.0f, 290.0f};
        stack.spacing = 14.0f;

        MakeButton(stack, "Play - Island", [play = cb.play]
                   { if (play) play("island"); });
        MakeButton(stack, "Play - Test World", [play = cb.play]
                   { if (play) play("test"); });

        auto &regen = stack.Add<ui::Checkbox>();
        regen.size = {360.0f, 32.0f};
        regen.text = "Regenerate world file";
        m_regenerate = &regen;

        MakeButton(stack, "Quit", [quit = cb.quit]
                   { if (quit) quit(); });

        m_message = &MakeLabel(main, Anchor::Bottom, {0.0f, 70.0f}, {1100.0f, 26.0f}, 20.0f, {1.0f, 0.4f, 0.35f, 1.0f}, "");
        MakeLabel(main, Anchor::Bottom, {0.0f, 30.0f}, {1100.0f, 26.0f}, 18.0f, {0.6f, 0.65f, 0.7f, 1.0f},
                  "WASD move | Shift sprint | Space jump | LMB fire | R reload | E interact | F flashlight | Esc pause");

        MakeSettingsMenu(parent, cb, post);

        // Pause menu
        auto &pause = parent.Add<ui::Panel>();
        pause.Stretch();
        pause.color = {0.0f, 0.0f, 0.0f, 0.6f};
        pause.visible = false;
        m_pause = &pause;

        MakeLabel(pause, Anchor::Top, {0.0f, 150.0f}, {800.0f, 80.0f}, 64.0f, {1.0f, 1.0f, 1.0f, 1.0f}, "PAUSED");
        auto &pauseStack = pause.Add<ui::StackPanel>();
        pauseStack.anchor = Anchor::Center;
        pauseStack.offset = {0.0f, 30.0f};
        pauseStack.size = {360.0f, 180.0f};
        pauseStack.spacing = 14.0f;
        MakeButton(pauseStack, "Resume", [f = cb.resume]
                   { if (f) f(); });
        MakeButton(pauseStack, "Main Menu", [f = cb.toMenu]
                   { if (f) f(); });
        MakeButton(pauseStack, "Quit", [f = cb.quit]
                   { if (f) f(); });
    }

    void MenuScreens::MakeSettingsMenu(ui::Widget &parent, MenuCallbacks cb, gfx::PostProcess &post)
    {
        using ui::Anchor;

        // Settings button will show the settings menu when clicked
        auto &settings = parent.Add<ui::Panel>();
        settings.Stretch();
        settings.visible = false;
        settings.anchor = Anchor::Top;
        settings.color = {0.05f, 0.09f, 0.14f, 1.0f};
        m_settings = &settings;

        auto &panel = settings.Add<ui::StackPanel>();
        panel.anchor = Anchor::Top;
        panel.offset = {0.0f, 50.0f};
        panel.spacing = 14.0f;

        auto &title = panel.Add<ui::Label>();
        title.text = "Settings Menu";

        auto &bloom = panel.Add<ui::Checkbox>();
        bloom.size = {360.0f, 32.0f};
        bloom.text = "Bloom";
        bloom.checked = post.bloomEnabled;
        bloom.onChange = [&post](bool on)
        { post.bloomEnabled = on; };

        m_volume = &MakeSlider(panel, "Volume", ui::WidgetBinding<float>{&m_masterVolume});
        auto &back = MakeButton(settings, "Back", [back = cb.toMenu]
                                { if (back) back(); });
        back.anchor = Anchor::BottomRight;
        back.size = {50.0f, 50.0f};
        back.offset = {20.0f, 20.0f};
    }

} // namespace game
