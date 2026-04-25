#include "GameViewBuilder.hpp"

namespace ScrabbleGame {

std::string ViewBuilder::json_game_state_for_user(const u_int64_t &user_id,
                                                  GameState &state) {}

userver::formats::json::Value ViewBuilder::public_state(GameState &state) {}
userver::formats::json::Value private_state(const u_int64_t &user_id,
                                            GameState &state) {}

} // namespace ScrabbleGame
