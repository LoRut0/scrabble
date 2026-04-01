#include "websocket.hpp"
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

#define MAX_SEQ 20

namespace services::websocket {

WebsocketsHandler::WebsocketsHandler(
    const components::ComponentConfig &config,
    const components::ComponentContext &context)
    : server::websocket::WebsocketHandlerBase(config, context) {};

void WebsocketsHandler::Handle(server::websocket::WebSocketConnection &chat,
                               server::request::RequestContext &) const {
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

void WebsocketsHandler::ParseMessage(
    server::websocket::Message &message) const {
    if (!message.is_text) {
        throw server::handlers::ClientError(
            server::handlers::ExternalBody{"No json in request"});
    }
}

} // namespace services::websocket
