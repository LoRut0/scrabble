#include "handlers.hpp"
#include <stdexcept>
#include <string_view>

#define MAX_SEQ 20

inline constexpr std::string_view OngoingCreateTable = R"~(
    CREATE TABLE IF NOT EXISTS ongoing (
    key INTEGER PRIMARY KEY
    )
)~";

namespace services::websocket {

namespace Actions {
    inline PlayerAction from_string(std::string str)
    {
        if (str == "start") {
            return PlayerAction::start;
        } else if (str == "place") {
            return PlayerAction::place;
        } else if (str == "change") {
            return PlayerAction::change;
        } else if (str == "pass") {
            return PlayerAction::pass;
        } else if (str == "submit") {
            return PlayerAction::submit;
        } else if (str == "end") {
            return PlayerAction::end;
        }
        throw server::handlers::ClientError(server::handlers::ExternalBody { "Wrong Player-req.action" });
    }
};

WebsocketsHandler::WebsocketsHandler(const components::ComponentConfig& config, const components::ComponentContext& context)
    : server::websocket::WebsocketHandlerBase(config, context)
    , sqlite_client_(context.FindComponent<components::SQLite>("sqlitedb").GetClient())
    , game_storage_client_(context.FindComponent<ScrabbleGame::Storage::StorageComponent>("game_storage").GetStorage()) { };

void WebsocketsHandler::Handle(server::websocket::WebSocketConnection& chat, server::request::RequestContext&) const
{
    server::websocket::Message message;
    while (!engine::current_task::ShouldCancel()) {
        chat.Recv(message); // throws on closed/dropped connection
        if (message.close_status)
            break; // explicit close if any
        ParseMessage(message);
        chat.Send(message); // throws on closed/dropped connection
    }
    if (message.close_status)
        chat.Close(*message.close_status);
}

int WebsocketsHandler::JsonFormatCheck(formats::json::Value& msg) const
{
    const std::string action = msg["action"].As<std::string>();
    return -1;
}

void WebsocketsHandler::ParseMessage(server::websocket::Message& message) const
{
    if (!message.is_text) {
        throw server::handlers::ClientError(server::handlers::ExternalBody { "No json in request" });
    }
    formats::json::Value json_msg { formats::json::FromString(std::string_view(message.data)) };
    const std::string action = json_msg["action"].As<std::string>();
    switch (Actions::from_string(action)) {
    case Actions::PlayerAction::start: {
        action_start(message, json_msg);
        break;
    }
    case Actions::PlayerAction::change: {
        break;
    }
    case Actions::PlayerAction::end: {
        break;
    }
    case Actions::PlayerAction::pass: {
        break;
    }
    case Actions::PlayerAction::submit: {
        break;
    }
    case Actions::PlayerAction::place: {
        break;
    }

        // TODO: more_cases
    }
}

void WebsocketsHandler::action_start(server::websocket::Message& message, const formats::json::Value& json_msg) const
{
    return;
}

void WebsocketsHandler::action_place(server::websocket::Message& message, const formats::json::Value& json_msg) const
{
    // LOG_DEBUG() << "Started case for rolling dices";
    //
    // const int64_t gameID = json_msg["gameID"].As<int64_t>();
    // formats::json::Value current_state = redisGetJson(std::to_string(gameID));
    // int rollnum = current_state["state"]["rollnum"].As<int>();
    //
    // if (rollnum >= 3) {
    //     LOG_DEBUG() << "Client tries to play exceccive turn";
    //     message.data = "{\"status\":409, \"error\":\"exceccive_turn\"}";
    //     return;
    // }
    //
    // // vector with boolean values, shows which dices to reroll
    // const std::vector<int> dices_to_reroll { ToVector<int>(json_msg["dices"]) };
    // // vector with random generated dices
    // const std::vector<int> random_dices { DiceGame::RollDices() };
    //
    // LOG_DEBUG() << [&dices_to_reroll, &random_dices, &gameID](auto& out) {
    //     out << "dices_to_reroll:\n";
    //     for (const auto& elem : dices_to_reroll) {
    //         out << elem << " ";
    //     }
    //     out << "\nrandom_dices:\n";
    //     for (const auto& elem : random_dices) {
    //         out << elem << " ";
    //     }
    //     out << "\ngameID = " << gameID;
    // };
    //
    // std::vector<int> dices_in_json { ToVector<int>(current_state["state"]["dices"]) };
    // formats::json::ValueBuilder vb { current_state };
    // for (int i = 0; i < dices_to_reroll.size(); i++) {
    //     if (dices_to_reroll[i]) {
    //         dices_in_json[i] = random_dices[i];
    //         vb["state"]["dices"][i] = random_dices[i];
    //     }
    // }
    // DiceGame::Combinations cur_combinations = DiceGame::CalculateSequences(dices_in_json);
    // vb["state"]["cur_sequences"] = cur_combinations;
    //
    // vb["state"]["rollnum"] = ++rollnum;
    // vb["status"] = 100;
    //
    // current_state = vb.ExtractValue();
    // message.data = formats::json::ToStableString(current_state);
    // redisPostJson(std::to_string(gameID), message.data);
    //
    // LOG_DEBUG() << "Websocket msg to client = " << message.data;

    return;
}

void WebsocketsHandler::action_choose(server::websocket::Message& message, const formats::json::Value& json_msg) const
{
    // LOG_DEBUG() << "Started case for choosing sequence";
    // const int64_t gameID = json_msg["gameID"].As<int64_t>();
    // formats::json::Value current_state = redisGetJson(std::to_string(gameID));
    //
    // if (current_state["state"]["rollnum"].As<int>() == 0) {
    //     LOG_DEBUG() << "Client tries to choose seq without rolling";
    //     message.data = "{\"status\":409, \"error\":\"no_roll\"}";
    //     return;
    // }
    //
    // std::string current_player;
    // if (current_state["state"]["playerq"].As<int>() == 0)
    //     current_player = "p1";
    // else
    //     current_player = "p2";
    // std::string choosed_seq { json_msg["seq"].As<std::string>() };
    //
    // if (current_state["state"][current_player]["sequences"][choosed_seq].As<int>() != -1) {
    //     LOG_DEBUG() << "Client tries to choose seq, which already had been choosed";
    //     message.data = "{\"status\":409, \"error\":\"seq_already_choosed\"}";
    //     return;
    // }
    //
    // formats::json::ValueBuilder vb { current_state };
    // vb["state"][current_player]["sequences"][choosed_seq] = current_state["state"]["cur_sequences"][choosed_seq].As<int>();
    // vb["state"]["cur_sequences"] = DiceGame::Combinations(0);
    //
    // vb["state"]["rollnum"] = 0;
    // if (current_player == "p1")
    //     vb["state"]["playerq"] = 1;
    // else
    //     vb["state"]["playerq"] = 0;
    //
    // current_state = vb.ExtractValue();
    // message.data = formats::json::ToStableString(current_state);
    // redisPostJson(std::to_string(gameID), message.data);
    return;
}

void WebsocketsHandler::DefaultInit(formats::json::ValueBuilder& json_for_redis, int64_t gameID) const
{
    // json_for_redis["gameID"] = gameID;
    // // TODO: random queue at the beginning of game
    // json_for_redis["state"]["playerq"] = 0;
    // json_for_redis["state"]["rollnum"] = 0;
    //
    // json_for_redis["state"]["dices"].Resize(5);
    // for (int i = 0; i < 5; i++) {
    //     json_for_redis["state"]["dices"][i] = -1;
    // }
    //
    // json_for_redis["state"]["p1"]["sequences"] = DiceGame::Combinations(-1);
    // json_for_redis["state"]["p2"]["sequences"] = DiceGame::Combinations(-1);
    // json_for_redis["state"]["cur_sequences"] = DiceGame::Combinations(-1);

    return;
}

template <typename T>
std::vector<T> WebsocketsHandler::ToVector(const formats::json::Value& json_vl) const
{
    std::vector<T> to_return;
    to_return.reserve(json_vl.GetSize());
    for (const auto& elem : json_vl) {
        to_return.push_back(elem.As<T>());
    }
    return to_return;
}

}
