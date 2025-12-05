#include "ScrabbleGame.hpp"
#include "handlers.hpp"
#include <userver/clients/dns/component.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>
// #include <userver/formats/json/value.hpp>
// #include <userver/formats/json.hpp>

using namespace userver;

/// [Websocket service sample - main]
int main(int argc, char* argv[])
{
    const auto component_list = components::MinimalServerComponentList()
                                    .Append<services::websocket::WebsocketsHandler>()
                                    .Append<services::http::GameHandler>()
                                    .Append<services::http::LoginHandler>()
                                    .Append<services::http::RegistrationHandler>()
                                    .Append<ScrabbleGame::Storage::StorageComponent>()
                                    .Append<components::DefaultSecdistProvider>()
                                    .Append<components::Secdist>()
                                    .Append<components::Redis>("redisdb")
                                    .Append<components::SQLite>("sqlitedb")
                                    .Append<components::TestsuiteSupport>()
                                    .Append<clients::dns::Component>();
    return utils::DaemonMain(argc, argv, component_list);
}

/// [Websocket service sample - main]
