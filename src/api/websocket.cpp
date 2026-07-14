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
    // Serve the connection for as long as the session is open: this covers the
    // pre-start lobby (game not ongoing yet) as well as the running game. The
    // loop ends when the session is closed (client disconnect or game end, both
    // of which call close_session() -> PlayerSession::Close()).
    while (game->session_open()) {
        std::string msg_to_user = game->wait_for_message(user_id);
        // close_session() woke us up with nothing to send: stop instead of
        // pushing onto a closing connection.
        if (!game->session_open())
            break;
        server::websocket::Message msg;
        msg.data = std::move(msg_to_user);
        // Mark as a text frame: without this userver sends a binary frame, and
        // browsers hand binary frames to onmessage as a Blob (not a string), so
        // the JS JSON.parse fails / the message looks "missing". The Python
        // test client accepts bytes, which is why tests passed but the browser
        // never saw the payload.
        msg.is_text = true;
        std::unique_lock<engine::Mutex> lock(mutex);
        LOG_DEBUG() << "Sending message to user=" << user_id
                    << " message = " << msg.data;
        chat.Send(msg);
    }
    LOG_DEBUG() << "send_loop_: session closed, stopping";
    return;
}
void WebsocketsHandler::read_loop_(server::websocket::WebSocketConnection &chat,
                                   engine::Mutex &mutex,
                                   std::shared_ptr<ScrabbleGame::GameRoom> game,
                                   const int &user_id) const {
    LOG_DEBUG() << "read_loop_: started";
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
        LOG_DEBUG() << "read_loop_: got message from user = " << user_id
                    << ", close_status=" << (bool)msg.close_status
                    << " data=" << msg.data;
        // TODO: rework this step with atomic bool (I dont remember why :( )
        if (msg.close_status) {
            chat.Close(*msg.close_status);
            game->close_session(user_id);
            return;
        }
        game->receive_message(user_id, msg.data);
        LOG_DEBUG() << "read_loop_: message processed " << msg.data;
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
}

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
