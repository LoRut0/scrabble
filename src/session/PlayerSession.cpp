#include "PlayerSession.hpp"

namespace ScrabbleGame {

void PlayerSession::send_raw_message(const std::string &msg) {
    std::unique_lock<engine::Mutex> lock(mutex_);
    send_queue_.push(msg);
    cv_.NotifyOne();
    return;
}

const std::string PlayerSession::pop_wait() {
    std::unique_lock<engine::Mutex> lock(mutex_);
    (void)cv_.Wait(lock, [&] { return closed_ or !send_queue_.empty(); });
    if (closed_)
        return "";
    const std::string msg = send_queue_.front();
    send_queue_.pop();
    return msg;
}

void PlayerSession::Close() {
    std::unique_lock<engine::Mutex> lock(mutex_);
    closed_ = true;
    cv_.NotifyAll();
    return;
}

} // namespace ScrabbleGame
