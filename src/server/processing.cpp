#include "netpulse/server/processing.hpp"

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

} // namespace

ProcessingResult processFrame(
    ProcessingTask task)
{
    // 空向量的 data() 不保证能作为有效字符指针使用，
    // 因此为空时显式使用空字符串。
    const char* payload_data{
        task.payload.empty()
            ? ""
            : reinterpret_cast<const char*>(
                task.payload.data())
    };

    const std::string_view payload{
        payload_data,
        task.payload.size()
    };

    const auto parse_result{
        netpulse::telemetry::parseTelemetry(payload)
    };

    if (!parse_result) {
        std::string error_response{kErrorPrefix};
        error_response += parse_result.error_message;

        return makeResult(
            task,
            error_response);
    }

    return makeResult(
        task,
        kAckResponse);
}

} // namespace netpulse::server