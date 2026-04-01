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

namespace ScrabbleGame {

userver::formats::json::Value
Serialize(const GameState &data,
          userver::formats::serialize::To<userver::formats::json::Value>) {
    formats::json::ValueBuilder json_vb;

    // letters
    for (size_t x = 0; x < data.board_letters.size(); ++x) {
        for (size_t y = 0; y < data.board_letters[x].size(); ++y) {

            std::string cell;

            if (data.board_letters[x][y]) {
                cell = Char32ToUtf8(*data.board_letters[x][y]);
            } else {
                cell = " "; // пустая клетка
            }

            json_vb["letters"][x][y] = cell;
        }
    }

    // prices
    for (size_t x = 0; x < data.board_prices.size(); ++x) {
        for (size_t y = 0; y < data.board_prices[x].size(); ++y) {
            json_vb["prices"][x][y] = data.board_prices[x][y];
        }
    }

    return json_vb.ExtractValue();
}

} // namespace ScrabbleGame
