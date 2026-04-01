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

namespace services::http {

class GameHandler final : public server::handlers::HttpHandlerBase {
public:
    // `kName` is used as the component name in static config
    static constexpr std::string_view kName = "http-game_handler";

    // Component is valid after construction and is able to accept requests
    using HttpHandlerBase::HttpHandlerBase;

    GameHandler(const components::ComponentConfig& config, const components::ComponentContext& context);

    std::string HandleRequest(server::http::HttpRequest& request, server::request::RequestContext&) const override;

private:
    std::string action_func_(const std::string& action, const int& id) const;
    formats::json::Value construct_json_(std::vector<std::array<int, 2>>& vec) const;

    std::shared_ptr<ScrabbleGame::Client> game_storage_client_;
};

} // namespace services::http
