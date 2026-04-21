#pragma once

#include <unordered_map>
#include <userver/engine/condition_variable.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/serialize/to.hpp>

#include <userver/components/component_base.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/mutex.hpp>
#include <userver/engine/shared_mutex.hpp>
#include <userver/formats/json.hpp>

#include "GameRoom.hpp"

namespace ScrabbleGame {

class StorageClient final {
  public:
    StorageClient() = default;
    ~StorageClient() = default;

    void new_room(std::shared_ptr<GameRoom> new_room);

    /*
     * @brief tries to delete game from storage
     * @notes checks if user is host
     * @returns {true} if success
     */
    bool delete_room(const u_int64_t &game_id, const int &user_id);

    /*
     * @brief returns shared_ptr for GameRoom
     * @paramm {id} id of game
     * @retval {nullptr} if game was not found
     */
    std::shared_ptr<GameRoom> get_game_room(const u_int64_t &id);

  private:
    engine::SharedMutex shared_mutex_;
    std::unordered_map<u_int64_t, std::shared_ptr<GameRoom>> umap;
};

class StorageComponent final : public components::ComponentBase {
  public:
    // name of your component to refer in static config
    static constexpr std::string_view kName = "game_storage";

    StorageComponent(const components::ComponentConfig &config,
                     const components::ComponentContext &context);

    std::shared_ptr<StorageClient> GetStorage();

  private:
    std::shared_ptr<StorageClient> client_;
};

} // namespace ScrabbleGame
