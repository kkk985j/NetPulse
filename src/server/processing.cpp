#include "netpulse/server/processing.hpp"

#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace netpulse::server {

namespace {

constexpr std::string_view kAckResponse{
    "ACK from NetPulse!"
};

} // namespace

ProcessingResult processFrame(
    ProcessingTask task)
{
    const auto response_bytes{
        std::as_bytes(
            std::span{
                kAckResponse.data(),
                kAckResponse.size()
            })
    };

    std::vector<std::byte> response{
        response_bytes.begin(),
        response_bytes.end()
    };

    return {
        task.client_fd,
        task.connection_id,
        std::move(response)
    };
}

} // namespace netpulse::server