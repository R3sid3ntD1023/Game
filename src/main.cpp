#include "game/Game.h"
#include <exception>
#include <spdlog/spdlog.h>
#include <string>

// Usage: Game [world-name] [--regen]
// With no world name the main menu is shown; with one, that world starts immediately.
int main(int argc, char **argv)
{
    std::string world;
    bool regenerate = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--regen")
            regenerate = true;
        else if (!arg.empty() && arg[0] != '-')
            world = arg;
    }

    try
    {
        game::Game game(world, regenerate);
        game.Run();
    }
    catch (const std::exception &e)
    {
        spdlog::critical("{}", e.what());
        return 1;
    }
    return 0;
}
