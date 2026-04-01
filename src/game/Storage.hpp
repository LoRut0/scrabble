#include <unordered_map>
#include <userver/engine/condition_variable.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/serialize/to.hpp>

#include <userver/components/component_base.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/mutex.hpp>
#include <userver/engine/shared_mutex.hpp>
#include <userver/formats/json.hpp>

#include "ScrabbleGame.hpp"

using namespace userver;

namespace ScrabbleGame::Storage {

class Client final {
  public:
    Client() = default;
    ~Client() = default;

    void add_game(const int &id, std::shared_ptr<ScrabbleGame> new_game);

    /*
     * @brief returns shared_ptr for game
     * @paramm {id} id of game
     * @retval {nullptr} if game was not found
     */
    std::shared_ptr<ScrabbleGame> get_game(const int &id);

  private:
    engine::SharedMutex shared_mutex_;
    std::unordered_map<int, std::shared_ptr<ScrabbleGame>> umap;
};

class StorageComponent final : public components::ComponentBase {
  public:
    // name of your component to refer in static config
    static constexpr std::string_view kName = "game_storage";

    StorageComponent(const components::ComponentConfig &config,
                     const components::ComponentContext &context);

    std::shared_ptr<Client> GetStorage();

  private:
    std::shared_ptr<Client> client_;
};

} // namespace ScrabbleGame::Storage
