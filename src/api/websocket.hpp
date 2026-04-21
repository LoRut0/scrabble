#pragma once
#include "session/GameStorage.hpp"
#include <userver/server/websocket/websocket_handler.hpp>
#include <userver/storages/sqlite/client.hpp>
#include <userver/storages/sqlite/component.hpp>
#include <userver/storages/sqlite/operation_types.hpp>

namespace services::websocket {

using namespace userver;

class WebsocketsHandler final : public server::websocket::WebsocketHandlerBase {
  public:
    // `kName` is used as the component name in static config
    static constexpr std::string_view kName = "websocket-handler";

    WebsocketsHandler(const components::ComponentConfig &config,
                      const components::ComponentContext &context);

    // Component is valid after construction and is able to accept requests
    using WebsocketHandlerBase::WebsocketHandlerBase;

    void Handle(server::websocket::WebSocketConnection &chat,
                server::request::RequestContext &) const override;

  private:
    void send_loop_(server::websocket::WebSocketConnection &chat,
                    engine::Mutex &mutex,
                    std::shared_ptr<ScrabbleGame::GameRoom> game,
                    const int &user_id) const;
    void read_loop_(server::websocket::WebSocketConnection &chat,
                    engine::Mutex &mutex,
                    std::shared_ptr<ScrabbleGame::GameRoom> game,
                    const int &user_id) const;

    bool check_if_user_joined_(const int &game_id, const int &user_id) const;
    /*
     * @returns all info for frontend for current user
     * @param {user_id} id of user
     * @param {game_id} id of game
     */
    void current_game_state_for_user_(server::websocket::Message &message,
                                      const int &user_id,
                                      const int &game_id) const;

    void DefaultInit(formats::json::ValueBuilder &json_for_redis,
                     int64_t gameID) const;

    /*
     * @brief asks database about owner of token
     * @returns user_id {int} or throws {ClientError}
     */
    int define_user_id_from_token_(const std::string &token) const;

    /*
     * @brief Receives first message which describes connection
     * @receives "token" of user
     * @returns {game_id, user_id}
     * @throws ClientError
     */
    std::pair<u_int64_t, int>
    init_user_id_(server::websocket::WebSocketConnection &chat) const;

    template <typename T>
    std::vector<T> ToVector(const formats::json::Value &json) const;

    /// [SQLITE component]

    std::vector<int> sql_ongoing_get_all() const;
    std::string sql_ongoing_new() const;
    int64_t sql_ongoing_delete(std::string_view key) const;

    storages::sqlite::ClientPtr sqlite_client_;

    std::shared_ptr<ScrabbleGame::StorageClient> game_storage_client_;
};

} // namespace services::websocket
