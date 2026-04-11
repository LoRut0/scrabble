#include "GameRoom.hpp"

namespace ScrabbleGame {

GameRoom::GameRoom(u_int64_t game_id, ScrabbleGame &&game)
    : game_id_{game_id}, game_(game), ongoing_{false} {}

void GameRoom::attach_session(const u_int64_t &id,
                              std::shared_ptr<PlayerSession> session) {
    std::unique_lock<userver::engine::SharedMutex> lock(mutex_);
    sessions_[id] = session;
    return;
}

const std::string GameRoom::wait_for_message(const u_int64_t &user_id) {
    std::shared_lock<userver::engine::SharedMutex> lock(mutex_);
    return sessions_[user_id]->pop_wait();
}

void GameRoom::send_message(const u_int64_t &user_id, const std::string &msg) {
    std::shared_lock<userver::engine::SharedMutex> lock(mutex_);
    sessions_[user_id]->send_raw_message(msg);
    return;
}

} // namespace ScrabbleGame
