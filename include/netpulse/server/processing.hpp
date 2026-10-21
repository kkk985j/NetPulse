#pragma once

#include "netpulse/storage/database_config.hpp"

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
/// 此重载不执行数据库写入，主要供不依赖外部数据库的单元测试使用。
///
/// @param task 等待处理的数据帧。
/// @return 保留连接身份并携带业务响应的处理结果。
[[nodiscard]] ProcessingResult processFrame(
    ProcessingTask task);

/// @brief 解析、校验并持久化客户端遥测帧。
///
/// 数据库连接在调用线程内创建，不会在多个 Worker 之间共享。
///
/// @param task 等待处理的数据帧。
/// @param database_config 数据库连接配置。
/// @return 保留连接身份并携带业务响应的处理结果。
[[nodiscard]] ProcessingResult processFrame(
    ProcessingTask task,
    const netpulse::storage::DatabaseConfig&
        database_config);

} // namespace netpulse::server