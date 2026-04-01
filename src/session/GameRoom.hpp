#include "game/Player.hpp"
#include "game/ScrabbleGame.hpp"
#include "session/PlayerSession.hpp"
#include <map>
#include <memory>
#include <userver/engine/mutex.hpp>
#include <userver/engine/shared_mutex.hpp>

namespace ScrabbleGame {
class GameRoom {
  public:
    GameRoom(int game_id, unsigned max_players);
    int attach_session(PlayerSession session);

  private:
    ScrabbleGame game_;
    unsigned players_max_;
    bool ongoing_;
    int game_id_;
    /*
     * @brief Vector with user_ids of players
     */
    std::vector<int> players_;
    std::map<int, std::weak_ptr<PlayerSession>> ws_connections_;
    userver::engine::SharedMutex mutex_;
};

} // namespace ScrabbleGame
