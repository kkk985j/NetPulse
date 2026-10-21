#pragma once

#include "netpulse/storage/database_config.hpp"

#include <mysql/mysql.h>

#include <string>

namespace netpulse::storage {

/// @brief 管理单个 MySQL 客户端连接。
///
/// MySqlConnection 使用 RAII 管理 MYSQL 句柄：
/// 构造时建立数据库连接，析构时自动关闭连接。
///
/// 单个 MySqlConnection 实例不是线程安全的，不应被多个线程同时使用。
class MySqlConnection
{
public:
    /// @brief 创建并连接到 MySQL 数据库。
    /// @param config 数据库连接配置。
    explicit MySqlConnection(
        const DatabaseConfig& config);

    /// @brief 关闭数据库连接并释放 MYSQL 句柄。
    ~MySqlConnection();

    MySqlConnection(
        const MySqlConnection&) = delete;

    MySqlConnection& operator=(
        const MySqlConnection&) = delete;

    /// @brief 转移数据库连接所有权。
    /// @param other 被转移的连接对象。
    MySqlConnection(
        MySqlConnection&& other) noexcept;

    /// @brief 转移数据库连接所有权。
    /// @param other 被转移的连接对象。
    /// @return 当前对象的引用。
    MySqlConnection& operator=(
        MySqlConnection&& other) noexcept;

    /// @brief 判断数据库连接是否已经成功建立。
    /// @return 连接有效时返回 true，否则返回 false。
    [[nodiscard]] bool valid() const noexcept;

    /// @brief 检查数据库连接是否仍然可用。
    ///
    /// 检查失败时会更新 errorNumber() 和 errorMessage()。
    ///
    /// @return 连接可用时返回 true，否则返回 false。
    [[nodiscard]] bool ping();

    /// @brief 获取最近一次 MySQL 错误码。
    /// @return MySQL 客户端错误码；没有错误时为 0。
    [[nodiscard]] unsigned int errorNumber()
        const noexcept;

    /// @brief 获取最近一次 MySQL 错误信息。
    /// @return 错误信息字符串。
    [[nodiscard]] const std::string& errorMessage()
        const noexcept;

    /// @brief 获取底层 MYSQL 句柄。
    ///
    /// 返回的句柄不转移所有权，调用方不得调用 mysql_close()。
    ///
    /// @return 连接有效时返回 MYSQL 指针，否则返回 nullptr。
    [[nodiscard]] MYSQL* nativeHandle() noexcept;

private:
    /// @brief 保存底层 MYSQL 句柄当前记录的错误。
    void captureError();

    /// @brief 关闭当前连接。
    void close() noexcept;

    MYSQL* handle_{nullptr};
    unsigned int error_number_{0};
    std::string error_message_{};
};

} // namespace netpulse::storage