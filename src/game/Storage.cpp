#include "Storage.hpp"
#include "utils/utils.hpp"

using namespace userver;

namespace ScrabbleGame::Storage {

StorageComponent::StorageComponent(const components::ComponentConfig &config,
                                   const components::ComponentContext &context)
    : components::ComponentBase(config, context),
      client_(std::make_shared<Client>()) {};

std::shared_ptr<Client> StorageComponent::GetStorage() { return client_; }

void Client::add_game(const int &id, std::shared_ptr<ScrabbleGame> new_game) {
    const std::lock_guard<engine::SharedMutex> lock(shared_mutex_);
    umap[id] = new_game;
}

std::shared_ptr<ScrabbleGame> Client::get_game(const int &id) {
    const std::shared_lock<engine::SharedMutex> lock(shared_mutex_);
    if (umap.find(id) == umap.end())
        return nullptr;
    return umap[id];
}

} // namespace ScrabbleGame::Storage
