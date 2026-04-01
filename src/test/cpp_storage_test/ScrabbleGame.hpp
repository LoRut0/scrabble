#pragma once

#include <initializer_list>
#include <unordered_map>

#include <userver/components/component_base.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/condition_variable.hpp>
#include <userver/engine/mutex.hpp>
#include <userver/engine/shared_mutex.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/serialize/to.hpp>

namespace ScrabbleGame
{

class Notifier
{
  public:
    userver::formats::json::Value WaitForUpdate ();
    void UpdateState (userver::formats::json::Value &game_state);

  private:
    userver::formats::json::Value game_state_;
    userver::engine::Mutex mutex_;
    userver::engine::ConditionVariable cond_var_;
};

class ScrabbleGame
{
  public:
    /*
     * @brief Creates new game instance with default values
     */
    ScrabbleGame (const int num = 0);

    /*
     * @brief starts the game with current player list
     * @retval {1} game is already ongoing
     * @retval {0} OK
     */
    int start ();

    /*
     * @brief ++1
     */
    void increment ();

    /*
     * @brief returns value
     */
    int get ();

    Notifier notifier;

  private:
    userver::engine::Mutex mutex_;

    int value;
    bool ongoing;
};

} // namespace ScrabbleGame

using namespace userver;

namespace ScrabbleGame
{

class Client final
{
  public:
    Client () = default;
    ~Client () = default;

    void add_game (const int &id, std::shared_ptr<ScrabbleGame> new_game);

    /*
     * @brief returns shared_ptr for game
     * @paramm {id} id of game
     * @retval {nullptr} if game was not found
     */
    std::shared_ptr<ScrabbleGame> get_game (const int &id);

    std::vector<std::array<int, 2>> get_all ();

  private:
    engine::SharedMutex shared_mutex_;
    std::unordered_map<int, std::shared_ptr<ScrabbleGame>> umap;
};

class StorageComponent final : public components::ComponentBase
{
  public:
    // name of your component to refer in static config
    static constexpr std::string_view kName = "game_storage";

    StorageComponent (const components::ComponentConfig &config,
                      const components::ComponentContext &context);

    std::shared_ptr<Client> GetStorage ();

  private:
    std::shared_ptr<Client> client_;
};

} // namespace ScrabbleGame
