#include "handlers.hpp"

using namespace userver;

inline constexpr std::string_view OngoingSelectAll = R"~(
    Select * from ongoing;
)~";

inline constexpr std::string_view OngoingSelectValueByKey = R"~(
    SELECT value FROM ongoing WHERE key = ?
)~";

inline constexpr std::string_view OngoingInsertKeyValue = R"~(
    INSERT OR IGNORE INTO ongoing DEFAULT VALUES
)~";

inline constexpr std::string_view OngoingDeleteKeyValue = R"~(
    DELETE FROM ongoing WHERE key = ?
)~";

namespace services::websocket {

std::vector<int> WebsocketsHandler::sqlGetAllOngoing() const
{
    std::vector<int> result = sqlite_client_->Execute(storages::sqlite::OperationType::kReadOnly, OngoingSelectAll.data())
                                  .AsVector<int>();
    return result;
}

std::string WebsocketsHandler::sqlNewOngoing() const
{
    storages::sqlite::Transaction transaction = sqlite_client_->Begin(storages::sqlite::OperationType::kReadWrite, {});

    storages::sqlite::ExecutionResult result = transaction.Execute(OngoingInsertKeyValue.data()).AsExecutionResult();
    if (result.rows_affected) {
        transaction.Commit();
        return std::to_string(result.last_insert_id);
    }

    throw server::handlers::InternalServerError(server::handlers::ExternalBody { "SQL Insert error" });
}

int64_t WebsocketsHandler::sqlDeleteValue(std::string_view key) const
{
    const storages::sqlite::Query kDeleteValue { OngoingDeleteKeyValue.data() };

    storages::sqlite::ExecutionResult result = sqlite_client_->Execute(storages::sqlite::OperationType::kReadWrite, kDeleteValue, key).AsExecutionResult();
    return result.rows_affected;
}

}
