#pragma once
#include "core/Window.h"
#include "eng/World.h"
#include "game/GameState.h"
#include "gfx/PostProcess.h"

namespace game {

class Game {
public:
    Game();
    void Run();

private:
    void BuildWorld();

    core::Window m_window;
    eng::World m_world;
    gfx::PostProcess m_post;
    GameState m_state;
    eng::GameObject* m_player = nullptr;
};

} // namespace game
