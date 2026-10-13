#pragma once

namespace game {

enum class Status { Playing, Won, Lost };

struct GameState {
    Status status = Status::Playing;
    int kills = 0;
};

} // namespace game
