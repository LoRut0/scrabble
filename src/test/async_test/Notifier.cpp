#include "Notifier.hpp"
#include <memory>

NotifierForUser::NotifierForUser(const std::string &UserName)
    : kUserName(UserName) {}

void NotifierForUser::push_message(const std::string &msg) {
    std::unique_lock<engine::Mutex> lock(mutex_);
    send_queue_.push(msg);
    cv_.NotifyOne();
    return;
}

const std::string NotifierForUser::pop_wait() {
    std::unique_lock<engine::Mutex> lock(mutex_);
    (void)cv_.Wait(lock, [&] { return !send_queue_.empty(); });
    const std::string msg = send_queue_.front();
    send_queue_.pop();
    return msg;
}

NotifierComponent::NotifierComponent(
    const components::ComponentConfig &config,
    const components::ComponentContext &context)
    : components::ComponentBase(config, context),
      notifier_(std::make_shared<NotifierClient>()) {};

std::shared_ptr<NotifierComponent::NotifierClient>
NotifierComponent::GetStorage() {
    return notifier_;
}

std::vector<std::pair<int, std::string>>
NotifierComponent::NotifierClient::get_all() {
    std::shared_lock<engine::SharedMutex> lock(shared_mutex_);
    std::vector<std::pair<int, std::string>> to_return;
    for (const auto &[id, notifier_ptr] : umap) {
        to_return.push_back({id, notifier_ptr->kUserName});
    }
    return to_return;
}

void NotifierComponent::NotifierClient::broadcast(const std::string &msg) {
    std::lock_guard<engine::SharedMutex> lock(shared_mutex_);
    for (const auto &[id, notifier_ptr] : umap)
        notifier_ptr->push_message(msg);
    return;
}

void NotifierComponent::NotifierClient::add_notifier(const int &id) {
    std::lock_guard<engine::SharedMutex> lock(shared_mutex_);
    umap[id] = std::make_shared<NotifierForUser>(std::to_string(id));
    return;
}

std::shared_ptr<NotifierForUser>
NotifierComponent::NotifierClient::get_notifier(const int &id) {
    std::shared_lock<engine::SharedMutex> lock(shared_mutex_);
    if (umap.contains(id))
        return umap[id];
    return nullptr;
}
