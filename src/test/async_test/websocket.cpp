#include "websocket.hpp"
#include <memory>
#include <optional>
#include <string_view>
#include <userver/crypto/crypto.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/sleep.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/websocket/server.hpp>
#include <userver/storages/sqlite/operation_types.hpp>
#include <userver/storages/sqlite/result_set.hpp>
#include <userver/utils/async.hpp>

#define MAX_SEQ 20

namespace services::websocket {

WebsocketsHandler::WebsocketsHandler(
    const components::ComponentConfig &config,
    const components::ComponentContext &context)
    : server::websocket::WebsocketHandlerBase(config, context),
      notifier_cl_(
          context.FindComponent<NotifierComponent>("notifier").GetStorage()) {};

void WebsocketsHandler::Handle(server::websocket::WebSocketConnection &chat,
                               server::request::RequestContext &) const {
    int my_id = notifier_cl_->get_all().size() + 1;
    notifier_cl_->add_notifier(my_id);
    LOG_DEBUG() << "New Handle id = " << my_id;

    engine::Mutex mutex;
    auto send_loop = engine::AsyncNoSpan(
        [&chat, &mutex, &my_id, this] { SendLoop_(chat, mutex, my_id); });
    ReadLoop_(chat, mutex, my_id);

    return;
}

void WebsocketsHandler::ReadLoop_(server::websocket::WebSocketConnection &chat,
                                  engine::Mutex &mutex,
                                  const int &my_id) const {
    server::websocket::Message msg;
    while (true) {
        int res;
        {
            std::unique_lock<engine::Mutex> lock(mutex);
            res = chat.TryRecv(msg);
        }
        if (!res) {
            engine::SleepFor(std::chrono::milliseconds(100));
            continue;
        }
        if (msg.close_status) {
            chat.Close(*msg.close_status);
            return;
        }
        LOG_DEBUG() << "Message received " << msg.data;
        notifier_cl_->broadcast(msg.data);
    }
}

void WebsocketsHandler::SendLoop_(server::websocket::WebSocketConnection &chat,
                                  engine::Mutex &mutex,
                                  const int &my_id) const {
    LOG_DEBUG() << "StartedAsync";
    std::shared_ptr<NotifierForUser> user_notifier =
        notifier_cl_->get_notifier(my_id);
    if (!user_notifier) {
        LOG_DEBUG() << "User Notifier not found";
        return;
    }
    while (true) {
        LOG_DEBUG() << "Starting cv wait ";
        std::string msg_to_user = user_notifier->pop_wait();
        server::websocket::Message msg;
        msg.data = std::move(msg_to_user);
        std::unique_lock<engine::Mutex> lock(mutex);
        chat.Send(msg);
    }
    return;
}

} // namespace services::websocket
