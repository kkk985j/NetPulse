#include "netpulse/server/processing.hpp"

#include <cstddef>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using netpulse::server::ProcessingTask;
using netpulse::server::processFrame;

namespace {

void require(
    bool condition,
    const char* message)
{
    if (!condition) {
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
    if (bytes.empty()) {
        return {};
    }

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
            R"({
                "device_id": "sensor-001",
                "temperature": 25.4,
                "humidity": 61
            })")
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

void testProducesAckForValidTelemetry()
{
    ProcessingTask task{
        7,
        2002,
        toBytes(
            R"({
                "device_id": "sensor-001",
                "temperature": 25.4,
                "humidity": 61
            })")
    };

    const auto result{
        processFrame(std::move(task))
    };

    require(
        toString(result.response)
            == "ACK from NetPulse!",
        "valid telemetry must produce ACK response");
}

void testRejectsInvalidJson()
{
    ProcessingTask task{
        8,
        3003,
        toBytes(
            R"({"device_id":)")
    };

    const auto result{
        processFrame(std::move(task))
    };

    require(
        toString(result.response)
            == "ERROR: payload is not valid JSON",
        "invalid JSON must produce an error response");
}

void testRejectsMissingField()
{
    ProcessingTask task{
        9,
        4004,
        toBytes(
            R"({
                "device_id": "sensor-001",
                "temperature": 25.4
            })")
    };

    const auto result{
        processFrame(std::move(task))
    };

    require(
        toString(result.response)
            == "ERROR: missing required field: humidity",
        "missing field must produce an error response");
}

void testRejectsEmptyPayload()
{
    ProcessingTask task{
        10,
        5005,
        {}
    };

    const auto result{
        processFrame(std::move(task))
    };

    require(
        toString(result.response)
            == "ERROR: payload is empty",
        "empty payload must produce an error response");
}

} // namespace

int main()
{
    try {
        testPreservesConnectionIdentity();
        testProducesAckForValidTelemetry();
        testRejectsInvalidJson();
        testRejectsMissingField();
        testRejectsEmptyPayload();
    }
    catch (const std::exception& exception) {
        std::cerr
            << "processing_test failed: "
            << exception.what()
            << '\n';

        return 1;
    }

    std::cout
        << "processing_test passed\n";

    return 0;
}