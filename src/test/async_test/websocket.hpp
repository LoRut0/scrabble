#include <userver/server/websocket/websocket_handler.hpp>

#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>

#include <userver/components/minimal_server_component_list.hpp>

#include <userver/logging/log.hpp>
#include <userver/server/handlers/exceptions.hpp>

#include "Notifier.hpp"

using namespace userver;

namespace services::websocket {

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
    /*
     * @brief Parses message from websocket, calls functions
     *
     * @throws {server::handlers::ClientError} if format of msg is bad
     */
    void ParseMessage(server::websocket::Message &message) const;
};

} // namespace services::websocket
