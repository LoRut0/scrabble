#include "websocket.hpp"
#include "utils/utils.hpp"
#include <memory>
#include <optional>
#include <string_view>
#include <userver/crypto/crypto.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/sleep.hpp>
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

WebsocketsHandler::WebsocketsHandler(
    const components::ComponentConfig &config,
    const components::ComponentContext &context)
    : server::websocket::WebsocketHandlerBase(config, context),
      sqlite_client_(
          context.FindComponent<components::SQLite>("sqlitedb").GetClient()),
      game_storage_client_(
          context.FindComponent<ScrabbleGame::StorageComponent>("game_storage")
              .GetStorage()) {};

void WebsocketsHandler::Handle(server::websocket::WebSocketConnection &chat,
                               server::request::RequestContext &) const {
    const auto [game_id, user_id] = init_user_id_(chat);
    std::shared_ptr<ScrabbleGame::GameRoom> game =
        game_storage_client_->get_game_room(game_id);
    LOG_DEBUG() << "GameRoom with id = " << game->game_id() << " was received";
    engine::Mutex mutex;
    auto send_loop =
        engine::AsyncNoSpan([&chat, &mutex, &game, &user_id, this] {
            send_loop_(chat, mutex, game, user_id);
        });
    read_loop_(chat, mutex, game, user_id);

    return;
}

std::pair<u_int64_t, int> WebsocketsHandler::init_user_id_(
    server::websocket::WebSocketConnection &chat) const {
    server::websocket::Message msg;
    chat.Recv(msg);
    const std::string token =
        formats::json::FromString(msg.data)["token"].As<std::string>();
    const int user_id = define_user_id_from_token_(token);
    const u_int64_t game_id =
        formats::json::FromString(msg.data)["game_id"].As<u_int64_t>();
    std::shared_ptr<ScrabbleGame::GameRoom> game =
        game_storage_client_->get_game_room(game_id);
    if (!game) {
        chat.Close(
            userver::v2_16_rc::server::websocket::CloseStatus::kBadMessageData);
        throw server::handlers::ClientError(
            server::handlers::ExternalBody{"Game doesn't exist"});
    }
    if (game->check_for_user(user_id) == -1) {
        chat.Close(
            userver::v2_16_rc::server::websocket::CloseStatus::kBadMessageData);
        throw server::handlers::ClientError(
            server::handlers::ExternalBody{"User has not joined in game"});
    }
    return {game_id, user_id};
}

void WebsocketsHandler::send_loop_(server::websocket::WebSocketConnection &chat,
                                   engine::Mutex &mutex,
                                   std::shared_ptr<ScrabbleGame::GameRoom> game,
                                   const int &user_id) const {
    LOG_DEBUG() << "Started asyncrous send loop";
    while (true) {
        std::string msg_to_user = game->wait_for_message(user_id);
        server::websocket::Message msg;
        msg.data = std::move(msg_to_user);
        std::unique_lock<engine::Mutex> lock(mutex);
        chat.Send(msg);
        // TODO: may be check for some atomic bool to define if we must continue
    }
    return;
}
void WebsocketsHandler::read_loop_(server::websocket::WebSocketConnection &chat,
                                   engine::Mutex &mutex,
                                   std::shared_ptr<ScrabbleGame::GameRoom> game,
                                   const int &user_id) const {
    server::websocket::Message msg;
    while (true) {
        int res;
        {
            std::unique_lock<engine::Mutex> lock(mutex);
            res = chat.TryRecv(msg);
        }
        if (!res) {
            engine::SleepFor(std::chrono::milliseconds(100));
            continue;
        }
        // TODO: rework this step with atomic bool
        if (msg.close_status) {
            chat.Close(*msg.close_status);
            return;
        }
        LOG_TRACE() << "Message received " << msg.data;
        // TODO: not send message but receive message
        game->send_message(user_id, msg.data);
    }
    return;
}

int WebsocketsHandler::define_user_id_from_token_(
    const std::string &token) const {
    const std::string jti = userver::crypto::hash::Sha256(token);
    storages::sqlite::ResultSet result = sqlite_client_->Execute(
        userver::v2_16_rc::storages::sqlite::OperationType::kReadOnly,
        SqlUserIdByJti.data(), jti);
    std::optional<int> user_id = std::move(result).AsOptionalSingleField<int>();
    if (!user_id)
        throw server::handlers::ClientError(
            server::handlers::ExternalBody{"invalid token from user"});
    return user_id.value();
}

void WebsocketsHandler::current_game_state_for_user_(
    server::websocket::Message &message, const int &user_id,
    const int &game_id) const {
    LOG_DEBUG() << "current_game_state";
    formats::json::ValueBuilder json_vb;
    auto game = game_storage_client_->get_game_room(game_id);
    if (!game) {
        LOG_DEBUG() << "No game pointer";
        return;
    }
    int player_index = game->check_for_user(user_id);
    if (!player_index) {
        message.data = R"({"error":404, "comment":"User not joined"})";
        return;
    }
    return;

    // TODO: game state for user exclusive

    // auto game_state = game->get_game_state();
    //
    // json_vb = game_state;
    //
    // for (size_t i = 0; i < game_state.playersState[player_index].hand.size();
    //      ++i) {
    //     std::string tile;
    //
    //     tile = Char32ToUtf8(game_state.playersState[player_index].hand[i]);
    //
    //     json_vb["tiles"][i] = tile;
    // }
    //
    // json_vb["score"] = game_state.playersState[player_index].score;
    //
    // message.data = formats::json::ToStableString(json_vb.ExtractValue());
    // return;
}

// void WebsocketsHandler::action_start(server::websocket::Message &message,
//                                      const formats::json::Value &json_msg,
//                                      const int &user_id) const {
//     storages::sqlite::ResultSet result = sqlite_client_->Execute(
//         userver::v2_16_rc::storages::sqlite::OperationType::kReadOnly,
//         SqlFindGameByHost.data(), user_id);
//     std::optional<int> game_id =
//     std::move(result).AsOptionalSingleField<int>();
//     // TODO: all games of player must end, if he is not host
//     if (!game_id) {
//         message.close_status =
//         server::websocket::CloseStatus::kBadMessageData; message.data =
//         "Player is not host"; return;
//     }
//     result = sqlite_client_->Execute(
//         userver::v2_16_rc::storages::sqlite::OperationType::kReadOnly,
//         SqlGetAllPlayers.data(), game_id);
//     std::vector<int64_t> all_players = std::move(result).AsVector<int64_t>();
//
//     std::shared_ptr<ScrabbleGame::ScrabbleGame> game =
//         game_storage_client_->get_game(*game_id);
//     if (!game) {
//         LOG_DEBUG() << "No game pointer";
//         return;
//     }
//     game->set_players(all_players);
//     game->start();
//
//     current_game_state_for_user_(message, user_id, *game_id);
//     return;
// }

inline constexpr std::string_view SqlCheckIfUserAlreadyJoined = R"~(
    SELECT 1 FROM game_users
    WHERE game_id = $1 AND user_id = $2
)~";

bool WebsocketsHandler::check_if_user_joined_(const int &game_id,
                                              const int &user_id) const {
    storages::sqlite::ResultSet result = sqlite_client_->Execute(
        userver::v2_16_rc::storages::sqlite::OperationType::kReadWrite,
        SqlCheckIfUserAlreadyJoined.data(), game_id, user_id);
    std::optional<int> field = std::move(result).AsOptionalSingleField<int>();
    if (!field)
        return 0;
    return 1;
}

void WebsocketsHandler::DefaultInit(formats::json::ValueBuilder &json_for_redis,
                                    int64_t gameID) const {
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
std::vector<T>
WebsocketsHandler::ToVector(const formats::json::Value &json_vl) const {
    std::vector<T> to_return;
    to_return.reserve(json_vl.GetSize());
    for (const auto &elem : json_vl) {
        to_return.push_back(elem.As<T>());
    }
    return to_return;
}
} // namespace services::websocket
