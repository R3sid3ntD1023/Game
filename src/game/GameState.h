#pragma once
#include "game/Ecology.h"
#include <memory>
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
        std::shared_ptr<Ecology> ecology = std::make_shared<Ecology>(); // plants, herbivores, raptors and pack calls
    };

} // namespace game
