#include "ScrabbleGame.hpp"
#include "handlers.hpp"
#include "utfcpp/source/utf8.h"
#include "utils.hpp"
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <userver/crypto/crypto.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/websocket/server.hpp>
#include <userver/storages/sqlite/operation_types.hpp>
#include <userver/storages/sqlite/result_set.hpp>
#include <vector>

#define MAX_SEQ 20

/*
 * @brief returns user_id by jti token
 * @param {jti} jti token
 * @retval {user_id} 100% only one value
 */
inline constexpr std::string_view SqlUserIdByJti = R"~(
    SELECT user_id 
    FROM auth_tokens tok
        WHERE jti = $1
)~";
/*
 * @brief returns game_id by user_id of host
 * @param {user_id} host player id
 * @retval {game_id} 100% one value
 */
inline constexpr std::string_view SqlFindGameByHost = R"~(
    SELECT game_id
    FROM game_users
        WHERE user_id == $1
)~";
/*
 * @brief returns all players of game with host at FIRST place
 * @param {game_id}
 */
inline constexpr std::string_view SqlGetAllPlayers = R"~(
    SELECT
        gu.user_id
    FROM game_users gu
    JOIN games g ON g.id = gu.game_id
    WHERE gu.game_id = $1
    ORDER BY
        CASE
            WHEN gu.user_id = g.host_user_id THEN 0
            ELSE 1
        END,
        gu.user_id;
)~";

namespace services::websocket {

namespace Actions {
    inline PlayerAction from_string(std::string str)
    {
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
        throw server::handlers::ClientError(server::handlers::ExternalBody { "Wrong Player-req.action" });
    }
};

WebsocketsHandler::WebsocketsHandler(const components::ComponentConfig& config, const components::ComponentContext& context)
    : server::websocket::WebsocketHandlerBase(config, context)
    , sqlite_client_(context.FindComponent<components::SQLite>("sqlitedb").GetClient())
    , game_storage_client_(context.FindComponent<ScrabbleGame::Storage::StorageComponent>("game_storage").GetStorage()) { };

void WebsocketsHandler::Handle(server::websocket::WebSocketConnection& chat, server::request::RequestContext&) const
{
    server::websocket::Message message;
    while (!engine::current_task::ShouldCancel()) {
        chat.Recv(message); // throws on closed/dropped connection
        if (message.close_status)
            break; // explicit close if any
        ParseMessage(message);
        chat.Send(message); // throws on closed/dropped connection
    }
    if (message.close_status)
        chat.Close(*message.close_status);
}

int WebsocketsHandler::define_user_id_from_token_(const std::string& token) const
{
    const std::string jti = userver::crypto::hash::Sha256(token);
    storages::sqlite::ResultSet result = sqlite_client_->Execute(storages::sqlite::OperationType::kReadOnly, SqlUserIdByJti.data(), jti);
    std::optional<int> user_id = std::move(result).AsOptionalSingleField<int>();
    if (!user_id)
        throw server::handlers::ClientError(server::handlers::ExternalBody { "invalid token from user" });
    return user_id.value();
}

void WebsocketsHandler::ParseMessage(server::websocket::Message& message) const
{
    if (!message.is_text) {
        throw server::handlers::ClientError(server::handlers::ExternalBody { "No json in request" });
    }
    formats::json::Value json_msg { formats::json::FromString(std::string_view(message.data)) };
    const int user_id = define_user_id_from_token_(json_msg["token"].As<std::string>());

    const std::string action_str = json_msg["action"].As<std::string>();
    const Actions::PlayerAction action = Actions::from_string(action_str);

    switch (action) {
    case Actions::PlayerAction::start: {
        action_start(message, json_msg, user_id);
        break;
    }
    case Actions::PlayerAction::change: {
        action_change(message, json_msg, user_id);
        break;
    }
    case Actions::PlayerAction::end: {
        action_end(message, json_msg, user_id);
        break;
    }
    case Actions::PlayerAction::pass: {
        action_pass(message, json_msg, user_id);
        break;
    }
    case Actions::PlayerAction::submit: {
        action_submit(message, json_msg, user_id);
        break;
    }
    case Actions::PlayerAction::place: {
        action_place(message, json_msg, user_id);
        break;
    }
    case Actions::PlayerAction::state: {
        int game_id = json_msg["game_id"].As<int>();
        current_game_state_for_user_(message, user_id, game_id);
        break;
    }

        // TODO: more_cases
    }
}

void WebsocketsHandler::current_game_state_for_user_(server::websocket::Message& message, const int& user_id, const int& game_id) const
{
    LOG_DEBUG() << "current_game_state";
    formats::json::ValueBuilder json_vb;
    auto game = game_storage_client_->get_game(game_id);
    if (!game) {
        LOG_DEBUG() << "No game pointer";
        return;
    }
    int player_index = game->check_if_player_joined(user_id);
    if (!player_index) {
        message.data = R"({"error":404, "comment":"User not joined"})";
        return;
    }
    auto game_state = game->get_game_state();

    json_vb = game_state;

    for (size_t i = 0; i < game_state.playersState[player_index].hand.size(); ++i) {
        std::string tile;

        tile = Char32ToUtf8(game_state.playersState[player_index].hand[i]);

        json_vb["tiles"][i] = tile;
    }

    json_vb["score"] = game_state.playersState[player_index].score;

    message.data = formats::json::ToStableString(json_vb.ExtractValue());
    return;
}

void WebsocketsHandler::action_start(server::websocket::Message& message, const formats::json::Value& json_msg, const int& user_id) const
{
    storages::sqlite::ResultSet result = sqlite_client_->Execute(userver::v2_::storages::sqlite::OperationType::kReadOnly, SqlFindGameByHost.data(), user_id);
    std::optional<int> game_id = std::move(result).AsOptionalSingleField<int>();
    // TODO: all games of player must end, if he is not host
    if (!game_id) {
        message.close_status = server::websocket::CloseStatus::kBadMessageData;
        message.data = "Player is not host";
        return;
    }
    result = sqlite_client_->Execute(userver::v2_::storages::sqlite::OperationType::kReadOnly, SqlGetAllPlayers.data(), game_id);
    std::vector<int64_t> all_players = std::move(result).AsVector<int64_t>();

    std::shared_ptr<ScrabbleGame::ScrabbleGame> game = game_storage_client_->get_game(*game_id);
    if (!game) {
        LOG_DEBUG() << "No game pointer";
        return;
    }
    game->set_players(all_players);
    game->start();

    current_game_state_for_user_(message, user_id, *game_id);
    return;
}

inline constexpr std::string_view SqlCheckIfUserAlreadyJoined = R"~(
    SELECT 1 FROM game_users
    WHERE game_id = $1 AND user_id = $2
)~";

bool WebsocketsHandler::check_if_user_joined_(const int& game_id, const int& user_id) const
{
    storages::sqlite::ResultSet result = sqlite_client_->Execute(userver::v2_::storages::sqlite::OperationType::kReadWrite, SqlCheckIfUserAlreadyJoined.data(), game_id, user_id);
    std::optional<int> field = std::move(result).AsOptionalSingleField<int>();
    if (!field)
        return 0;
    return 1;
}

void WebsocketsHandler::action_place(server::websocket::Message& message, const formats::json::Value& json_msg, const int& user_id) const
{
    int game_id = json_msg["game_id"].As<int>();
    auto game = game_storage_client_->get_game(game_id);
    if (!game) {
        message.data = std::string { "game not exists" };
        return;
    }
    if (!(game->check_if_player_joined(user_id))) {
        message.data = std::string { "player not joined" };
        return;
    }
    std::vector<std::vector<int>> coordinates = json_msg["coordinates"].As<std::vector<std::vector<int>>>();
    std::string letters = json_msg["letters"].As<std::string>();
    std::vector<char32_t> tiles = utf8str_to_utf32vec_(letters);
    int result = game->TryPlaceTiles(coordinates, tiles);
    if (result == -1) {
        message.data = R"(
            {"score": -1}
        )";
        return;
    }
    formats::json::ValueBuilder json_vb;
    json_vb["score"] = result;
    message.data = formats::json::ToStableString(json_vb.ExtractValue());
    return;
}

void WebsocketsHandler::action_submit(server::websocket::Message& message, const formats::json::Value& json_msg, const int& user_id) const
{
    int game_id = json_msg["game_id"].As<int>();
    auto game = game_storage_client_->get_game(game_id);
    if (!game) {
        message.data = std::string { "game not exists" };
        return;
    }
    if (!(game->check_if_player_joined(user_id))) {
        message.data = std::string { "player not joined" };
        return;
    }
    int result = game->SubmitWord();
    if (result == -1) {
        message.data = R"(
            {"score": -1}
        )";
        return;
    }
    formats::json::ValueBuilder json_vb;
    json_vb["score"] = result;
    message.data = formats::json::ToStableString(json_vb.ExtractValue());
    return;
}

void WebsocketsHandler::action_change(server::websocket::Message& message, const formats::json::Value& json_msg, const int& user_id) const
{
    return;
}
void WebsocketsHandler::action_end(server::websocket::Message& message, const formats::json::Value& json_msg, const int& user_id) const
{
    return;
}
void WebsocketsHandler::action_pass(server::websocket::Message& message, const formats::json::Value& json_msg, const int& user_id) const
{
    return;
}

void WebsocketsHandler::DefaultInit(formats::json::ValueBuilder& json_for_redis, int64_t gameID) const
{
    // json_for_redis["gameID"] = gameID;
    // // TODO: random queue at the beginning of game
    // json_for_redis["state"]["playerq"] = 0;
    // json_for_redis["state"]["rollnum"] = 0;
    //
    // json_for_redis["state"]["dices"].Resize(5);
    // for (int i = 0; i < 5; i++) {
    //     json_for_redis["state"]["dices"][i] = -1;
    // }
    //
    // json_for_redis["state"]["p1"]["sequences"] = DiceGame::Combinations(-1);
    // json_for_redis["state"]["p2"]["sequences"] = DiceGame::Combinations(-1);
    // json_for_redis["state"]["cur_sequences"] = DiceGame::Combinations(-1);

    return;
}

template <typename T>
std::vector<T> WebsocketsHandler::ToVector(const formats::json::Value& json_vl) const
{
    std::vector<T> to_return;
    to_return.reserve(json_vl.GetSize());
    for (const auto& elem : json_vl) {
        to_return.push_back(elem.As<T>());
    }
    return to_return;
}
}
