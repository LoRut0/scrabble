#include "game/Player.hpp"
#include "game/ScrabbleGame.hpp"
#include "session/PlayerSession.hpp"
#include <map>
#include <memory>
#include <userver/engine/mutex.hpp>
#include <userver/engine/shared_mutex.hpp>

namespace ScrabbleGame {

/*
 * Should store info about players of gae, their status ??(connected, offline,
 * afk)?
 * - map<user_id, PlayerSession>
 * - broadcast logic
 * - notifying connected players
 * - if game is ongoing
 */

class GameRoom {
  public:
    GameRoom(const u_int64_t &game_id, ScrabbleGame &&game);
    void attach_session(const u_int64_t &user_id,
                        std::shared_ptr<PlayerSession> session);

    const std::string wait_for_message(const u_int64_t &user_id);
    void send_message(const u_int64_t &user_id, const std::string &msg);

    void receive_message(const int &user_id, const std::string &msg);

    u_int64_t game_id() const;

    /*
     * Checks for user existence in vector players_
     * @returns {index} in players_ or {-1} if player is not in game
     */
    int check_for_user(const u_int64_t &user_id) const;

    // TODO: some logic for ongoing_ game
    bool start();

    // TODO: something for detaching session, ending session

    std::string json_game_state_for_user(const u_int64_t &user_id);

  private:
    const u_int64_t game_id_;
    ScrabbleGame game_;
    bool ongoing_;
    /*
     * @brief Vector with user_ids of players
     * @note [0] is the admin of game
     */
    std::vector<u_int64_t> players_;
    std::map<u_int64_t, std::shared_ptr<PlayerSession>> sessions_;
    userver::engine::SharedMutex mutex_;

    enum class PlayerAction {
        // start a game
        start,
        // try to place tiles on board
        place,
        // change tiles in hand
        change,
        // pass the move
        pass,
        // try to submit tiles placed on board
        submit,
        // end a game
        end,
        // try to receive cur game_state in json
        state,
        error
    };

    PlayerAction from_string(const std::string &str);

    std::string action_place(const formats::json::Value &json_msg,
                             const int &user_id) const;
    // std::string action_start(
    //                   const formats::json::Value &json_msg,
    //                   const int &user_id) const;
    std::string action_change(const formats::json::Value &json_msg,
                              const int &user_id) const;
    std::string action_end(const formats::json::Value &json_msg,
                           const int &user_id) const;
    std::string action_pass(const formats::json::Value &json_msg,
                            const int &user_id) const;
    std::string action_submit(const formats::json::Value &json_msg,
                              const int &user_id) const;
    std::string game_state_for_user_(const formats::json::Value &json_msg,
                                     const int &user_id);

    formats::json::Value public_state_();
    formats::json::Value private_state_(const u_int64_t &user_id);
};

} // namespace ScrabbleGame
