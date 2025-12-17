#include "ScrabbleGame.hpp"
#include "utils.hpp"
#include <codecvt>
#include <cstddef>
#include <locale>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <userver/components/component_base.hpp>
#include <userver/engine/mutex.hpp>
#include <userver/engine/shared_mutex.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/handlers/exceptions.hpp>

namespace ScrabbleGame {

userver::formats::json::Value Notifier::WaitForUpdate()
{
    std::unique_lock<userver::engine::Mutex> lock(mutex_);
    cond_var_.Wait(lock, [&] { return !game_state_.IsEmpty(); });
    return game_state_;
}

void Notifier::UpdateState(userver::formats::json::Value& game_state)
{
    std::lock_guard<userver::engine::Mutex> lock(mutex_);
    game_state_ = std::move(game_state);
    cond_var_.NotifyAll();
    return;
}

Randomizer::Randomizer(const int& bag_size, const int& jokers_num)
    : bag_size(bag_size)
    , jokers_num(jokers_num)
    , current_max(bag_size)
{
    std::random_device rand_device;
    mt_random.seed(rand_device());
}

int Randomizer::random_tile()
{
    std::uniform_int_distribution distribution(0, bag_size - 1);
    return distribution(mt_random);
}

int Randomizer::random_tile_wo_jokers()
{
    std::uniform_int_distribution distribution(0, bag_size - 1 - jokers_num);
    return distribution(mt_random);
}

int Randomizer::decrement_random_tile()
{
    if (current_max <= 0)
        return -1;
    current_max--;
    std::uniform_int_distribution distribution(0, current_max);
    return distribution(mt_random);
}

ScrabbleGame::ScrabbleGame(
    std::function<bool(const std::u32string& word)> word_checker,
    const int& tiles_max, const int& players_num, const int& bag_size,
    const int& jokers_num, const std::array<char32_t, 128>& default_tiles)
    : state_(tiles_max, players_num, bag_size, jokers_num, default_tiles)
    , word_checker(word_checker) { };

// TODO: use players_num
GameState::GameState(const int& tiles_max, const int& players_num,
    const int& bag_size, const int& jokers_num,
    const std::array<char32_t, 128>& default_tiles)
    : TILES_MAX_IN_HAND(tiles_max)
    , board_letters(15, std::vector<std::optional<char32_t>>(15)) // 15 is board dimension
    , board_prices(15, std::vector<int>(15, -1))
    , randomizer(bag_size, jokers_num)
{
    bag.resize(bag_size);
    FillBag_(bag_size, jokers_num, default_tiles);
    // TODO: dimensions of board must be settable
}

void GameState::FillBag_(const int& bag_size, const int& jokers_num,
    const std::array<char32_t, 128>& default_tiles)
{
    if (bag_size == 131 and jokers_num == 3) {
        for (int i = 0; i < 127; ++i) {
            bag[i] = default_tiles[i];
        }
        for (int i = 0; i < jokers_num; ++i) {
            bag[128 + i] = '*';
        }
        return;
    }

    for (int i = 0; i < bag_size; ++i) {
        bag[i] = default_tiles[randomizer.random_tile_wo_jokers()];
    }
    for (int i = 0; i < jokers_num; ++i) {
        bag[bag_size + i] = '*';
    }
    return;
}

void GameState::FillHand(const int index)
{
    PlayerState current_player = playersState[index];

    while (current_player.hand.size() < TILES_MAX_IN_HAND) {
        char32_t new_tile = DrawTile_();
        if (new_tile == 0)
            return;
        current_player.hand.push_back(new_tile);
    }
    return;
}

char32_t GameState::DrawTile_()
{
    if (bag.size() == 0)
        return 0;

    int rand_int = randomizer.decrement_random_tile();
    char32_t return_tile = bag[rand_int];
    bag[rand_int] = bag[bag.size() - 1];
    bag.pop_back();

    return return_tile;
}

void ScrabbleGame::TilesCheck_()
{
    auto& coordinates = state_.new_tiles_coordinates;
    auto& tiles = state_.new_letters;
    auto new_board = state_.board_letters;

    // TODO: to redo from here see todo in declaration
    bool horizontal = 1;
    bool vertical = 1;
    std::vector<int> last_coords = coordinates[0];
    for (size_t i = 1; i < coordinates.size(); i++) {
        const auto& tile_coords = coordinates[i];
        if (tile_coords[0] != last_coords[0])
            vertical = 0;
    }
    last_coords = coordinates[0];
    for (size_t i = 1; i < coordinates.size(); i++) {
        const auto& tile_coords = coordinates[i];
        if (tile_coords[1] != last_coords[1])
            horizontal = 0;
    }

    if (!vertical && !horizontal)
        throw "Tiles are not in line";
    // to here

    for (size_t i = 0; i < coordinates.size(); i++) {
        const auto& tile_coords = coordinates[i];

        if (new_board[tile_coords[0]][tile_coords[1]])
            throw "Coordinates already occupied by another Tile";

        new_board[tile_coords[0]][tile_coords[1]] = tiles[i];
    }

    std::vector<std::u32string> words = GetNewWords_(new_board, coordinates, horizontal);

    for (const auto& word : words) {
        if (!word_checker(word)) {
            state_.score = -1;
            return;
        }
        state_.score += calculate_score_(word);
    }

    return;
}

int ScrabbleGame::calculate_score_(const std::u32string& word)
{
    int score = 0;
    for (const char32_t& letter : word) {
        switch (letter) {
        case U'А':
            score += 1;
            break;
        case U'Б':
            score += 3;
            break;
        case U'В':
            score += 2;
            break;
        case U'Г':
            score += 3;
            break;
        case U'Д':
            score += 2;
            break;
        case U'Е':
            score += 1;
            break;
        case U'Ж':
            score += 5;
            break;
        case U'З':
            score += 5;
            break;
        case U'И':
            score += 1;
            break;
        case U'Й':
            score += 2;
            break;
        case U'К':
            score += 2;
            break;
        case U'Л':
            score += 2;
            break;
        case U'М':
            score += 2;
            break;
        case U'Н':
            score += 1;
            break;
        case U'О':
            score += 1;
            break;
        case U'П':
            score += 2;
            break;
        case U'Р':
            score += 2;
            break;
        case U'С':
            score += 2;
            break;
        case U'Т':
            score += 2;
            break;
        case U'У':
            score += 3;
            break;
        case U'Ф':
            score += 10;
            break;
        case U'Х':
            score += 5;
            break;
        case U'Ц':
            score += 10;
            break;
        case U'Ч':
            score += 5;
            break;
        case U'Ш':
            score += 10;
            break;
        case U'Щ':
            score += 10;
            break;
        case U'Ъ':
            score += 10;
            break;
        case U'Ы':
            score += 5;
            break;
        case U'Ь':
            score += 5;
            break;
        case U'Э':
            score += 10;
            break;
        case U'Ю':
            score += 10;
            break;
        case U'Я':
            score += 3;
            break;
        }
    }
    return score;
}

int ScrabbleGame::TryPlaceTiles(std::vector<std::vector<int>>& coordinates,
    std::vector<char32_t>& tiles)
{
    std::lock_guard<engine::Mutex> lock(mutex_);
    state_.score = -1;

    if (coordinates.size() != tiles.size())
        throw "Vector sizes does not match";

    state_.new_tiles_coordinates = std::move(coordinates);
    state_.new_letters = std::move(tiles);

    TilesCheck_();
    return state_.score;
}

int ScrabbleGame::SubmitWord()
{
    std::lock_guard<engine::Mutex> lock(mutex_);
    if (state_.score == -1)
        return -1;

    auto& new_tiles = state_.new_letters;
    auto& new_tiles_coordinates = state_.new_tiles_coordinates;

    for (size_t i = 0; i < new_tiles.size(); ++i) {
        const int& tile_x = new_tiles_coordinates[i][0];
        const int& tile_y = new_tiles_coordinates[i][1];
        state_.board_letters[tile_x][tile_y] = new_tiles[i];
    }
    return state_.score;
}

std::vector<std::u32string> ScrabbleGame::GetNewWords_(
    std::vector<std::vector<std::optional<char32_t>>>& new_board_letters,
    std::vector<std::vector<int>>& coordinates, const bool& horizontal)
{
    // TODO: may be better to have possibility to place tiles not in a row
    std::vector<std::u32string> words;
    if (horizontal) {
        // horizontal check
        {
            std::u32string word = horizontal_check_(new_board_letters, coordinates[0]);
            if (word.size() > 1)
                words.push_back(word);
        }
        // vertical checks
        for (size_t i = 0; i < coordinates.size(); i++) {
            std::u32string word = vertical_check_(new_board_letters, coordinates[i]);
            if (word.size() > 1)
                words.push_back(word);
        }
    } else {
        // horizontal check
        for (size_t i = 0; i < coordinates.size(); i++) {
            std::u32string word = horizontal_check_(new_board_letters, coordinates[i]);
            if (word.size() > 1)
                words.push_back(word);
        }
        // vertical checks
        {
            std::u32string word = vertical_check_(new_board_letters, coordinates[0]);
            if (word.size() > 1)
                words.push_back(word);
        }
    }
    return words;
}

GameState ScrabbleGame::get_game_state()
{
    std::lock_guard<engine::Mutex> lock(mutex_);
    return state_;
}

std::u32string ScrabbleGame::horizontal_check_(
    std::vector<std::vector<std::optional<char32_t>>>& new_board_letters,
    std::vector<int>& tile_coords)
{
    std::u32string word;
    const int& tile_x = tile_coords[0];
    const int& tile_y = tile_coords[1];

    for (int x = tile_x - 1; x >= 0; x--) {
        if (!new_board_letters[x][tile_y])
            break;
        word += *(new_board_letters[x][tile_y]);
    }
    word = std::u32string(word.rbegin(), word.rend()) + *(new_board_letters[tile_x][tile_y]);
    for (size_t x = tile_x + 1; x < new_board_letters.size(); x++) {
        if (!new_board_letters[x][tile_y])
            break;
        word += *(new_board_letters[x][tile_y]);
    }

    return word;
}

std::u32string ScrabbleGame::vertical_check_(
    std::vector<std::vector<std::optional<char32_t>>>& new_board_letters,
    std::vector<int>& tile_coords)
{
    std::u32string word;
    const int& tile_x = tile_coords[0];
    const int& tile_y = tile_coords[1];

    for (int y = tile_y - 1; y >= 0; y--) {
        if (!new_board_letters[tile_x][y])
            break;
        word += *(new_board_letters[tile_x][y]);
    }
    word = std::u32string(word.rbegin(), word.rend()) + *(new_board_letters[tile_x][tile_y]);
    for (size_t y = tile_y + 1; y < new_board_letters[0].size(); y++) {
        if (!new_board_letters[tile_x][y])
            break;
        word += *(new_board_letters[tile_x][y]);
    }

    return word;
}

int ScrabbleGame::check_if_player_joined(const int64_t& user_id)
{
    std::lock_guard<engine::Mutex> lock(mutex_);
    auto finder = std::find(state_.players.begin(), state_.players.end(), user_id);
    if (finder == state_.players.end())
        return 0;
    return std::distance(state_.players.begin(), finder);
}

int ScrabbleGame::set_players(std::vector<int64_t>& players)
{
    std::lock_guard<engine::Mutex> lock(mutex_);
    this->state_.players.swap(players);
    return 0;
}

int ScrabbleGame::start()
{
    std::lock_guard<engine::Mutex> lock(mutex_);
    if (ongoing)
        return 1;
    ongoing = 1;
    return 0;
}

#ifdef DEBUG
void ScrabbleGame::draw()
{

    auto print_char32 = [](char32_t c) {
        std::u32string s(1, c);
        std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
        std::string utf8 = conv.to_bytes(s);
        std::cout << utf8;
    };

    const auto& board = state_.board_letters;
    for (size_t x = 0; x < board.size(); ++x) {
        for (size_t y = 0; y < board[0].size(); ++y) {
            if (!board[x][y]) {
                std::cout << "* ";
            } else {
                print_char32(*(board[x][y]));
                std::cout << ' ';
            }
        }
        std::cout << '\n';
    }
}
#endif

} // namespace ScrabbleGame

using namespace userver;

namespace ScrabbleGame::Storage {

StorageComponent::StorageComponent(const components::ComponentConfig& config, const components::ComponentContext& context)
    : components::ComponentBase(config, context)
    , client_(std::make_shared<Client>()) { };

std::shared_ptr<Client> StorageComponent::GetStorage()
{
    return client_;
}

void Client::add_game(const int& id, std::shared_ptr<ScrabbleGame> new_game)
{
    const std::lock_guard<engine::SharedMutex> lock(shared_mutex_);
    umap[id] = new_game;
}

std::shared_ptr<ScrabbleGame> Client::get_game(const int& id)
{
    const std::shared_lock<engine::SharedMutex> lock(shared_mutex_);
    if (umap.find(id) == umap.end())
        return nullptr;
    return umap[id];
}

} // namespace ScrabbleGame::Storage

namespace ScrabbleGame {

userver::formats::json::Value Serialize(const GameState& data, userver::formats::serialize::To<userver::formats::json::Value>)
{
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
