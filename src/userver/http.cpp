#include "ScrabbleGame.hpp"
#include "handlers.hpp"
#include <cstddef>
#include <memory>
#include <optional>
#include <regex>
#include <sodium.h>
#include <stdexcept>
#include <string>
#include <userver/crypto/base64.hpp>
#include <userver/crypto/crypto.hpp>
#include <userver/crypto/hash.hpp>
#include <userver/crypto/random.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/storages/sqlite/execution_result.hpp>
#include <userver/storages/sqlite/operation_types.hpp>
#include <userver/storages/sqlite/options.hpp>
#include <userver/storages/sqlite/result_set.hpp>
#include <userver/storages/sqlite/transaction.hpp>

namespace services::general {

} // namespace services::general

namespace services::http {

/************************
 * REGISTRATION_HANDLER *
 ************************/

/*
 * @brief registering new user in users table
 * @param {username}
 * @param {email}
 */
inline constexpr std::string_view RegisterQueryUsersInsert = R"~(
    INSERT INTO users(username, display_name, email)
    VALUES($1, $1, $2);
)~";
/*
 * @brief registering new user in user in user_credentials, takes last_insert_rowid() as user_id
 * @param {passwd_hash}
 */
inline constexpr std::string_view RegisterQueryCredentialsInsert = R"~(
    INSERT INTO user_credentials(user_id, password_hash)
    VALUES(last_insert_rowid(), $1);
)~";

bool VerifyPassword(const std::string& password,
    const std::string& stored_hash)
{
    return crypto_pwhash_str_verify(stored_hash.c_str(), password.c_str(),
               password.size())
        == 0;
}

std::string HashPassword(const std::string& password)
{
    char hash[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(hash, password.c_str(), password.size(),
            crypto_pwhash_OPSLIMIT_MODERATE,
            crypto_pwhash_MEMLIMIT_MODERATE)
        != 0) {
        throw std::runtime_error("Out of Memory");
    }

    return std::string(hash);
}

RegistrationHandler::RegistrationStatus RegistrationHandler::register_new_user_(const std::string& email, const std::string& passwd, const std::string& nick) const
{
    // TODO: check for user existence
    std::string hashed_passwd = HashPassword(passwd);
    storages::sqlite::Transaction transaction = sqlite_client_->Begin(userver::v2_::storages::sqlite::OperationType::kReadWrite, storages::sqlite::settings::TransactionOptions());
    transaction.Execute(RegisterQueryUsersInsert.data(), nick, email);
    storages::sqlite::ExecutionResult result = transaction.Execute(RegisterQueryCredentialsInsert.data(), hashed_passwd).AsExecutionResult();
    if (result.rows_affected != 0) {
        transaction.Commit();
        return RegistrationStatus::OK;
    }
    transaction.Rollback();
    return RegistrationStatus::InternalFailure;
};

bool RegistrationHandler::nick_check_(const std::string& nick) const
{
    // 3–32 символа: буквы, цифры, _, -, .
    static const std::regex re(R"(^[A-Za-z0-9_.-]{3,32}$)");

    if (!std::regex_match(nick, re))
        return false;

    // нельзя начинать или заканчивать точкой
    if (nick.front() == '.' || nick.back() == '.')
        return false;

    // нельзя иметь две точки подряд
    if (nick.find("..") != std::string::npos)
        return false;

    return true;
};

bool RegistrationHandler::email_check_(const std::string& email) const
{
    if (email.size() > 254)
        return false;

    static const std::regex re(
        R"(^[A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]+@[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)+$)");

    if (!std::regex_match(email, re))
        return false;

    if (email.find("..") != std::string::npos)
        return false;

    return true;
};

std::string RegistrationHandler::HandleRequest(server::http::HttpRequest& request, server::request::RequestContext&) const
{
    request.GetHttpResponse().SetHeader(std::string { "Access-Control-Allow-Origin" }, services::general::origins.data());

    formats::json::Value body_json { formats::json::FromString(request.RequestBody()) };
    const std::string email = body_json["email"].As<std::string>();
    const std::string passwd = body_json["passwd"].As<std::string>();
    const std::string nick = body_json["nick"].As<std::string>();
    if (!email_check_(email)
        || !nick_check_(nick)) {
        throw server::handlers::ClientError { server::handlers::ExternalBody { "invalid email or nick" } };
    }

    switch (register_new_user_(email, passwd, nick)) {
    case RegistrationStatus::OK:
        request.SetResponseStatus(server::http::HttpStatus::OK);
        return nick;
    case RegistrationStatus::NicknameExists:
        request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
        return { "nickname already exists" };
    case RegistrationStatus::UserExists:
        request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
        return { "user already exists" };
    case RegistrationStatus::InternalFailure:
        request.SetResponseStatus(server::http::HttpStatus::InternalServerError);
        return { "internal server error :)" };
    }

    return { "how you got there? RegistrationHandler::HandleRequest" };
};

RegistrationHandler::RegistrationHandler(const components::ComponentConfig& config, const components::ComponentContext& context)
    : HttpHandlerBase(config, context)
    , sqlite_client_(context.FindComponent<components::SQLite>("sqlitedb").GetClient()) { };

/*****************
 * LOGIN_HANDLER *
 *****************/

SessionTokens GenerateSessionTokens()
{
    constexpr size_t kTokenSize = 32;
    std::string random_bytes = userver::crypto::GenerateRandomBlock(kTokenSize);

    std::string raw_token = userver::crypto::base64::Base64UrlEncode(random_bytes);
    std::string jti = userver::crypto::hash::Sha256(raw_token);

    return { raw_token, jti };
}

LoginHandler::LoginHandler(const components::ComponentConfig& config, const components::ComponentContext& context)
    : HttpHandlerBase(config, context)
    , sqlite_client_(context.FindComponent<components::SQLite>("sqlitedb").GetClient()) { };

/*
 * @brief returns Auth token if exists
 * @param {$1} jti token
 * @retval {jti}
 */
inline constexpr std::string_view CheckAuthTokenByJti = R"~(
    SELECT tok.jti
    FROM auth_tokens tok
    WHERE tok.jti = $1
        AND tok.expires_at > CURRENT_TIMESTAMP
        AND tok.revoked_at IS NULL
)~";
/*
 * @brief returns Auth token if exists
 * @param {$1} jti token
 * @retval {user_id} INTEGER
 */
inline constexpr std::string_view GetUidByJti = R"~(
    SELECT tok.user_id
    FROM auth_tokens tok
    WHERE tok.user_id = $1
)~";
/*
 * @brief adds auth token to table with expire time +30 days
 * @param {$1} jti token
 * @param {$2} user_id
 */
inline constexpr std::string_view AddAuthToken = R"~(
    INSERT INTO auth_tokens(jti, user_id, expires_at)
    VALUES ($1, $2, DATETIME(CURRENT_TIMESTAMP, '+30 days'))
)~";
/*
 * @brief updates expire date of token
 * @param {$1} jti token
 */
inline constexpr std::string_view UpdateAuthToken = R"~(
    UPDATE auth_tokens
    SET expires_at = DATETIME(CURRENT_TIMESTAMP, '+30 days')
    WHERE jti = $1
        AND revoked_at IS NULL;
)~";
/*
 * @brief returns passwd_hash of user from nickname
 * @param {$1} username
 * @retval {password_hash}
 */
inline constexpr std::string_view LoginQueryNick = R"~(
    SELECT 
        c.password_hash,
        c.user_id
    FROM users u
    JOIN user_credentials c ON u.id = c.user_id
    WHERE u.username = $1
)~";
/*
 * @brief returns passwd_hash of user from email
 * @param {$1} email
 * @retval {password_hash}
 */
inline constexpr std::string_view LoginQueryEmail = R"~(
    SELECT 
        c.password_hash, 
        c.user_id
    FROM users u
    JOIN user_credentials c ON u.id = c.user_id
    WHERE u.email = $1
)~";

bool LoginHandler::define_login_type_(const std::string& login) const
{
    // TODO: to think of something better?
    if (login.find('@') != std::string::npos)
        return 0;
    return 1;
}

LoginHandler::LoginStatus LoginHandler::login_checker_(const std::string& login, const std::string& passwd, int& user_id) const
{
    // storages::sqlite::ResultSet result = (define_login_type_(login))
    //     ? sqlite_client_->Execute(
    //           storages::sqlite::OperationType::kReadOnly,
    //           LoginQueryEmail.data(),
    //           login)
    //     : sqlite_client_->Execute(
    //           storages::sqlite::OperationType::kReadOnly,
    //           LoginQueryNick.data(),
    //           login);
    storages::sqlite::ResultSet result = sqlite_client_->Execute(
        storages::sqlite::OperationType::kReadOnly,
        LoginQueryNick.data(),
        login);
    std::optional<UserDBInfo> user_info = std::move(result).AsOptionalSingleRow<UserDBInfo>();
    if (!user_info)
        return LoginStatus::UserNotExists;
    bool login_success = VerifyPassword(passwd, user_info->passwd_hash);
    if (!login_success)
        return LoginStatus::PasswdIncorrect;
    user_id = user_info->user_id;
    return LoginStatus::OK;
};

LoginHandler::LoginStatus LoginHandler::get_token_for_client_(const int& user_id, std::string& raw_token) const
{
    SessionTokens tokens = GenerateSessionTokens();

    storages::sqlite::ExecutionResult result = sqlite_client_->Execute(storages::sqlite::OperationType::kReadWrite, AddAuthToken.data(), tokens.jti, user_id).AsExecutionResult();

    if (result.rows_affected == 0)
        return LoginStatus::InternalFailure;
    raw_token = std::move(tokens.raw_token);

    return LoginStatus::OK;
}

std::string LoginHandler::HandleRequest(server::http::HttpRequest& request, server::request::RequestContext&) const
{
    request.GetHttpResponse().SetHeader(std::string { "Access-Control-Allow-Origin" }, services::general::origins.data());

    formats::json::Value body_json { formats::json::FromString(request.RequestBody()) };
    const std::string login = body_json["login"].As<std::string>();
    const std::string passwd = body_json["passwd"].As<std::string>();

    int user_id;
    LoginStatus login_status = login_checker_(login, passwd, user_id);

    SessionTokens tokens;

    std::string raw_token_for_client;
    if (login_status == LoginStatus::OK) {
        login_status = get_token_for_client_(user_id, raw_token_for_client);
    }

    switch (login_status) {
    case LoginStatus::UserNotExists:
        request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
        return { "UserNotExists" };
    case LoginStatus::PasswdIncorrect:
        request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
        return { "PasswdIncorrect" };
    case LoginStatus::InternalFailure:
        request.SetResponseStatus(server::http::HttpStatus::InternalServerError);
        return { "InternalFailure" };
    case LoginStatus::OK:
        request.SetResponseStatus(server::http::HttpStatus::OK);
        break;
    }

    return raw_token_for_client;
}

/****************
 * GAME_HANDLER *
 ****************/

/*
 * @brief inserts new game in games
 * @param {user_id} of host user
 * @retval {id} new game id
 */
inline constexpr std::string_view SqlInsertNewGame = R"~(
    INSERT INTO games(host_user_id)
    VALUES ($1)
    RETURNING id
)~";
/*
 * @brief deletes game by id
 * @param {id} id of game
 */
inline constexpr std::string_view SqlDeleteGame = R"~(
    DELETE FROM games
        WHERE id = $1
)~";
/*
 * @brief deletes game if user is host
 * @param {id} game id
 * @param {host_user_id} user id
 */
inline constexpr std::string_view SqlDeleteGameWithHostCheck = R"~(
    DELETE FROM games
    WHERE id = $1
        AND host_user_id = $2;
)~";
/*
 * @brief inserts new user to game_users or tries to join game
 * @param {game_id}
 * @param {user_id}
 * @retval {"inserted"} if inserted into game_users
 * @retval {"joined"} if player already was inserted into game_users
 * @retval {"error"} if error
 */
inline constexpr std::string_view SqlInsertNewUserInGame = R"~(
    INSERT INTO game_users(game_id, user_id)
    VALUES ($1, $2);
)~";
/*
 * @brief inserts new user to game_users or tries to join game
 * @param {game_id}
 * @param {user_id}
 * @retval {"inserted"} if inserted into game_users
 * @retval {"joined"} if player already was inserted into game_users
 * @retval {"error"} if error
 */
inline constexpr std::string_view SqlCheckUserStatusInGame = R"~(
    INSERT OR IGNORE INTO game_users(game_id, user_id)
    VALUES ($1, $2);
)~";
/*
 * @brief reeturns all games ids
 * @retval {id} game_id
 * @retval {host_user_id} id of host_user
 */
inline constexpr std::string_view SqlGetAllGames = R"~(
    SELECT id, host_user_id
    FROM games
)~";
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
 * @brief checks if host is user_id
 * @param {game_id}
 * @param {user_id} user if of host
 * @retval {1} 1 row if user is host else 0 rows
 */
inline constexpr std::string_view SqlCheckHost = R"~(
    SELECT 1 
    FROM games
        WHERE id = $1 AND host_user_id = $2
)~";
/*
 * @brief returns all players of game
 * @param {game_id}
 * @returns {user_id} all players user ids
 */
inline constexpr std::string_view SqlReturnAllGamePlayers = R"~(
    SELECT user_id
    FROM (
        SELECT host_user_id AS user_id
        FROM games
        WHERE id = $1

        UNION

        SELECT user_id
        FROM game_users
        WHERE game_id = $1
    )
)~";

int GameHandler::define_user_id_from_token_(const std::string& token) const
{
    const std::string jti = userver::crypto::hash::Sha256(token);
    storages::sqlite::ResultSet result = sqlite_client_->Execute(storages::sqlite::OperationType::kReadOnly, SqlUserIdByJti.data(), jti);
    std::optional<int> user_id = std::move(result).AsOptionalSingleField<int>();
    if (!user_id)
        throw server::handlers::ClientError(server::handlers::ExternalBody { "invalid token from user" });
    return user_id.value();
}

GameHandler::GameAction GameHandler::from_string_GameAction(const std::string& str) const
{
    if (str == "join") {
        return GameAction::join;
    } else if (str == "start") {
        return GameAction::start;
    } else if (str == "end") {
        return GameAction::end;
    } else if (str == "create") {
        return GameAction::create;
    } else if (str == "list") {
        return GameAction::list;
    }
    throw server::handlers::ClientError(server::handlers::ExternalBody { "invalid gameAction" });
}

GameHandler::GameHandler(const components::ComponentConfig& config, const components::ComponentContext& context)
    : HttpHandlerBase(config, context)
    , sqlite_client_(context.FindComponent<components::SQLite>("sqlitedb").GetClient())
    , game_storage_client_(context.FindComponent<ScrabbleGame::Storage::StorageComponent>("game_storage").GetStorage()) { };

std::string GameHandler::create_game_(server::http::HttpRequest& request, const int& user_id) const
{
    // TODO: check if current user already hosts a game and end it
    storages::sqlite::ResultSet result = sqlite_client_->Execute(storages::sqlite::OperationType::kReadWrite, SqlInsertNewGame.data(), user_id);
    const int new_game_id = std::move(result).AsSingleField<int>();

    request.SetResponseStatus(server::http::HttpStatus::OK);
    return std::to_string(new_game_id);
}

GameHandler::JoinGameResult GameHandler::from_string_JoinGameResult(const std::string& str) const
{
    if (str == "joined") {
        return JoinGameResult::joined;
    } else if (str == "inserted") {
        return JoinGameResult::inserted;
    } else if (str == "error") {
        return JoinGameResult::error;
    }
    return JoinGameResult::error;
}

/*
 * @brief returns one field with 1, if user already joined game
 * @param {game_id}
 * @param {user_id}
 */
inline constexpr std::string_view SqlCheckIfUserAlreadyJoined = R"~(
    SELECT 1 FROM game_users
    WHERE game_id = $1 AND user_id = $2
)~";

/*
 * @brief checks if user has already joined the game
 * @retval {0} not joined
 * @retval {1} joined
 */
bool GameHandler::check_if_already_joined_(const int& game_id, const int& user_id) const
{
    storages::sqlite::ResultSet result = sqlite_client_->Execute(userver::v2_::storages::sqlite::OperationType::kReadWrite, SqlCheckIfUserAlreadyJoined.data(), game_id, user_id);
    std::optional<int> field = std::move(result).AsOptionalSingleField<int>();
    if (!field)
        return 0;
    return 1;
}

std::string GameHandler::join_game_(server::http::HttpRequest& request, const int& user_id) const
{
    // user joins game, all of his other games should end if he hosts any
    // if he already joined game then he just receives game_id
    // otherwise he appended to game_users, then receives game_id
    formats::json::Value json = userver::formats::json::FromString(request.RequestBody());
    const int game_id = json["game_id"].As<int>();

    if (check_if_already_joined_(game_id, user_id)) {
        request.SetResponseStatus(server::http::HttpStatus::OK);
        return std::to_string(game_id);
    }

    storages::sqlite::ResultSet result = sqlite_client_->Execute(userver::v2_::storages::sqlite::OperationType::kReadWrite, SqlInsertNewUserInGame.data(), game_id, user_id);
    // TODO: make check for max players, make all other games of player end
    const JoinGameResult join_game_result = JoinGameResult::joined;
    // const JoinGameResult join_game_result = from_string_JoinGameResult(std::move(result).AsSingleField<std::string>());

    switch (join_game_result) {
    case JoinGameResult::joined:
        request.SetResponseStatus(server::http::HttpStatus::OK);
        return std::to_string(game_id);
    case JoinGameResult::inserted:
        request.SetResponseStatus(server::http::HttpStatus::OK);
        return std::to_string(game_id);
    case JoinGameResult::error:
        request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
        return { "Error while joining" };
    }

    request.SetResponseStatus(server::http::HttpStatus::kInternalServerError);
    return {};
}

// formats::json::ValueBuilder GameHandler::json_default_init_(const int& game_id) const
// {
//
// }

std::string GameHandler::start_game_(server::http::HttpRequest& request, const int& user_id) const
{
    LOG_DEBUG() << "Starting new Game";
    formats::json::Value json = userver::formats::json::FromString(request.RequestBody());
    const int game_id = json["game_id"].As<int>();

    // TODO: check if user is host

    // take current game
    // post it to redis_client_
    //
    // TODO: make it ongoing in sql
    //
    // return ok if all is good

    std::shared_ptr<ScrabbleGame::ScrabbleGame> game_ptr = std::make_shared<ScrabbleGame::ScrabbleGame>(
        [](const std::u32string&) { return 1; });
    game_storage_client_->add_game(game_id, game_ptr);

    request.SetResponseStatus(server::http::HttpStatus::OK);
    return std::to_string(game_id);
}

std::string GameHandler::end_game_(server::http::HttpRequest& request, const int& user_id) const
{
    formats::json::Value json = userver::formats::json::FromString(request.RequestBody());
    const int game_id = json["game_id"].As<int>();

    storages::sqlite::ExecutionResult result = sqlite_client_->Execute(userver::v2_::storages::sqlite::OperationType::kReadWrite, SqlDeleteGameWithHostCheck.data(), game_id, user_id).AsExecutionResult();

    if (result.rows_affected == 0) {
        request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
        return {};
    }
    request.SetResponseStatus(server::http::HttpStatus::OK);
    return std::to_string(game_id);
}

/*
 * @brief returns info about games for list of games
 * @retval {game_id}
 * @retval {host_user_name}
 * @retval {num_of_users}
 */
inline constexpr std::string_view sqlReturnGamesInfo = R"~(
    WITH game_players_count AS (
        SELECT 
            g.id AS game_id,
            COUNT(gu.user_id) AS player_count
        FROM games g
        LEFT JOIN game_users gu ON gu.game_id = g.id
        GROUP BY g.id
    )
    SELECT
        g.id as game_id,
        u.display_name as host_user_name,
        gpc.player_count as num_of_users
    FROM games g
    JOIN users u ON g.host_user_id = u.id
    JOIN game_players_count gpc ON g.id = gpc.game_id
)~";

std::vector<GameHandler::GameInfo> GameHandler::list_games_helper_() const
{
    storages::sqlite::ResultSet result = sqlite_client_->Execute(userver::v2_::storages::sqlite::OperationType::kReadOnly, sqlReturnGamesInfo.data());
    std::vector<GameInfo> list_game_info = std::move(result).AsVector<GameInfo>();
    return list_game_info;
}

std::string GameHandler::list_games_(server::http::HttpRequest& request) const
{
    std::vector<GameInfo> list_games_info = list_games_helper_();
    formats::json::ValueBuilder json_vb;
    json_vb["game_info_list"].Resize(list_games_info.size());

    for (size_t i = 0; i < list_games_info.size(); ++i) {
        const GameInfo& game_info = list_games_info[i];
        formats::json::ValueBuilder vb_game_info;
        vb_game_info["game_id"] = game_info.game_id;
        vb_game_info["host_user_name"] = game_info.host_user_name;
        vb_game_info["num_of_users"] = game_info.num_of_users;
        json_vb["game_info_list"][i] = std::move(vb_game_info);
    }

    request.SetResponseStatus(server::http::HttpStatus::OK);
    return formats::json::ToStableString(json_vb.ExtractValue());
}

std::string GameHandler::HandleRequest(server::http::HttpRequest& request, server::request::RequestContext&) const
{
    request.GetHttpResponse().SetHeader(std::string { "Access-Control-Allow-Origin" }, services::general::origins.data());

    formats::json::Value json = userver::formats::json::FromString(request.RequestBody());
    const std::string action = json["action"].As<std::string>();
    const int user_id = define_user_id_from_token_(json["token"].As<std::string>());
    // TODO: change throw to kBadRequest inside define_user_id_from_token_

    LOG_DEBUG() << "action = " << action;
    LOG_DEBUG() << "user_id = " << user_id;

    const GameAction enumAction = from_string_GameAction(action);
    switch (enumAction) {
    case GameAction::create:
        return create_game_(request, user_id);
        break;
    case GameAction::join:
        return join_game_(request, user_id);
        break;
    case GameAction::start:
        return start_game_(request, user_id);
        break;
    case GameAction::end:
        return end_game_(request, user_id);
        break;
    case GameAction::list:
        return list_games_(request);
        break;
    }
    throw server::handlers::ClientError(server::handlers::ExternalBody { "invalid gameAction" });
}

} // namespace services::http
