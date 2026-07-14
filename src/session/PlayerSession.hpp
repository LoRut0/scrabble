#include <queue>
#include <string>
#include <userver/engine/condition_variable.hpp>
#include <userver/engine/mutex.hpp>

namespace ScrabbleGame {

using namespace userver;

/*
 * Represents relation between 1 person tand GameSession
 * Manages:
 * - websocket connection
 * - coroutines with chat.Send, chat.TryRecv
 *
 * @note should be closed before deleting
 */
class PlayerSession {
  private:
    std::queue<std::string> send_queue_;
    engine::Mutex mutex_;
    engine::ConditionVariable cv_;

    bool closed_{false};

  public:
    void send_raw_message(const std::string &msg);
    const std::string pop_wait();

    void Close();

    /*
     * @brief whether Close() has been called on this session
     * @note the websocket send loop polls this to know if the connection is
     *       still alive (independent of whether the game is ongoing)
     */
    bool closed();

    PlayerSession() = default;
    PlayerSession(PlayerSession &&) = delete;
    PlayerSession(PlayerSession &) = delete;
    PlayerSession &operator=(const PlayerSession &) = delete;
    PlayerSession &operator=(PlayerSession &&) = delete;
};

} // namespace ScrabbleGame
