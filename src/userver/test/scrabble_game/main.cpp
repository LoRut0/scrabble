#include "ScrabbleGame.hpp"
#include <cassert>
#include <iostream>

bool word_checker(const std::u32string& word)
{
    if (word.size() > 2)
        return 1;
    return 0;
};

int main()
{
    std::locale::global(std::locale("")); // загружаем локаль по умолчанию (UTF-8)
    std::wcin.imbue(std::locale());
    std::wcout.imbue(std::locale());

    ScrabbleGame::ScrabbleGame game(word_checker);

    while (true) {
        game.draw();

        int coord_len;
        std::cout << "num of letters: ";
        std::wcin >> coord_len;
        assert(coord_len > 0);

        std::vector<std::vector<int>> coords;
        std::vector<char32_t> letters;

        for (int i = 0; i < coord_len; ++i) {
            int x, y;
            wchar_t letter;
            std::cout << "input letter " << i << " x y ";
            std::wcin >> x >> y;
            std::cout << "input letter: ";
            std::wcin >> letter;
            if (letter < U'А' or letter > U'я') {
                std::cerr << "incorrect letters were inputted\n";
                --i;
                continue;
            }
            char32_t converted_letter = static_cast<char32_t>(letter);
            coords.push_back({ x, y });
            letters.push_back(converted_letter);
        }

        std::cout << "score= " << game.TryPlaceTiles(coords, letters) << '\n';
        std::cout << "Place Tiles? = ";
        bool place_bool;
        std::wcin >> place_bool;
        if (place_bool)
            std::cout << "score for tiles = " << game.SubmitWord() << '\n';
    }
    return 0;
}
