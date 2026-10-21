#include "netpulse/storage/telemetry_repository.hpp"

#include <mysql/mysql.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace netpulse::storage {

namespace {

constexpr std::string_view kInsertTelemetrySql{
    "INSERT INTO telemetry_records "
    "(device_id, temperature, humidity) "
    "VALUES (?, ?, ?)"
};

/// @brief 在当前作用域结束时自动释放 MYSQL_STMT。
class StatementGuard
{
public:
    explicit StatementGuard(
        MYSQL_STMT* statement) noexcept
        : statement_{statement}
    {
    }

    ~StatementGuard()
    {
        if (statement_ != nullptr)
        {
            ::mysql_stmt_close(statement_);
        }
    }

    StatementGuard(
        const StatementGuard&) = delete;

    StatementGuard& operator=(
        const StatementGuard&) = delete;

    StatementGuard(
        StatementGuard&&) = delete;

    StatementGuard& operator=(
        StatementGuard&&) = delete;

    [[nodiscard]] MYSQL_STMT* get() const noexcept
    {
        return statement_;
    }

private:
    MYSQL_STMT* statement_{nullptr};
};

/// @brief 根据 Prepared Statement 构造失败结果。
/// @param status 失败状态。
/// @param statement 发生错误的 Prepared Statement。
/// @return 包含 MySQL 错误信息的写入结果。
TelemetryInsertResult makeStatementError(
    TelemetryInsertStatus status,
    MYSQL_STMT* statement)
{
    return {
        status,
        0,
        ::mysql_stmt_errno(statement),
        ::mysql_stmt_error(statement)
    };
}

} // namespace

TelemetryRepository::TelemetryRepository(
    MySqlConnection& connection) noexcept
    : connection_{connection}
{
}

TelemetryInsertResult TelemetryRepository::insert(
    const netpulse::telemetry::TelemetryRecord& record)
{
    MYSQL* connection_handle{
        connection_.nativeHandle()
    };

    if (connection_handle == nullptr)
    {
        return {
            TelemetryInsertStatus::InvalidConnection,
            0,
            connection_.errorNumber(),
            connection_.errorMessage()
        };
    }

    MYSQL_STMT* statement_handle{
        ::mysql_stmt_init(connection_handle)
    };

    if (statement_handle == nullptr)
    {
        return {
            TelemetryInsertStatus::
                StatementInitializationFailed,
            0,
            ::mysql_errno(connection_handle),
            ::mysql_error(connection_handle)
        };
    }

    StatementGuard statement{statement_handle};

    if (::mysql_stmt_prepare(
            statement.get(),
            kInsertTelemetrySql.data(),
            static_cast<unsigned long>(
                kInsertTelemetrySql.size())) != 0)
    {
        return makeStatementError(
            TelemetryInsertStatus::
                StatementPreparationFailed,
            statement.get());
    }

    unsigned long device_id_length{
        static_cast<unsigned long>(
            record.device_id.size())
    };

    double temperature{
        record.temperature
    };

    double humidity{
        record.humidity
    };

    std::array<MYSQL_BIND, 3> parameters{};

    parameters[0].buffer_type = MYSQL_TYPE_STRING;
    parameters[0].buffer =
        const_cast<char*>(record.device_id.data());
    parameters[0].buffer_length = device_id_length;
    parameters[0].length = &device_id_length;

    parameters[1].buffer_type = MYSQL_TYPE_DOUBLE;
    parameters[1].buffer = &temperature;
    parameters[1].buffer_length = sizeof(temperature);

    parameters[2].buffer_type = MYSQL_TYPE_DOUBLE;
    parameters[2].buffer = &humidity;
    parameters[2].buffer_length = sizeof(humidity);

    if (::mysql_stmt_bind_param(
            statement.get(),
            parameters.data()) != 0)
    {
        return makeStatementError(
            TelemetryInsertStatus::
                ParameterBindingFailed,
            statement.get());
    }

    if (::mysql_stmt_execute(statement.get()) != 0)
    {
        return makeStatementError(
            TelemetryInsertStatus::
                StatementExecutionFailed,
            statement.get());
    }

    return {
        TelemetryInsertStatus::Success,
        static_cast<std::uint64_t>(
            ::mysql_stmt_insert_id(
                statement.get())),
        0,
        {}
    };
}

} // namespace netpulse::storage