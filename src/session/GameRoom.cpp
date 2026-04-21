#include "GameRoom.hpp"
#include "utils/utils.hpp"

namespace ScrabbleGame {

GameRoom::PlayerAction GameRoom::from_string(const std::string &str) {
    if (str == "start") {
        return PlayerAction::start;
    } else if (str == "place") {
        return PlayerAction::place;
    } else if (str == "change") {
        return PlayerAction::change;
    } else if (str == "pass") {
        return PlayerAction::pass;
    } else if (str == "submit") {
        return PlayerAction::submit;
    } else if (str == "end") {
        return PlayerAction::end;
    } else if (str == "state") {
        return PlayerAction::state;
    }
    return PlayerAction::error;
}

GameRoom::GameRoom(const u_int64_t &game_id, ScrabbleGame &&game)
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
    // TODO: to rework so that every user receives message for itself
    std::shared_lock<userver::engine::SharedMutex> lock(mutex_);
    sessions_[user_id]->send_raw_message(msg);
    return;
}

void GameRoom::receive_message(const int &user_id, const std::string &msg) {
    formats::json::Value json_msg{formats::json::FromString(msg)};

    const std::string action_str = json_msg["action"].As<std::string>();
    const PlayerAction action = from_string(action_str);

    switch (action) {
    case PlayerAction::start: {
        // action_start(message, json_msg, user_id);
        break;
    }
    case PlayerAction::change: {
        action_change(json_msg, user_id);
        break;
    }
    case PlayerAction::end: {
        action_end(json_msg, user_id);
        break;
    }
    case PlayerAction::pass: {
        action_pass(json_msg, user_id);
        break;
    }
    case PlayerAction::submit: {
        action_submit(json_msg, user_id);
        break;
    }
    case PlayerAction::place: {
        action_place(json_msg, user_id);
        break;
    }
    case PlayerAction::state: {
        game_state_for_user_(json_msg, user_id);
        break;
    }
        // TODO: more_cases
    }
}

std::string GameRoom::json_game_state_for_user(const u_int64_t &user_id) {
    formats::json::ValueBuilder json_vb;
    json_vb["pulic"] = public_state_();
    json_vb["private"] = private_state_(user_id);
    return formats::json::ToStableString(json_vb.ExtractValue());
}

formats::json::Value GameRoom::public_state_() {
    formats::json::ValueBuilder field = game_.get_game_state();
}

formats::json::Value GameRoom::private_state_(const u_int64_t &user_id) {
}

// std::string GameRoom::action_place(const formats::json::Value &json_msg,
//                                    const int &user_id) const {
//     std::vector<std::vector<int>> coordinates =
//         json_msg["coordinates"].As<std::vector<std::vector<int>>>();
//     std::string letters = json_msg["letters"].As<std::string>();
//     std::vector<char32_t> tiles = utf8str_to_utf32vec_(letters);
//     int result = game_->TryPlaceTiles(coordinates, tiles);
//     if (result == -1) {
//         message.data = R"(
//             {"score": -1}
//         )";
//         return;
//     }
//     formats::json::ValueBuilder json_vb;
//     json_vb["score"] = result;
//     message.data = formats::json::ToStableString(json_vb.ExtractValue());
//     return {};
// }

// void GameRoom::action_submit(const formats::json::Value &json_msg,
//                              const int &user_id) const {
// int game_id = json_msg["game_id"].As<int>();
// auto game = game_storage_client_->get_game(game_id);
// if (!game) {
//     message.data = std::string{"game not exists"};
//     return;
// }
// if (!(game->check_if_player_joined(user_id))) {
//     message.data = std::string{"player not joined"};
//     return;
// }
// int result = game->SubmitWord();
// if (result == -1) {
//     message.data = R"(
//         {"score": -1}
//     )";
//     return;
// }
// formats::json::ValueBuilder json_vb;
// json_vb["score"] = result;
// message.data = formats::json::ToStableString(json_vb.ExtractValue());
//     return {};
// }

// void GameRoom::action_change(const formats::json::Value &json_msg,
//                              const int &user_id) const {
//     return;
// }
// void GameRoom::action_end(const formats::json::Value &json_msg,
//                           const int &user_id) const {
//     return;
// }
// void GameRoom::action_pass(const formats::json::Value &json_msg,
//                            const int &user_id) const {
//     return;
// }

u_int64_t GameRoom::game_id() const {
    return game_id_;
}

int GameRoom::check_for_user(const u_int64_t &user_id) const {
    auto iter = std::find(players_.begin(), players_.end(), user_id);
    if (iter == players_.end())
        return -1;
    return std::distance(players_.begin(), iter);
}

bool GameRoom::start() {
    if ((int)players_.size() != game_.get_players_max())
        return false;
    ongoing_ = true;
    return true;
}

} // namespace ScrabbleGame

