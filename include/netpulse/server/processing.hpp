#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace netpulse::server {

using ConnectionId = std::uint64_t;

struct ProcessingTask
{
    int client_fd{-1};
    ConnectionId connection_id{0};
    std::vector<std::byte> payload{};
};

struct ProcessingResult
{
    int client_fd{-1};
    ConnectionId connection_id{0};
    std::vector<std::byte> response{};
};

[[nodiscard]] ProcessingResult processFrame(
    ProcessingTask task);

} // namespace netpulse::server