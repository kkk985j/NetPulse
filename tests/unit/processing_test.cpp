#include "netpulse/server/processing.hpp"

#include <cstddef>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

using netpulse::server::ProcessingTask;
using netpulse::server::processFrame;

namespace {

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error{message};
    }
}

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

std::string toString(
    const std::vector<std::byte>& bytes)
{
    return {
        reinterpret_cast<const char*>(
            bytes.data()),
        bytes.size()
    };
}

void testPreservesConnectionIdentity()
{
    ProcessingTask task{
        42,
        1001,
        toBytes(
            R"({"device_id":"sensor-001"})")
    };

    const auto result{
        processFrame(std::move(task))
    };

    require(
        result.client_fd == 42,
        "result must preserve client fd");

    require(
        result.connection_id == 1001,
        "result must preserve connection id");
}

void testProducesAckResponse()
{
    ProcessingTask task{
        7,
        2002,
        toBytes("telemetry")
    };

    const auto result{
        processFrame(std::move(task))
    };

    require(
        toString(result.response) ==
            "ACK from NetPulse!",
        "processing must produce ACK response");
}

void testAcceptsEmptyPayload()
{
    ProcessingTask task{
        9,
        3003,
        {}
    };

    const auto result{
        processFrame(std::move(task))
    };

    require(
        toString(result.response) ==
            "ACK from NetPulse!",
        "empty payload must still produce ACK");
}

} // namespace

int main()
{
    try
    {
        testPreservesConnectionIdentity();
        testProducesAckResponse();
        testAcceptsEmptyPayload();
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "processing_test failed: "
            << exception.what()
            << '\n';

        return 1;
    }

    std::cout << "processing_test passed\n";
    return 0;
}