#pragma once
#include "game/Storage.hpp"
#include <userver/server/websocket/websocket_handler.hpp>
#include <userver/storages/sqlite/client.hpp>
#include <userver/storages/sqlite/component.hpp>
#include <userver/storages/sqlite/operation_types.hpp>

namespace services::websocket {

namespace Actions {

enum class PlayerAction {
    // start a game
    start,
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

inline PlayerAction from_string(std::string str);
}; // namespace Actions

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
    bool check_if_user_joined_(const int &game_id, const int &user_id) const;
    /*
     * @returns all info for frontend for current user
     * @param {user_id} id of user
     * @param {game_id} id of game
     */
    void current_game_state_for_user_(server::websocket::Message &message,
                                      const int &user_id,
                                      const int &game_id) const;
    /*
     * @brief Parses message from websocket, calls functions
     *
     * @throws {server::handlers::ClientError} if format of msg is bad
     */
    void ParseMessage(server::websocket::Message &message) const;
    void action_place(server::websocket::Message &message,
                      const formats::json::Value &json_msg,
                      const int &user_id) const;
    void action_start(server::websocket::Message &message,
                      const formats::json::Value &json_msg,
                      const int &user_id) const;
    void action_change(server::websocket::Message &message,
                       const formats::json::Value &json_msg,
                       const int &user_id) const;
    void action_end(server::websocket::Message &message,
                    const formats::json::Value &json_msg,
                    const int &user_id) const;
    void action_pass(server::websocket::Message &message,
                     const formats::json::Value &json_msg,
                     const int &user_id) const;
    void action_submit(server::websocket::Message &message,
                       const formats::json::Value &json_msg,
                       const int &user_id) const;

    void DefaultInit(formats::json::ValueBuilder &json_for_redis,
                     int64_t gameID) const;

    int define_user_id_from_token_(const std::string &token) const;

    template <typename T>
    std::vector<T> ToVector(const formats::json::Value &json) const;

    /// [SQLITE component]

    std::vector<int> sql_ongoing_get_all() const;
    std::string sql_ongoing_new() const;
    int64_t sql_ongoing_delete(std::string_view key) const;

    storages::sqlite::ClientPtr sqlite_client_;

    std::shared_ptr<ScrabbleGame::Storage::Client> game_storage_client_;
};

} // namespace services::websocket
