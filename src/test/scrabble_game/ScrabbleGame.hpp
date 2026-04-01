#pragma once

#ifdef DEBUG
#include <iostream>
#endif

// #include <unordered_map>
// #include <map>
// #include <algorithm>
// #include <numeric>
#include <array>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <vector>

// #include <userver/formats/json.hpp>

namespace ScrabbleGame {

struct Tile {
    char32_t letter;
    int points;
};

} // namespace ScrabbleGame

constexpr std::array<char32_t, 128> defaultTiles {
    { U'А', U'А', U'А', U'А', U'А', U'А', U'А', U'А', U'А', U'А', U'Б', U'Б', U'Б', U'В', U'В', U'В', U'В', U'В', U'Г', U'Г', U'Г', U'Д', U'Д', U'Д', U'Д', U'Д', U'Е', U'Е', U'Е', U'Е', U'Е', U'Е', U'Е', U'Е', U'Е', U'Ж', U'Ж', U'З', U'З', U'И', U'И', U'И', U'И', U'И', U'И', U'И', U'И', U'Й', U'Й', U'Й', U'Й', U'К', U'К', U'К', U'К', U'К', U'К', U'Л', U'Л', U'Л', U'Л', U'М', U'М', U'М', U'М', U'М', U'Н', U'Н', U'Н', U'Н', U'Н', U'Н', U'Н', U'Н', U'О', U'О', U'О', U'О', U'О', U'О', U'О', U'О', U'О', U'О', U'П', U'П', U'П', U'П', U'П', U'П', U'Р', U'Р', U'Р', U'Р', U'Р', U'Р', U'С', U'С', U'С', U'С', U'С', U'С', U'Т', U'Т', U'Т', U'Т', U'Т', U'У', U'У', U'У', U'Ф', U'Х', U'Х', U'Ц', U'Ч', U'Ч', U'Ш', U'Щ', U'Ъ', U'Ы', U'Ы', U'Ь', U'Ь', U'Э', U'Ю', U'Я', U'Я', U'Я' }
};

namespace ScrabbleGame {

struct PlayerState {
    std::vector<char32_t> hand;
    int score = 0;
};

class Randomizer {
    const int bag_size;
    const int jokers_num;
    int current_max;

    std::mt19937 mt_random;

public:
    Randomizer(const int& bag_size, const int& jokers_num);

    /*
     * @brief returns random number [0, bag_size)
     */
    int random_tile();
    /*
     * @brief return random number [0, bag_size-jokers_num)
     */
    int random_tile_wo_jokers();
    /*
     * @brief returns random number [0, bag_size-num_of_returns)
     *
     * @retval {int} -1 if num_of_returns >= bag_size
     */
    int decrement_random_tile();
};

struct GameState {
public:
    // max num of tiles in player hand
    const unsigned long TILES_MAX_IN_HAND;

    // coordinates of new_tiles in same order
    std::vector<std::vector<int>> new_tiles_coordinates;
    // new_letters that player tries to place
    std::vector<char32_t> new_letters;
    /*
     * @brief score for new tiles
     *
     * @param {-1} new_letters placement is bad
     * @param {>0} new_letters placement is good, and score for them is score
     */
    int score = -1;

    std::vector<PlayerState> playersState;
    int current_player; // index in playersState
    std::vector<char32_t> bag;
    /*
     * @brief holds two-dimensional vector, which represents letters on board
     *
     * @notes board_letters[x][y]
     */
    std::vector<std::vector<std::optional<char32_t>>> board_letters;
    /*
     * @brief holds two-dimensional vector, which represents price of tiles on board
     *
     * @notes board_prices[x][y]
     */
    std::vector<std::vector<int>> board_prices;

    /*
     * @brief fills player's hand with tiles
     *
     * @param {index} player index fron vector playersState
     */
    void FillHand(const int index);

    /*
     * @brief constructor of GameState
     *
     * @param {tiles_max} max num of tiles in player hand
     * @param {players_num} number of players
     * @param {bag_size} number of tiles in bag
     * @param {jokers_num} number of jokers in bag
     * @param {default_tiles} array with all possible tiles with tiles necessary
     * frequency
     */
    GameState(const int& tiles_max, const int& players_num, const int& bag_size,
        const int& jokers_num, const std::array<char32_t, 128>& default_tiles);

private:
    Randomizer randomizer;
    /*
     * @brief fills players bags with tiles, used in initializer
     */
    void FillBag_(const int& bag_size, const int& jokers_num,
        const std::array<char32_t, 128>& default_tiles);
    /*
     * @brief Tries to draw a Tile from bag
     * @retval {char32_t} returns letter from bag (random), if value of char ==
     * 0, then bag is empty
     */
    char32_t DrawTile_();
};

class ScrabbleGame {
public:
    /*
     * @brief Creates new game instance with default values
     *
     * @param {tiles_max} max num of tiles in player hand, def=7, recomended >= 7
     * @param {players_num} num of players, def=2, must be >= 2
     * @param {bag_size} how much letters in bag
     * @param {jokers_num} how much jokers in bag
     * @param {default_tiles} array with all possible tiles with tiles necessary
     * frequency
     */
    ScrabbleGame(std::function<bool(const std::u32string& word)> word_checker,
        const int& tiles_max = 7, const int& players_num = 2,
        const int& bag_size = 131, const int& jokers_num = 3,
        const std::array<char32_t, 128>& default_tiles = defaultTiles);

    /*
     * @brief Tries to place a word on board
     *
     * @param {coordinates} vector{{x,y}, ...} where Tiles placed
     * @param {tiles} vector of letters of word
     * words, should return true if word exists, else if not
     *
     * @retval {state_.score} state_.score becomes 0 or more if tiles placement is
     * correct
     * @retval {int} returns score for Tiles if placement is correct, else -1
     *
     * @throws {char*} error
     */
    int TryPlaceTiles(std::vector<std::vector<int>>& coordinates,
        std::vector<char32_t>& tiles);

    /*
     * @brief Places tiles on board if possible
     *
     * @retval {state_.score>=0} score for tiles, if tiles is placed
     * @retval {state_.score==-1} if tiles can't be placed
     */
    int SubmitWord();

#ifdef DEBUG
    void draw();
#endif

private:
    GameState state_;
    std::function<bool(const std::u32string& word)> word_checker;

    /*
     * @brief Checks placement of tiles inside TryPlaceTiles()
     *
     * @param {word_checker(std::u32string&)} function to check existence of
     * words, should return true if word exists, else if not
     *
     * @retval {state_.score} state_.score becomes 0 or more if tiles placement is
     * correct
     *
     * @throws {char*} error
     */
    void TilesCheck_();

    // TODO: receives only horiz or vert placed tiles,
    // TODO: can return words with one letter
    /*
     * @brief gathers all words thah were formed by placed tiles
     *
     * @retval {vector<u32string>} vector with all words
     *
     *
     * @param {&new_board_letters} board with all new placed tiles
     * @param {&coordinates} vector with coordinates of all new placed tiles
     * @param {&horizontal} should be 1 if all tiles in horizontal row, 0 if all
     * tiles in vertical row
     */
    std::vector<std::u32string>
    GetNewWords_(std::vector<std::vector<std::optional<char32_t>>>& new_board_letters,
        std::vector<std::vector<int>>& coordinates,
        const bool& horizontal);

    /*
     * @brief gathers word that were formed by one tile, which belongs to tiles
     * forming horizontal line
     *
     * @retval {u32string} word formed by this tile
     *
     * @param {&new_board_letters} board with all new placed tiles
     * @param {&tile_coords} coords of tile to check formed words from
     */
    std::u32string
    horizontal_check_(std::vector<std::vector<std::optional<char32_t>>>& new_board_letters,
        std::vector<int>& tile_coords);

    /*
     * @brief gathers word that were formed by one tile, which belongs to tiles
     * forming vertical line
     *
     * @retval {u32string} word formed by this tile
     *
     * @param {&new_board_letters} board with all new placed tiles
     * @param {&tile_coords} coords of tile to check formed words from
     */
    std::u32string
    vertical_check_(std::vector<std::vector<std::optional<char32_t>>>& new_board_letters,
        std::vector<int>& tile_coords);

    /*
     * @brief calculates value of word
     *
     * @param {&word} letter sequence to score, joker = '*'
     *
     * @retval {int} value of word
     */
    int calculate_score_(const std::u32string& word);
};

} // namespace ScrabbleGame
