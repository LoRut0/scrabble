#include "utfcpp/source/utf8.h"
#include <string>
#include <vector>

inline std::string Char32ToUtf8(char32_t cp)
{
    std::string out;
    utf8::utf32to8(&cp, &cp + 1, std::back_inserter(out));
    return out;
}

inline std::vector<char32_t> utf8str_to_utf32vec_(std::string& utf8str)
{
    std::vector<char32_t> tiles;
    tiles.reserve(utf8str.size()); // upper bound

    utf8::utf8to32(
        utf8str.begin(),
        utf8str.end(),
        std::back_inserter(tiles));
    return tiles;
}
