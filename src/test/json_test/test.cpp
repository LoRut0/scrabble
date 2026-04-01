#include <cstddef>
#include <iostream>
#include <userver/formats/json.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>

using namespace userver;

int main()
{
    formats::json::ValueBuilder json_vb;

    json_vb["game_list"].Resize(5);

    json_vb["game_list"][0]["lox"] = 3;
    json_vb["game_list"][1]["lox"] = 3;
    json_vb["game_list"][2]["lox"] = 3;
    json_vb["game_list"][3]["lox"] = 3;
    json_vb["game_list"][4]["lox"] = 3;

    formats::json::Value json_vl = json_vb.ExtractValue();

    std::cout << formats::json::ToPrettyString(json_vl) << std::endl;

    return 0;
}
