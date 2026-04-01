#include "websocket.hpp"
#include <userver/clients/dns/component.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>
// #include <userver/formats/json/value.hpp>
// #include <userver/formats/json.hpp>

using namespace userver;

/// [Websocket service sample - main]
int main(int argc, char *argv[]) {
    const auto component_list =
        components::MinimalServerComponentList()
            .Append<services::websocket::WebsocketsHandler>()
            .Append<components::TestsuiteSupport>()
            .Append<clients::dns::Component>();
    return utils::DaemonMain(argc, argv, component_list);
}

/// [Websocket service sample - main]
