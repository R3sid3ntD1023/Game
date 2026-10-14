#include "game/Hud.h"
#include "eng/BuiltinComponents.h"
#include "game/Weapon.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game
{

    namespace
    {

        ui::Label &MakeLabel(ui::Widget &parent, ui::Anchor anchor, glm::vec2 offset, glm::vec2 size, float textSize,
                             ui::Align align, ui::Color color)
        {
            auto &l = parent.Add<ui::Label>();
            l.anchor = anchor;
            l.offset = offset;
            l.size = size;
            l.textSize = textSize;
            l.align = align;
            l.color = color;
            return l;
        }

        const ui::Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};
        const ui::Color kGray{0.75f, 0.75f, 0.75f, 1.0f};
        const ui::Color kGold{1.0f, 0.86f, 0.47f, 1.0f};

    } // namespace

    Hud::Hud(ui::Widget &parent)
    {
        using ui::Align;
        using ui::Anchor;

        auto &root = parent.Add<ui::Widget>();
        root.Stretch();
        m_root = &root;

        // Low-health vignette (alpha animated in Update)
        m_vignetteTop = &root.Add<ui::Panel>();
        m_vignetteTop->anchor = Anchor::Top;
        m_vignetteTop->stretchX = true;
        m_vignetteTop->size = {0.0f, 130.0f};
        m_vignetteTop->gradient = true;
        m_vignetteTop->visible = false;
        m_vignetteBottom = &root.Add<ui::Panel>();
        m_vignetteBottom->anchor = Anchor::Bottom;
        m_vignetteBottom->stretchX = true;
        m_vignetteBottom->size = {0.0f, 130.0f};
        m_vignetteBottom->gradient = true;
        m_vignetteBottom->visible = false;

        // Health
        MakeLabel(root, Anchor::BottomLeft, {24.0f, 52.0f}, {120.0f, 20.0f}, 16.0f, Align::Left, kGray).text = "HEALTH";
        m_healthBar = &root.Add<ui::ProgressBar>();
        m_healthBar->anchor = Anchor::BottomLeft;
        m_healthBar->offset = {24.0f, 24.0f};
        m_healthBar->size = {280.0f, 24.0f};
        m_healthText = &MakeLabel(*m_healthBar, Anchor::TopLeft, {0.0f, 0.0f}, {0.0f, 0.0f}, 18.0f, Align::Center, kWhite);
        m_healthText->Stretch();

        // Weapon panel
        m_weaponPanel = &root.Add<ui::Panel>();
        m_weaponPanel->anchor = Anchor::BottomRight;
        m_weaponPanel->offset = {24.0f, 24.0f};
        m_weaponPanel->size = {260.0f, 92.0f};
        m_weaponPanel->color = {0.0f, 0.0f, 0.0f, 0.47f};
        m_weaponPanel->border = {1.0f, 1.0f, 1.0f, 0.35f};
        m_weaponPanel->borderWidth = 1.5f;
        ui::Widget &wp = *m_weaponPanel;

        m_weaponName = &MakeLabel(wp, Anchor::TopLeft, {12.0f, 6.0f}, {150.0f, 26.0f}, 22.0f, Align::Left, kGold);
        m_weaponMode = &MakeLabel(wp, Anchor::TopLeft, {12.0f, 32.0f}, {80.0f, 18.0f}, 15.0f, Align::Left, kGray);
        m_ammo = &MakeLabel(wp, Anchor::TopRight, {84.0f, 6.0f}, {90.0f, 46.0f}, 42.0f, Align::Right, kWhite);
        m_reserve = &MakeLabel(wp, Anchor::TopRight, {12.0f, 20.0f}, {66.0f, 28.0f}, 22.0f, Align::Left, kGray);
        m_weaponStatus = &MakeLabel(wp, Anchor::BottomLeft, {12.0f, 24.0f}, {236.0f, 18.0f}, 15.0f, Align::Left, kGold);
        m_reloadBar = &wp.Add<ui::ProgressBar>();
        m_reloadBar->anchor = Anchor::BottomLeft;
        m_reloadBar->offset = {12.0f, 12.0f};
        m_reloadBar->size = {236.0f, 8.0f};
        m_reloadBar->fill = {1.0f, 0.78f, 0.24f, 0.9f};
        m_unarmed = &MakeLabel(wp, Anchor::Center, {0.0f, -10.0f}, {240.0f, 32.0f}, 26.0f, Align::Center, {0.67f, 0.67f, 0.67f, 1.0f});
        m_unarmed->text = "UNARMED";
        m_unarmedHint = &MakeLabel(wp, Anchor::Center, {0.0f, 22.0f}, {240.0f, 20.0f}, 15.0f, Align::Center, {0.55f, 0.55f, 0.55f, 1.0f});
        m_unarmedHint->text = "find a weapon";

        // Kills
        m_kills = &MakeLabel(root, Anchor::TopRight, {24.0f, 24.0f}, {200.0f, 28.0f}, 22.0f, Align::Right, kWhite);

        // Crosshair: dot plus four ticks
        const struct
        {
            glm::vec2 offset, size;
        } ticks[] = {
            {{0.0f, 0.0f}, {3.0f, 3.0f}},
            {{-9.5f, 0.0f}, {9.0f, 2.0f}},
            {{9.5f, 0.0f}, {9.0f, 2.0f}},
            {{0.0f, -9.5f}, {2.0f, 9.0f}},
            {{0.0f, 9.5f}, {2.0f, 9.0f}},
        };
        for (const auto &t : ticks)
        {
            auto &p = root.Add<ui::Panel>();
            p.anchor = Anchor::Center;
            p.offset = t.offset;
            p.size = t.size;
            p.color = {1.0f, 1.0f, 1.0f, 0.8f};
            m_crosshair.push_back(&p);
        }

        // Interaction prompt
        m_prompt = &MakeLabel(root, Anchor::Center, {0.0f, 110.0f}, {800.0f, 32.0f}, 24.0f, Align::Center, {1.0f, 0.94f, 0.63f, 1.0f});

        // End of game
        m_endOverlay = &root.Add<ui::Panel>();
        m_endOverlay->Stretch();
        m_endOverlay->color = {0.0f, 0.0f, 0.0f, 0.55f};
        m_endOverlay->visible = false;
        m_endTitle = &MakeLabel(*m_endOverlay, Anchor::Center, {0.0f, -70.0f}, {1000.0f, 90.0f}, 72.0f, Align::Center, kWhite);
        m_endSub = &MakeLabel(*m_endOverlay, Anchor::Center, {0.0f, 20.0f}, {600.0f, 34.0f}, 26.0f, Align::Center, kWhite);
        MakeLabel(*m_endOverlay, Anchor::Center, {0.0f, 62.0f}, {600.0f, 26.0f}, 20.0f, Align::Center, {0.8f, 0.8f, 0.8f, 1.0f}).text =
            "Press Esc for the menu";
    }

    void Hud::Update(eng::GameObject &player, const GameState &state, float time)
    {
        const bool playing = state.status == Status::Playing;
        char buf[64];

        // Health
        if (const auto *health = player.GetComponent<eng::Health>())
        {
            const float frac = health->max > 0.0f ? std::clamp(health->current / health->max, 0.0f, 1.0f) : 0.0f;
            m_healthBar->value = frac;
            m_healthBar->fill = glm::mix(ui::Color(0.86f, 0.16f, 0.16f, 0.9f), ui::Color(0.2f, 0.78f, 0.25f, 0.9f), frac);
            std::snprintf(buf, sizeof(buf), "%.0f / %.0f", health->current, health->max);
            m_healthText->text = buf;

            const bool low = playing && frac > 0.0f && frac < 0.3f;
            m_vignetteTop->visible = m_vignetteBottom->visible = low;
            if (low)
            {
                const float a = 0.25f + 0.2f * (0.5f + 0.5f * std::sin(time * 6.0f));
                m_vignetteTop->color = {0.8f, 0.0f, 0.0f, a};
                m_vignetteTop->colorBottom = {0.8f, 0.0f, 0.0f, 0.0f};
                m_vignetteBottom->color = {0.8f, 0.0f, 0.0f, 0.0f};
                m_vignetteBottom->colorBottom = {0.8f, 0.0f, 0.0f, a};
            }
        }

        // Weapon
        const auto *weapon = player.GetComponent<WeaponComponent>();
        const bool armed = weapon && weapon->Equipped();
        for (ui::Label *l : {m_weaponName, m_weaponMode, m_ammo, m_reserve, m_weaponStatus})
            l->visible = armed;
        m_reloadBar->visible = armed && weapon->Reloading();
        m_unarmed->visible = m_unarmedHint->visible = !armed;
        if (armed)
        {
            m_weaponName->text = weapon->Stats().name;
            m_weaponMode->text = weapon->Stats().automatic ? "AUTO" : "SEMI";
            std::snprintf(buf, sizeof(buf), "%d", weapon->Ammo());
            m_ammo->text = buf;
            m_ammo->color = weapon->Ammo() == 0 ? ui::Color(1.0f, 0.31f, 0.24f, 1.0f) : kWhite;
            std::snprintf(buf, sizeof(buf), "/ %d", weapon->Reserve());
            m_reserve->text = buf;

            if (weapon->Reloading())
            {
                m_weaponStatus->text = "RELOADING";
                m_reloadBar->value = weapon->ReloadProgress();
            }
            else if (weapon->Ammo() == 0)
            {
                m_weaponStatus->text = weapon->Reserve() == 0 ? "OUT OF AMMO" : "PRESS R TO RELOAD";
            }
            else
            {
                m_weaponStatus->text.clear();
            }
        }

        std::snprintf(buf, sizeof(buf), "KILLS  %d", state.kills);
        m_kills->text = buf;

        m_prompt->text = state.prompt;
        m_prompt->visible = playing && !state.prompt.empty();
        for (ui::Panel *p : m_crosshair)
            p->visible = playing;

        m_endOverlay->visible = !playing;
        if (!playing)
        {
            const bool won = state.status == Status::Won;
            m_endTitle->text = won ? "YOU ESCAPED" : "YOU WERE EATEN";
            m_endTitle->color = won ? ui::Color(0.47f, 1.0f, 0.63f, 1.0f) : ui::Color(1.0f, 0.35f, 0.27f, 1.0f);
            std::snprintf(buf, sizeof(buf), "Raptors killed: %d", state.kills);
            m_endSub->text = buf;
        }
    }

} // namespace game
