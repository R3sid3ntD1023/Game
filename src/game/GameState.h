#pragma once
#include <string>

namespace game
{

    enum class Status
    {
        Playing,
        Won,
        Lost
    };

    struct GameState
    {
        Status status = Status::Playing;
        int kills = 0;
        std::string prompt; // interaction hint for the object being looked at
    };

} // namespace game
