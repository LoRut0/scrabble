#pragma once

#include <map>
#include <memory>
#include <queue>
#include <userver/components/component_base.hpp>
#include <userver/engine/mutex.hpp>
#include <userver/engine/shared_mutex.hpp>
#include <vector>

using namespace userver;

class NotifierForUser {
  private:
    std::queue<std::string> send_queue_;
    engine::Mutex mutex_;
    engine::ConditionVariable cv_;

  public:
    const std::string kUserName;

    const std::string pop_wait();
    void push_message(const std::string &msg);

    NotifierForUser(const std::string &UserName);
};

class NotifierComponent final : public components::ComponentBase {
  public:
    class NotifierClient;
    // name of your component to refer in static config
    static constexpr std::string_view kName = "notifier";

    NotifierComponent(const components::ComponentConfig &config,
                      const components::ComponentContext &context);

    std::shared_ptr<NotifierClient> GetStorage();

  private:
    std::shared_ptr<NotifierClient> notifier_;
};

class NotifierComponent::NotifierClient final {
  public:
    NotifierClient() = default;
    ~NotifierClient() = default;

    void broadcast(const std::string &msg);

    void add_notifier(const int &id);

    /*
     * @brief returns shared_ptr for notifier
     * @paramm {id} id of notifier
     * @retval {nullptr} if notifier was not found
     */
    std::shared_ptr<NotifierForUser> get_notifier(const int &id);

    std::vector<std::pair<int, std::string>> get_all();

  private:
    engine::SharedMutex shared_mutex_;
    std::unordered_map<int, std::shared_ptr<NotifierForUser>> umap;
};
