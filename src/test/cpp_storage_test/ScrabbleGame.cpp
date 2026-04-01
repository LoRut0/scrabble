#include "ScrabbleGame.hpp"

#include <mutex>
#include <shared_mutex>
#include <userver/components/component_base.hpp>
#include <userver/engine/mutex.hpp>
#include <userver/engine/shared_mutex.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/handlers/exceptions.hpp>

namespace ScrabbleGame
{

// try for coroutining
userver::formats::json::Value
Notifier::WaitForUpdate ()
{
    std::unique_lock<userver::engine::Mutex> lock (mutex_);
    cond_var_.Wait (lock, [&] { return !game_state_.IsEmpty (); });
    return game_state_;
}

void
Notifier::UpdateState (userver::formats::json::Value &game_state)
{
    std::lock_guard<userver::engine::Mutex> lock (mutex_);
    game_state_ = std::move (game_state);
    cond_var_.NotifyAll ();
    return;
}

ScrabbleGame::ScrabbleGame (const int num) : value (num), ongoing (false) {};

int
ScrabbleGame::start ()
{
    std::lock_guard<engine::Mutex> lock (mutex_);
    if (ongoing)
        return 1;
    ongoing = 1;
    return 0;
}

void
ScrabbleGame::increment ()
{
    std::lock_guard<engine::Mutex> lock (mutex_);
    ++value;
    return;
}

int
ScrabbleGame::get ()
{
    std::lock_guard<engine::Mutex> lock (mutex_);
    return value;
}

using namespace userver;

StorageComponent::StorageComponent (
    const components::ComponentConfig &config,
    const components::ComponentContext &context)
    : components::ComponentBase (config, context),
      client_ (std::make_shared<Client> ()) {};

std::shared_ptr<Client>
StorageComponent::GetStorage ()
{
    return client_;
}

void
Client::add_game (const int &id, std::shared_ptr<ScrabbleGame> new_game)
{
    const std::lock_guard<engine::SharedMutex> lock (shared_mutex_);
    if (umap.find (id) != umap.end ())
        return;
    umap[id] = new_game;
    return;
}

std::shared_ptr<ScrabbleGame>
Client::get_game (const int &id)
{
    const std::shared_lock<engine::SharedMutex> lock (shared_mutex_);
    if (umap.find (id) == umap.end ())
        return nullptr;
    return umap[id];
}

std::vector<std::array<int, 2>>
Client::get_all ()
{
    const std::shared_lock<engine::SharedMutex> lock (shared_mutex_);
    std::vector<std::array<int, 2>> to_return;
    for (const auto &[id, game_ptr] : umap)
        {
            to_return.push_back ({ id, game_ptr->get () });
        }
    return to_return;
}

} // namespace ScrabbleGame
