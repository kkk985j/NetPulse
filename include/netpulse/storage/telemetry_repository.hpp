#pragma once

#include "netpulse/storage/mysql_connection.hpp"
#include "netpulse/telemetry/telemetry_record.hpp"

#include <cstdint>
#include <string>

namespace netpulse::storage {

/// @brief 遥测记录写入操作的状态。
enum class TelemetryInsertStatus
{
    Success,
    InvalidConnection,
    StatementInitializationFailed,
    StatementPreparationFailed,
    ParameterBindingFailed,
    StatementExecutionFailed
};

/// @brief 遥测记录写入操作的结果。
struct TelemetryInsertResult
{
    TelemetryInsertStatus status{
        TelemetryInsertStatus::InvalidConnection
    };

    std::uint64_t record_id{0};
    unsigned int error_number{0};
    std::string error_message{};

    /// @brief 判断写入操作是否成功。
    /// @return 写入成功时返回 true，否则返回 false。
    [[nodiscard]] bool ok() const noexcept
    {
        return status == TelemetryInsertStatus::Success;
    }

    /// @brief 将写入结果转换为布尔值。
    /// @return 写入成功时返回 true，否则返回 false。
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return ok();
    }
};

/// @brief 负责将设备遥测记录持久化到 MySQL。
///
/// TelemetryRepository 不拥有数据库连接。调用方必须保证传入的
/// MySqlConnection 在仓储对象的整个生命周期内保持有效。
///
/// 单个仓储对象不应被多个线程同时使用。
class TelemetryRepository
{
public:
    /// @brief 创建遥测数据仓储。
    /// @param connection 仓储使用的 MySQL 连接。
    explicit TelemetryRepository(
        MySqlConnection& connection) noexcept;

    /// @brief 将一条遥测记录写入数据库。
    ///
    /// 写入操作使用 Prepared Statement，不通过字符串拼接构造 SQL。
    ///
    /// @param record 已经过解析和业务校验的遥测记录。
    /// @return 数据库写入结果，成功时包含新记录的自增 ID。
    [[nodiscard]] TelemetryInsertResult insert(
        const netpulse::telemetry::TelemetryRecord& record);

private:
    MySqlConnection& connection_;
};

} // namespace netpulse::storage