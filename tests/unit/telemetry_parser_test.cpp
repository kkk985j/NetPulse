#include "netpulse/telemetry/telemetry_parser.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

using netpulse::telemetry::TelemetryParseStatus;
using netpulse::telemetry::parseTelemetry;

constexpr double kFloatingPointTolerance{1e-9};

bool expectStatus(
    std::string_view test_name,
    std::string_view payload,
    TelemetryParseStatus expected_status)
{
    const auto result{
        parseTelemetry(payload)
    };

    if (result.status == expected_status
        && !result.record.has_value()) {
        return true;
    }

    std::cerr
        << "FAILED: "
        << test_name
        << ", expected status="
        << static_cast<int>(expected_status)
        << ", actual status="
        << static_cast<int>(result.status)
        << '\n';

    return false;
}

bool testValidTelemetry()
{
    constexpr std::string_view payload{
        R"({
            "device_id": "sensor-001",
            "temperature": 25.4,
            "humidity": 61,
            "location": "laboratory"
        })"
    };

    const auto result{
        parseTelemetry(payload)
    };

    if (!result || !result.record.has_value()) {
        std::cerr
            << "FAILED: valid telemetry was rejected: "
            << result.error_message
            << '\n';

        return false;
    }

    const auto& record{*result.record};

    if (record.device_id != "sensor-001") {
        std::cerr
            << "FAILED: device_id was parsed incorrectly\n";

        return false;
    }

    if (std::abs(record.temperature - 25.4)
        > kFloatingPointTolerance) {
        std::cerr
            << "FAILED: temperature was parsed incorrectly\n";

        return false;
    }

    if (std::abs(record.humidity - 61.0)
        > kFloatingPointTolerance) {
        std::cerr
            << "FAILED: humidity was parsed incorrectly\n";

        return false;
    }

    return true;
}

bool testBoundaryValues()
{
    constexpr std::string_view payload{
        R"({
            "device_id": "boundary-sensor",
            "temperature": -100,
            "humidity": 100
        })"
    };

    const auto result{
        parseTelemetry(payload)
    };

    if (!result || !result.record.has_value()) {
        std::cerr
            << "FAILED: valid boundary values were rejected\n";

        return false;
    }

    return true;
}

} // namespace

int main()
{
    std::size_t failure_count{0};

    const auto check{
        [&failure_count](bool passed)
        {
            if (!passed) {
                ++failure_count;
            }
        }
    };

    check(testValidTelemetry());
    check(testBoundaryValues());

    check(expectStatus(
        "empty payload",
        "",
        TelemetryParseStatus::InvalidJson));

    check(expectStatus(
        "invalid JSON",
        R"({"device_id":)",
        TelemetryParseStatus::InvalidJson));

    check(expectStatus(
        "array root",
        R"([])",
        TelemetryParseStatus::ExpectedObject));

    check(expectStatus(
        "missing humidity",
        R"({
            "device_id": "sensor-001",
            "temperature": 25.4
        })",
        TelemetryParseStatus::MissingField));

    check(expectStatus(
        "invalid device_id type",
        R"({
            "device_id": 123,
            "temperature": 25.4,
            "humidity": 61
        })",
        TelemetryParseStatus::InvalidFieldType));

    check(expectStatus(
        "empty device_id",
        R"({
            "device_id": "",
            "temperature": 25.4,
            "humidity": 61
        })",
        TelemetryParseStatus::InvalidDeviceId));

    const std::string long_device_id(65, 'x');

    const std::string long_device_payload{
        "{\"device_id\":\""
        + long_device_id
        + "\",\"temperature\":25.4,\"humidity\":61}"
    };

    check(expectStatus(
        "device_id too long",
        long_device_payload,
        TelemetryParseStatus::InvalidDeviceId));

    check(expectStatus(
        "temperature too high",
        R"({
            "device_id": "sensor-001",
            "temperature": 201,
            "humidity": 61
        })",
        TelemetryParseStatus::TemperatureOutOfRange));

    check(expectStatus(
        "humidity too high",
        R"({
            "device_id": "sensor-001",
            "temperature": 25.4,
            "humidity": 101
        })",
        TelemetryParseStatus::HumidityOutOfRange));

    if (failure_count != 0) {
        std::cerr
            << failure_count
            << " telemetry parser test(s) failed\n";

        return EXIT_FAILURE;
    }

    std::cout
        << "All telemetry parser tests passed.\n";

    return EXIT_SUCCESS;
}