#pragma once
#include "eng/GameObject.h"
#include "game/GameState.h"
#include "ui/Widget.h"
#include <vector>

namespace game
{

    // In-game overlay built once from retained widgets: health bar, weapon/ammo panel, crosshair,
    // interaction prompt, kill counter and the end-of-game banner. Update() only changes widget state.
    class Hud
    {
    public:
        explicit Hud(ui::Widget &parent);

        void SetVisible(bool visible) { m_root->visible = visible; }
        void Update(eng::GameObject &player, const GameState &state, float time);

    private:
        ui::Widget *m_root = nullptr;

        ui::Panel *m_vignetteTop = nullptr;
        ui::Panel *m_vignetteBottom = nullptr;
        ui::ProgressBar *m_healthBar = nullptr;
        ui::Label *m_healthText = nullptr;

        ui::Panel *m_weaponPanel = nullptr;
        ui::Label *m_weaponName = nullptr;
        ui::Label *m_weaponMode = nullptr;
        ui::Label *m_ammo = nullptr;
        ui::Label *m_reserve = nullptr;
        ui::Label *m_weaponStatus = nullptr;
        ui::ProgressBar *m_reloadBar = nullptr;
        ui::Label *m_unarmed = nullptr;
        ui::Label *m_unarmedHint = nullptr;

        ui::Label *m_kills = nullptr;
        ui::Label *m_prompt = nullptr;
        std::vector<ui::Panel *> m_crosshair;

        ui::Panel *m_endOverlay = nullptr;
        ui::Label *m_endTitle = nullptr;
        ui::Label *m_endSub = nullptr;
    };

} // namespace game
