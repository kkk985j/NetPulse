#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace netpulse::storage {

/// @brief 数据库连接所需的配置。
///
/// 配置由环境变量加载，不应在源码中保存数据库密码。
struct DatabaseConfig
{
    std::string host{};      ///< MySQL 服务地址。
    std::uint16_t port{0};   ///< MySQL TCP 端口。
    std::string database{};  ///< 目标数据库名称。
    std::string user{};      ///< 数据库用户名。
    std::string password{};  ///< 数据库密码。
};

/// @brief 数据库配置加载状态。
enum class DatabaseConfigStatus
{
    Success,              ///< 所有配置均有效。
    MissingHost,          ///< 缺少 `NETPULSE_DB_HOST`。
    MissingPort,          ///< 缺少 `NETPULSE_DB_PORT`。
    MissingDatabaseName,  ///< 缺少 `NETPULSE_DB_NAME`。
    MissingUser,          ///< 缺少 `NETPULSE_DB_USER`。
    MissingPassword,      ///< 缺少 `NETPULSE_DB_PASSWORD`。
    InvalidPort           ///< 端口不是 1～65535 范围内的整数。
};

/// @brief 数据库配置加载结果。
struct DatabaseConfigResult
{
    DatabaseConfigStatus status{
        DatabaseConfigStatus::MissingHost
    };

    std::optional<DatabaseConfig> config{};
    std::string error_message{};

    /// @brief 判断配置是否成功加载。
    ///
    /// @return 状态为 `Success` 且包含配置时返回 `true`。
    [[nodiscard]] bool ok() const noexcept
    {
        return status == DatabaseConfigStatus::Success
            && config.has_value();
    }

    /// @brief 将配置结果显式转换为布尔值。
    ///
    /// @return 与 `ok()` 相同的判断结果。
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return ok();
    }
};

/// @brief 从进程环境变量中加载数据库配置。
///
/// 需要以下环境变量：
///
/// - `NETPULSE_DB_HOST`
/// - `NETPULSE_DB_PORT`
/// - `NETPULSE_DB_NAME`
/// - `NETPULSE_DB_USER`
/// - `NETPULSE_DB_PASSWORD`
///
/// @return 包含数据库配置或具体失败原因的结果。
[[nodiscard]] DatabaseConfigResult
loadDatabaseConfigFromEnvironment();

} // namespace netpulse::storage