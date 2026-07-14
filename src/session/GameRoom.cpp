#include "GameRoom.hpp"
#include "utils/utils.hpp"
#include <userver/formats/parse/common_containers.hpp>
#include <userver/logging/log.hpp>

namespace ScrabbleGame {

GameRoom::PlayerAction GameRoom::from_string(const std::string &str) {
    if (str == "place") {
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
    return PlayerAction::state;
}

GameRoom::GameRoom(const u_int64_t game_id, ScrabbleGame &&game)
    : game_id_{game_id}, game_(game), ongoing_{false}, open_{true} {}

void GameRoom::attach_session(const u_int64_t id,
                              std::shared_ptr<PlayerSession> session) {
    std::unique_lock<userver::engine::SharedMutex> lock(mutex_);
    players_.push_back(id);
    sessions_[id] = session;
    return;
}

const std::string GameRoom::wait_for_message(const u_int64_t user_id) {
    std::shared_lock<userver::engine::SharedMutex> lock(mutex_);
    return sessions_[user_id]->pop_wait();
}

void GameRoom::send_new_states() {
    std::shared_lock<userver::engine::SharedMutex> lock(mutex_);
    for (const auto &[user_id, session] : sessions_) {
        session->send_raw_message(json_game_state_for_user(user_id));
    }
}

void GameRoom::receive_message(const int user_id, const std::string &msg) {
    formats::json::Value json_msg{formats::json::FromString(msg)};

    const std::string action_str = json_msg["action"].As<std::string>();
    const PlayerAction action = from_string(action_str);

    if (!ongoing_ && action != PlayerAction::end &&
        action != PlayerAction::state) {
        LOG_DEBUG() << "Message declined, Game has not started";
        sessions_[user_id]->send_raw_message(
            R"({"error":"Game has not started"})");
        return;
    }

    switch (action) {
    case PlayerAction::change: {
        LOG_DEBUG() << "action_change_ is called";
        action_change_(json_msg, user_id);
        break;
    }
    case PlayerAction::end: {
        LOG_DEBUG() << "action_end_ is called";
        action_end_(json_msg, user_id);
        break;
    }
    case PlayerAction::pass: {
        LOG_DEBUG() << "action_pass_ is called";
        action_pass_(json_msg, user_id);
        break;
    }
    case PlayerAction::submit: {
        LOG_DEBUG() << "action_submit_ is called";
        action_submit_(json_msg, user_id);
        break;
    }
    case PlayerAction::place: {
        LOG_DEBUG() << "action_place_ is called";
        action_place_(json_msg, user_id);
        break;
    }
    case PlayerAction::state: {
        LOG_DEBUG() << "action_state_ is called";
        game_state_for_user_(json_msg, user_id);
        break;
    }
        // TODO: more_cases
    }
}

std::string GameRoom::json_game_state_for_user(const u_int64_t user_id) {
    formats::json::ValueBuilder json_vb;
    if (!ongoing_) {
        json_vb["ongoing"] = false;
    } else {
        json_vb["ongoing"] = true;
        LOG_TRACE() << "json_game_state_for_user: before public_state_";
        json_vb["public"] = public_state_();

        LOG_TRACE() << "json_game_state_for_user: before private_state_";
        json_vb["private"] = private_state_(user_id);
    }

    LOG_TRACE() << "json_game_state_for_user: before ToStableString";
    std::string result = formats::json::ToStableString(json_vb.ExtractValue());

    LOG_DEBUG() << "json_game_state_for_user: done";
    LOG_TRACE() << "json_for_user: " << result;
    return result;
}

formats::json::Value GameRoom::public_state_() {
    LOG_TRACE() << "public_state_: before get_game_state";
    GameState state = game_.get_game_state();
    LOG_TRACE() << "public_state_: after get_game_state, players="
                << state.players.size()
                << " playersState=" << state.playersState.size();

    formats::json::ValueBuilder json_vb;

    json_vb["current_player"] = state.current_player;
    json_vb["bag_size"] = state.bag.size();

    // players: id + score per player, index matches current_player
    json_vb["players"].Resize(state.players.size());
    for (size_t i = 0; i < state.players.size(); ++i) {
        json_vb["players"][i]["id"] = state.players[i];
        json_vb["players"][i]["score"] = state.playersState[i].score;
    }
    LOG_TRACE() << "public_state_: after players";

    // letters
    LOG_TRACE() << "public_state_: board_letters.size()="
                << state.board_letters.size() << " row0.size()="
                << (state.board_letters.empty()
                        ? -1
                        : (int)state.board_letters[0].size());
    json_vb["letters"].Resize(state.board_letters.size());
    for (size_t x = 0; x < state.board_letters.size(); ++x) {
        json_vb["letters"][x].Resize(state.board_letters[x].size());
        for (size_t y = 0; y < state.board_letters[x].size(); ++y) {
            std::string cell;
            if (state.board_letters[x][y]) {
                cell = Char32ToUtf8(*state.board_letters[x][y]);
            } else {
                cell = " "; // пустая клетка
            }
            json_vb["letters"][x][y] = cell;
        }
        LOG_TRACE() << "public_state_: letters row x=" << x << " done";
    }
    LOG_TRACE() << "public_state_: after letters";

    // prices
    json_vb["prices"].Resize(state.board_prices.size());
    for (size_t x = 0; x < state.board_prices.size(); ++x) {
        json_vb["prices"][x].Resize(state.board_prices[x].size());
        for (size_t y = 0; y < state.board_prices[x].size(); ++y) {
            json_vb["prices"][x][y] = state.board_prices[x][y];
        }
    }
    LOG_TRACE() << "public_state_: done";

    return json_vb.ExtractValue();
}

formats::json::Value GameRoom::private_state_(const u_int64_t user_id) {
    formats::json::ValueBuilder json_vb;
    LOG_TRACE() << "private_state_: before get_player_hand";
    const std::vector<char32_t> hand = game_.get_player_hand(user_id);
    LOG_TRACE() << "private_state_: after get_player_hand, hand.size()="
                << hand.size();
    json_vb["hand"].Resize(hand.size());
    for (size_t i = 0; i < hand.size(); ++i) {
        json_vb["hand"][i] = Char32ToUtf8(hand[i]);
    }
    LOG_TRACE() << "private_state_: before whose_move_id";
    if (game_.whose_move_id() == static_cast<int64_t>(user_id))
        json_vb["pending_score"] = game_.get_pending_score();
    LOG_TRACE() << "private_state_: done";
    return json_vb.ExtractValue();
}

void GameRoom::action_place_(const formats::json::Value &json_msg,
                             const int user_id) {
    if (!check_if_users_move_(user_id)) {
        sessions_[user_id]->send_raw_message(R"({"error":"Not your move"})");
        return;
    }
    std::vector<std::vector<int>> coordinates =
        json_msg["coordinates"].As<std::vector<std::vector<int>>>();
    std::string letters = json_msg["letters"].As<std::string>();
    std::vector<char32_t> tiles = utf8str_to_utf32vec_(letters);

    // TryPlaceTiles never throws: "" means the placement is valid, otherwise
    // the string is the reason to report to the player.
    std::string err =
        game_.TryPlaceTiles(std::move(coordinates), std::move(tiles));
    if (!err.empty()) {
        formats::json::ValueBuilder vb;
        vb["error"] = err;
        sessions_[user_id]->send_raw_message(
            formats::json::ToStableString(vb.ExtractValue()));
        return;
    }
    send_new_states();
}

void GameRoom::action_submit_(const formats::json::Value &json_msg,
                              const int user_id) {

    if (!check_if_users_move_(user_id)) {
        sessions_[user_id]->send_raw_message(R"({"error":"Not your move"})");
        return;
    }
    int result = game_.SubmitWord();
    if (result == -1) {
        sessions_[user_id]->send_raw_message(
            R"({"error":"Invalid placement"})");
        return;
    }
    send_new_states();
}

void GameRoom::action_change_(const formats::json::Value &json_msg,
                              const int user_id) {
    std::string tiles_utf8 = json_msg["tiles"].As<std::string>();
    std::vector<char32_t> tiles = utf8str_to_utf32vec_(tiles_utf8);

    if (!game_.Change(user_id, std::move(tiles))) {
        sessions_[user_id]->send_raw_message(R"({"error":"Invalid tiles"})");
        return;
    }
    send_new_states();
}
void GameRoom::action_end_(const formats::json::Value &json_msg,
                           const int user_id) {
    return;
}
void GameRoom::action_pass_(const formats::json::Value &json_msg,
                            const int user_id) {
    if (!check_if_users_move_(user_id)) {
        sessions_[user_id]->send_raw_message(R"({"error":"Not your move"})");
        return;
    }
    game_.Pass();
    send_new_states();
}

void GameRoom::game_state_for_user_(const formats::json::Value &json_msg,
                                    const int user_id) {
    sessions_[user_id]->send_raw_message(json_game_state_for_user(user_id));
}

void GameRoom::close_session(const u_int64_t user_id) {
    // A player's connection is gone: mark the room closed and end the game,
    // then close every session so each blocked send loop wakes up, sees
    // session_open() == false and stops instead of spinning on a closing
    // connection. open_ must be cleared before waking the loops.
    //
    // TODO: this ends the whole game when any single player disconnects, since
    // open_/ongoing_ are room-scoped and there is no per-player connection
    // state yet. Distinguish "one player disconnected" (close only user_id's
    // session, keep the game ongoing / allow reconnect) from "game ended"
    // (close all).
    open_ = false;
    ongoing_ = false;
    std::shared_lock<userver::engine::SharedMutex> lock(mutex_);
    for (auto &[id, session] : sessions_)
        session->Close();
}

bool GameRoom::ongoing() const { return ongoing_; }

bool GameRoom::session_open() const { return open_; }

void GameRoom::set_players() {
    std::unique_lock<userver::engine::SharedMutex> lock(mutex_);
    // players_ is the authoritative membership list (the same one
    // check_for_user validates against), so derive the game's player list
    // from it instead of a separate query to keep the two in sync.
    std::vector<int64_t> players(players_.begin(), players_.end());
    game_.set_players(std::move(players));
}

u_int64_t GameRoom::game_id() const { return game_id_; }

int GameRoom::check_for_user(const u_int64_t user_id) const {
    auto iter = std::ranges::find(players_, user_id);
    if (iter == players_.end())
        return -1;
    return std::distance(players_.begin(), iter);
}

bool GameRoom::check_if_users_move_(const int user_id) {
    if (game_.whose_move_id() != user_id)
        return false;
    return true;
}

bool GameRoom::start() {
    if ((int)players_.size() != game_.get_players_max())
        return false;
    ongoing_ = true;
    return true;
}

} // namespace ScrabbleGame
