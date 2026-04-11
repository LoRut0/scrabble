#include "game/Player.hpp"
#include "game/ScrabbleGame.hpp"
#include "session/PlayerSession.hpp"
#include <map>
#include <memory>
#include <userver/engine/mutex.hpp>
#include <userver/engine/shared_mutex.hpp>

/*
 * Should store info about players of gae, their status ??(connected, offline,
 * afk)?
 * - map<user_id, PlayerSession>
 * - broadcast logic
 * - notifying connected players
 * - if game is ongoing
 */

namespace ScrabbleGame {
class GameRoom {
  public:
    GameRoom(u_int64_t game_id, ScrabbleGame &&game);
    void attach_session(const u_int64_t &user_id,
                        std::shared_ptr<PlayerSession> session);

    const std::string wait_for_message(const u_int64_t &user_id);
    void send_message(const u_int64_t &user_id, const std::string &msg);

    // TODO: something for detaching session, ending session

  private:
    u_int64_t game_id_;
    ScrabbleGame game_;
    bool ongoing_;
    /*
     * @brief Vector with user_ids of players
     * @note [0] is the admin of game
     */
    std::vector<u_int64_t> players_;
    std::map<u_int64_t, std::shared_ptr<PlayerSession>> sessions_;
    userver::engine::SharedMutex mutex_;
};

} // namespace ScrabbleGame
