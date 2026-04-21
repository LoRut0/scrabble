#include "GameStorage.hpp"
#include "utils/utils.hpp"

using namespace userver;

namespace ScrabbleGame {

StorageComponent::StorageComponent(const components::ComponentConfig &config,
                                   const components::ComponentContext &context)
    : components::ComponentBase(config, context),
      client_(std::make_shared<StorageClient>()) {};

std::shared_ptr<StorageClient> StorageComponent::GetStorage() {
    return client_;
}

void StorageClient::new_room(std::shared_ptr<GameRoom> new_room) {
    u_int64_t game_id = new_room->game_id();
    const std::lock_guard<engine::SharedMutex> lock(shared_mutex_);
    umap[game_id] = new_room;
    return;
}

bool StorageClient::delete_room(const u_int64_t &game_id, const int &user_id) {
    if (get_game_room(game_id)->check_for_user(user_id) != 0)
        return false;
    umap.erase(game_id);
    return true;
}

std::shared_ptr<GameRoom> StorageClient::get_game_room(const u_int64_t &id) {
    const std::shared_lock<engine::SharedMutex> lock(shared_mutex_);
    if (umap.find(id) == umap.end())
        return nullptr;
    return umap[id];
}

} // namespace ScrabbleGame

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
