#pragma once

#include "netpulse/storage/mysql_connection.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace netpulse::server {

using ConnectionId = std::uint64_t;

/// @brief 一项等待后台线程处理的客户端数据帧。
struct ProcessingTask
{
    int client_fd{-1};
    ConnectionId connection_id{0};
    std::vector<std::byte> payload{};
};

/// @brief 后台线程完成业务处理后产生的响应。
struct ProcessingResult
{
    int client_fd{-1};
    ConnectionId connection_id{0};
    std::vector<std::byte> response{};
};

/// @brief 解析并校验客户端遥测帧。
///
/// 此重载不执行数据库写入，主要供不依赖数据库的单元测试使用。
///
/// @param task 等待处理的数据帧。
/// @return 保留连接身份并携带业务响应的处理结果。
[[nodiscard]] ProcessingResult processFrame(
    ProcessingTask task);

/// @brief 解析、校验并持久化客户端遥测帧。
///
/// 数据库连接由调用方管理，该函数不会取得连接所有权。
/// connection 为空时返回数据库不可用错误。
///
/// @param task 等待处理的数据帧。
/// @param connection 当前 Worker 从连接池取得的数据库连接。
/// @return 保留连接身份并携带业务响应的处理结果。
[[nodiscard]] ProcessingResult processFrame(
    ProcessingTask task,
    netpulse::storage::MySqlConnection*
        connection);

} // namespace netpulse::server