#include "game/Game.h"
#include <exception>
#include <spdlog/spdlog.h>

int main() {
    try {
        game::Game game;
        game.Run();
    } catch (const std::exception& e) {
        spdlog::critical("{}", e.what());
        return 1;
    }
    return 0;
}
