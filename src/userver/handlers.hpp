#pragma once
#include <string_view>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/websocket/websocket_handler.hpp>

#include <userver/storages/sqlite/client.hpp>
#include <userver/storages/sqlite/component.hpp>
#include <userver/storages/sqlite/operation_types.hpp>

#include <userver/storages/redis/client.hpp>
#include <userver/storages/redis/component.hpp>

//?
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>

#include <userver/logging/log.hpp>

#include "ScrabbleGame.hpp"

using namespace userver;

namespace services::general {

// for http headers
constexpr static std::string_view origins = "*";

}

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
};

class WebsocketsHandler final : public server::websocket::WebsocketHandlerBase {
public:
    // `kName` is used as the component name in static config
    static constexpr std::string_view kName = "websocket-handler";

    WebsocketsHandler(const components::ComponentConfig& config, const components::ComponentContext& context);

    // Component is valid after construction and is able to accept requests
    using WebsocketHandlerBase::WebsocketHandlerBase;

    void Handle(server::websocket::WebSocketConnection& chat, server::request::RequestContext&) const override;

private:
    bool check_if_user_joined_(const int& game_id, const int& user_id) const;
    /*
     * @returns all info for frontend for current user
     * @param {user_id} id of user
     * @param {game_id} id of game
     */
    void current_game_state_for_user_(server::websocket::Message& message, const int& user_id, const int& game_id) const;
    /*
     * @brief Parses message from websocket, calls functions
     *
     * @throws {server::handlers::ClientError} if format of msg is bad
     */
    void ParseMessage(server::websocket::Message& message) const;
    void action_place(server::websocket::Message& message, const formats::json::Value& json_msg, const int& user_id) const;
    void action_start(server::websocket::Message& message, const formats::json::Value& json_msg, const int& user_id) const;
    void action_change(server::websocket::Message& message, const formats::json::Value& json_msg, const int& user_id) const;
    void action_end(server::websocket::Message& message, const formats::json::Value& json_msg, const int& user_id) const;
    void action_pass(server::websocket::Message& message, const formats::json::Value& json_msg, const int& user_id) const;
    void action_submit(server::websocket::Message& message, const formats::json::Value& json_msg, const int& user_id) const;

    void DefaultInit(formats::json::ValueBuilder& json_for_redis, int64_t gameID) const;

    int define_user_id_from_token_(const std::string& token) const;

    template <typename T>
    std::vector<T> ToVector(const formats::json::Value& json) const;

    /// [SQLITE component]

    std::vector<int> sqlGetAllOngoing() const;
    std::string sqlNewOngoing() const;
    int64_t sqlDeleteValue(std::string_view key) const;

    storages::sqlite::ClientPtr sqlite_client_;

    std::shared_ptr<ScrabbleGame::Storage::Client> game_storage_client_;
};

} // namespace services::websocket

namespace services::http {

bool VerifyPassword(const std::string& password,
    const std::string& stored_hash);
std::string HashPassword(const std::string& password);

class RegistrationHandler final : public server::handlers::HttpHandlerBase {
public:
    // `kName` is used as the component name in static config
    static constexpr std::string_view kName = "http-registration_handler";

    // Component is valid after construction and is able to accept requests
    using HttpHandlerBase::HttpHandlerBase;

    RegistrationHandler(const components::ComponentConfig& config, const components::ComponentContext& context);

    std::string HandleRequest(server::http::HttpRequest& request, server::request::RequestContext&) const override;

private:
    enum class RegistrationStatus {
        OK,
        InternalFailure,
        UserExists,
        NicknameExists
    };
    storages::sqlite::ClientPtr sqlite_client_;

    bool nick_check_(const std::string& nick) const;
    bool email_check_(const std::string& email) const;

    RegistrationStatus register_new_user_(const std::string& email, const std::string& passwd, const std::string& nick) const;
};

struct SessionTokens {
    std::string raw_token;
    std::string jti;
};

SessionTokens GenerateSessionTokens();

class LoginHandler final : public server::handlers::HttpHandlerBase {
public:
    // `kName` is used as the component name in static config
    static constexpr std::string_view kName = "http-login_handler";

    // Component is valid after construction and is able to accept requests
    using HttpHandlerBase::HttpHandlerBase;

    LoginHandler(const components::ComponentConfig& config, const components::ComponentContext& context);

    /*
     * @returns {raw_token_for_client}
     */
    std::string HandleRequest(server::http::HttpRequest& request, server::request::RequestContext&) const override;

private:
    enum class LoginStatus {
        OK,
        InternalFailure,
        UserNotExists,
        PasswdIncorrect
    };
    struct UserDBInfo {
        std::string passwd_hash;
        int user_id;
    };
    storages::sqlite::ClientPtr sqlite_client_;

    LoginStatus login_checker_(const std::string& login, const std::string& passwd, int& user_id) const;
    /*
     * @brief defines if user submited email or nickname
     *
     * @retval {1} Email
     * @retval {0} Nickname
     */
    bool define_login_type_(const std::string& login) const;

    LoginStatus get_token_for_client_(const int& user_id, std::string& raw_token) const;
};

class GameHandler final : public server::handlers::HttpHandlerBase {
public:
    // `kName` is used as the component name in static config
    static constexpr std::string_view kName = "http-game_handler";

    // Component is valid after construction and is able to accept requests
    using HttpHandlerBase::HttpHandlerBase;

    GameHandler(const components::ComponentConfig& config, const components::ComponentContext& context);

    std::string HandleRequest(server::http::HttpRequest& request, server::request::RequestContext&) const override;

private:
    enum class GameAction {
        join,
        create,
        end,
        start,
        list
    };
    GameAction from_string_GameAction(const std::string& str) const;

    /*
     * @brief creates new game with current user as host
     * @param {"token"} user token
     * @returns new game id in body
     */
    std::string create_game_(server::http::HttpRequest& request, const int& user_id) const;
    /*
     * @brief adds user to game players
     * @param {"token"} user token
     * @param {"game_id"} game to join
     * @returns {"game_id"} of joined game or {} if error
     */
    enum class JoinGameResult {
        inserted,
        joined,
        error
    };
    JoinGameResult from_string_JoinGameResult(const std::string& str) const;
    std::string join_game_(server::http::HttpRequest& request, const int& user_id) const;
    bool check_if_already_joined_(const int& game_id, const int& user_id) const;
    /*
     * @brief starts game that user is hosting
     * @param {"token"} user token
     * @param {"game_id"} game that is hosted by user
     * @returns game id in body
     */
    std::string start_game_(server::http::HttpRequest& request, const int& user_id) const;
    /*
     * @brief game with current user as host
     * @param {"token"} user token
     * @param {"game_id"}
     * @returns deleted {game_id} or {} if no game were deleted
     */
    std::string end_game_(server::http::HttpRequest& request, const int& user_id) const;
    struct GameInfo {
        int game_id;
        std::string host_user_name;
        int num_of_users;
    };
    /*
     * @brief returns list of all games
     * @param {"token"} user token
     * @returns json with list of games
     */
    std::string list_games_(server::http::HttpRequest& request) const;
    /*
     * @brief helper function for list_games_
     * @returns vector with info about game servers
     */
    std::vector<GameInfo> list_games_helper_() const;

    int define_user_id_from_token_(const std::string& token) const;

    storages::sqlite::ClientPtr sqlite_client_;

    std::shared_ptr<ScrabbleGame::Storage::Client> game_storage_client_;

    int redis_post_json_(const std::string& key, const formats::json::Value& json_to_post) const;
    int redis_post_json_(const std::string& key, const std::string& json_string) const;
    size_t redisDelete(const std::string& key) const;
};

} // namespace services::http
