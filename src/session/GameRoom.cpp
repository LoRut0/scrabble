#include "GameRoom.hpp"

ScrabbleGame::GameRoom::GameRoom(int game_id, unsigned max_players)
    : players_max_{max_players}, ongoing_{false}, game_id_{game_id} {}
