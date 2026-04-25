#include "ScrabbleGame.hpp"
#include <userver/formats/json/value.hpp>

namespace ScrabbleGame {

class ViewBuilder {
  public:
    std::string json_game_state_for_user(const u_int64_t &user_id,
                                         GameState &state);

  private:
    userver::formats::json::Value public_state(GameState &state);
    userver::formats::json::Value private_state(const u_int64_t &user_id,
                                                GameState &state);
};

} // namespace ScrabbleGame
