#include "netpulse/server/processing.hpp"

#include "netpulse/storage/telemetry_repository.hpp"
#include "netpulse/telemetry/telemetry_parser.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace netpulse::server {

namespace {

constexpr std::string_view kAckResponse{
    "ACK from NetPulse!"
};

constexpr std::string_view kErrorPrefix{
    "ERROR: "
};

constexpr std::string_view kDatabaseConnectionError{
    "telemetry database is unavailable"
};

constexpr std::string_view kDatabaseInsertError{
    "telemetry persistence failed"
};

/// @brief 将字符数据复制为字节数组。
/// @param text 等待转换的字符串。
/// @return 包含相同数据的字节数组。
std::vector<std::byte> toBytes(
    std::string_view text)
{
    const auto bytes{
        std::as_bytes(
            std::span{
                text.data(),
                text.size()
            })
    };

    return {
        bytes.begin(),
        bytes.end()
    };
}

/// @brief 构造保留连接身份的处理结果。
/// @param task 原始处理任务。
/// @param response 返回给客户端的文本。
/// @return 完整的异步处理结果。
ProcessingResult makeResult(
    const ProcessingTask& task,
    std::string_view response)
{
    return {
        task.client_fd,
        task.connection_id,
        toBytes(response)
    };
}

/// @brief 构造客户端可接收的错误响应。
/// @param task 原始处理任务。
/// @param message 错误信息。
/// @return 带有 `ERROR:` 前缀的处理结果。
ProcessingResult makeErrorResult(
    const ProcessingTask& task,
    std::string_view message)
{
    std::string response{kErrorPrefix};
    response += message;

    return makeResult(task, response);
}

/// @brief 将任务负载转换为字符串视图。
/// @param task 原始处理任务。
/// @return 指向任务负载的只读字符串视图。
std::string_view payloadView(
    const ProcessingTask& task) noexcept
{
    // 空向量的 data() 不保证能作为有效字符指针使用，
    // 因此为空时显式使用空字符串。
    const char* payload_data{
        task.payload.empty()
            ? ""
            : reinterpret_cast<const char*>(
                task.payload.data())
    };

    return {
        payload_data,
        task.payload.size()
    };
}

} // namespace

ProcessingResult processFrame(
    ProcessingTask task)
{
    const auto parse_result{
        netpulse::telemetry::parseTelemetry(
            payloadView(task))
    };

    if (!parse_result)
    {
        return makeErrorResult(
            task,
            parse_result.error_message);
    }

    return makeResult(
        task,
        kAckResponse);
}

ProcessingResult processFrame(
    ProcessingTask task,
    netpulse::storage::MySqlConnection*
        connection)
{
    const auto parse_result{
        netpulse::telemetry::parseTelemetry(
            payloadView(task))
    };

    if (!parse_result)
    {
        return makeErrorResult(
            task,
            parse_result.error_message);
    }

    if (connection == nullptr
        || !connection->valid())
    {
        return makeErrorResult(
            task,
            kDatabaseConnectionError);
    }

    netpulse::storage::TelemetryRepository repository{
        *connection
    };

    const auto insert_result{
        repository.insert(
            parse_result.record.value())
    };

    if (!insert_result)
    {
        // 不向设备泄漏数据库地址、账号或底层 SQL 错误。
        return makeErrorResult(
            task,
            kDatabaseInsertError);
    }

    return makeResult(
        task,
        kAckResponse);
}

} // namespace netpulse::server