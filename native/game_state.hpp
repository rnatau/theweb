#pragma once
#include "rules/session_rules.hpp"
namespace web {
class GameState {
public:
    rules::PlayerRoster players;
    rules::TurnState turn{players};
    rules::SharedGameState shared{players};
    GameState() = default;
    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;
    rules::Player& active_player() {
        return players.at(unsigned(turn.current_player));
    }
    const rules::Player& active_player() const {
        return players.at(unsigned(turn.current_player));
    }
};
} // namespace web
