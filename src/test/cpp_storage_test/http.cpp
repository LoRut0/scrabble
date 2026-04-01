#include "ScrabbleGame.hpp"
#include "handlers.hpp"
#include <memory>
#include <sodium.h>
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

namespace services::http {

/****************
 * GAME_HANDLER *
 ****************/

GameHandler::GameHandler(const components::ComponentConfig& config, const components::ComponentContext& context)
    : HttpHandlerBase(config, context)
    , game_storage_client_(context.FindComponent<ScrabbleGame::StorageComponent>("game_storage").GetStorage()) { };

std::string GameHandler::HandleRequest(server::http::HttpRequest& request, server::request::RequestContext&) const
{
    formats::json::Value json = userver::formats::json::FromString(request.RequestBody());
    const std::string action = json["action"].As<std::string>();
    const int id = json["id"].As<int>();
    // TODO: change throw to kBadRequest inside define_user_id_from_token_

    LOG_DEBUG() << "action = " << action;
    LOG_DEBUG() << "id = " << id;

    return action_func_(action, id);
}

formats::json::Value GameHandler::construct_json_(std::vector<std::array<int, 2>>& vec) const
{
    formats::json::ValueBuilder json_vb;
    json_vb.Resize(vec.size());
    for (size_t i = 0; i < vec.size(); ++i) {
        std::array<int, 2>& cur_game = vec[i]; // 0 - id, 1 - value
        formats::json::ValueBuilder temp;
        temp["id"] = cur_game[0];
        temp["value"] = cur_game[1];
        json_vb[i] = std::move(temp);
    }
    return json_vb.ExtractValue();
}

std::string GameHandler::action_func_(const std::string& action, const int& id) const
{
    if (action == "increment") {
        LOG_INFO()<<"Client requested to increment one counter";
        std::shared_ptr<ScrabbleGame::ScrabbleGame> game = game_storage_client_->get_game(id);
        if (!game)
            return R"({"error":"game with given id does not exist")";
        game->increment();
        return "";
    }
    if (action == "get") {
        LOG_INFO()<<"Client requested one game";
        std::shared_ptr<ScrabbleGame::ScrabbleGame> game = game_storage_client_->get_game(id);
        if (!game)
            return R"({"error":"game with given id does not exist")";
        int temp = game->get();
        return std::to_string(temp);
    }
    if (action == "new") {
        LOG_INFO()<<"Client tries to make new game";
        auto new_game = std::make_shared<ScrabbleGame::ScrabbleGame>(0);
        game_storage_client_->add_game(id, new_game);
        return std::to_string(id);
    }
    if (action == "all") {
        LOG_INFO()<<"Client requested list of all games";
        std::vector<std::array<int, 2>> vec = game_storage_client_->get_all();
        formats::json::Value json_vl = construct_json_(vec);
        return formats::json::ToStableString(json_vl);
    }
    return "error: no viable action";
}

} // namespace services::http
