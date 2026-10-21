#pragma once

#include "netpulse/storage/database_config.hpp"
#include "netpulse/storage/mysql_connection.hpp"

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace netpulse::storage {

/// @brief 固定容量的 MySQL 连接池。
///
/// 连接池在构造时建立全部数据库连接。调用方通过 acquire()
/// 获取一个 Lease，Lease 析构时会自动把连接归还连接池。
///
/// 连接池必须比所有从中取得的 Lease 存活更久。
class MySqlConnectionPool
{
public:
    class Lease;

    /// @brief 创建固定容量的数据库连接池。
    /// @param config 每个 MySQL 连接使用的数据库配置。
    /// @param pool_size 连接池包含的连接数量，必须大于 0。
    MySqlConnectionPool(
        const DatabaseConfig& config,
        std::size_t pool_size);

    ~MySqlConnectionPool() = default;

    MySqlConnectionPool(
        const MySqlConnectionPool&) = delete;

    MySqlConnectionPool& operator=(
        const MySqlConnectionPool&) = delete;

    MySqlConnectionPool(
        MySqlConnectionPool&&) = delete;

    MySqlConnectionPool& operator=(
        MySqlConnectionPool&&) = delete;

    /// @brief 判断连接池是否初始化成功。
    /// @return 全部连接均成功建立时返回 true。
    [[nodiscard]] bool valid() const noexcept;

    /// @brief 获取连接池初始化时记录的 MySQL 错误码。
    /// @return MySQL 错误码；没有错误时为 0。
    [[nodiscard]] unsigned int errorNumber()
        const noexcept;

    /// @brief 获取连接池初始化错误信息。
    /// @return 错误信息字符串。
    [[nodiscard]] const std::string& errorMessage()
        const noexcept;

    /// @brief 获取连接池总容量。
    /// @return 构造时请求的连接数量。
    [[nodiscard]] std::size_t capacity()
        const noexcept;

    /// @brief 获取当前可用连接数量。
    /// @return 尚未被 Lease 持有的连接数量。
    [[nodiscard]] std::size_t availableCount()
        const;

    /// @brief 获取一个数据库连接。
    ///
    /// 当所有连接都在使用时，该函数会等待其他 Lease
    /// 归还连接。连接池无效时立即返回空值。
    ///
    /// @return 持有数据库连接的 Lease；连接池无效时为空。
    [[nodiscard]] std::optional<Lease> acquire();

private:
    friend class Lease;

    /// @brief 将连接归还连接池。
    /// @param connection 需要归还的数据库连接。
    void release(
        std::unique_ptr<MySqlConnection>
            connection) noexcept;

    mutable std::mutex mutex_;
    std::condition_variable connection_available_;

    std::vector<std::unique_ptr<MySqlConnection>>
        available_{};

    std::size_t capacity_{0};
    unsigned int error_number_{0};
    std::string error_message_{};
    bool initialized_{false};
};

/// @brief 一个从连接池临时借出的 MySQL 连接。
///
/// Lease 是只移动类型。Lease 生命周期结束时，连接自动返回
/// 创建它的 MySqlConnectionPool。
class MySqlConnectionPool::Lease
{
public:
    ~Lease();

    Lease(const Lease&) = delete;
    Lease& operator=(const Lease&) = delete;

    /// @brief 转移连接租约所有权。
    /// @param other 被转移的租约。
    Lease(Lease&& other) noexcept;

    /// @brief 转移连接租约所有权。
    /// @param other 被转移的租约。
    /// @return 当前租约的引用。
    Lease& operator=(Lease&& other) noexcept;

    /// @brief 判断租约是否持有有效连接。
    /// @return 持有连接时返回 true。
    [[nodiscard]] bool valid() const noexcept;

    /// @brief 获取租约持有的数据库连接。
    /// @return 数据库连接引用。
    [[nodiscard]] MySqlConnection& connection();

    /// @brief 通过箭头操作符访问数据库连接。
    /// @return 数据库连接指针。
    [[nodiscard]] MySqlConnection* operator->()
        noexcept;

    /// @brief 通过解引用操作符访问数据库连接。
    /// @return 数据库连接引用。
    [[nodiscard]] MySqlConnection& operator*();

private:
    friend class MySqlConnectionPool;

    Lease(
        MySqlConnectionPool* pool,
        std::unique_ptr<MySqlConnection>
            connection) noexcept;

    /// @brief 将当前连接归还连接池。
    void reset() noexcept;

    MySqlConnectionPool* pool_{nullptr};

    std::unique_ptr<MySqlConnection>
        connection_{};
};

} // namespace netpulse::storage