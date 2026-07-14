#include "game/Player.hpp"
#include "game/ScrabbleGame.hpp"
#include "session/PlayerSession.hpp"
#include <atomic>
#include <map>
#include <memory>
#include <userver/engine/mutex.hpp>
#include <userver/engine/shared_mutex.hpp>

namespace ScrabbleGame {

/*
 * Should store info about players of game, their status ??(connected, offline,
 * afk)?
 * - map<user_id, PlayerSession>
 * - broadcast logic
 * - notifying connected players
 * - if game is ongoing
 */

class GameRoom {
  public:
    GameRoom(const u_int64_t game_id, ScrabbleGame &&game);
    void attach_session(const u_int64_t user_id,
                        std::shared_ptr<PlayerSession> session);

    const std::string wait_for_message(const u_int64_t user_id);
    void send_new_states();

    void receive_message(const int user_id, const std::string &msg);

    u_int64_t game_id() const;

    /*
     * Checks for user existence in vector players_
     * @returns {index} in players_ or {-1} if player is not in game
     */
    int check_for_user(const u_int64_t user_id) const;

    /*
     * @brief registers this room's players_ (the single source of truth,
     *        also used by check_for_user) in the underlying ScrabbleGame,
     *        filling state_.players/playersState from it
     */
    void set_players();

    // TODO: some logic for ongoing_ game
    bool start();

    /*
     * @brief whether the game in this room is still running
     * @note the websocket read/send loops poll this to know when to stop
     *       serving a client (set false once the game ends or a player's
     *       connection is closed)
     */
    bool ongoing() const;

    /*
     * @brief whether the given user still has an open (not closed) session
     * @note the websocket send loop polls this to decide whether to keep
     *       serving the connection; unlike ongoing() this is true during the
     *       pre-start lobby too, so lobby messages still get delivered
     */
    bool session_open() const;

    void close_session(const u_int64_t user_id);

    std::string json_game_state_for_user(const u_int64_t user_id);

  private:
    const u_int64_t game_id_;
    ScrabbleGame game_;
    // Atomic so the websocket loops can poll ongoing() without taking mutex_,
    // which wait_for_message() holds (in shared mode) while blocked.
    std::atomic<bool> ongoing_;
    // open_ is true from the initialization
    std::atomic<bool> open_;
    /*
     * @brief Vector with user_ids of players
     * @note [0] is the admin of game
     */
    std::vector<u_int64_t> players_;
    std::map<u_int64_t, std::shared_ptr<PlayerSession>> sessions_;
    userver::engine::SharedMutex mutex_;

    enum class PlayerAction {
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
        state
    };

    bool check_if_users_move_(const int user_id);

    PlayerAction from_string(const std::string &str);

    void action_place_(const formats::json::Value &json_msg, const int user_id);
    void action_change_(const formats::json::Value &json_msg,
                        const int user_id);
    void action_end_(const formats::json::Value &json_msg, const int user_id);
    void action_pass_(const formats::json::Value &json_msg, const int user_id);
    void action_submit_(const formats::json::Value &json_msg,
                        const int user_id);
    void game_state_for_user_(const formats::json::Value &json_msg,
                              const int user_id);

    formats::json::Value public_state_();
    formats::json::Value private_state_(const u_int64_t user_id);
};

} // namespace ScrabbleGame
