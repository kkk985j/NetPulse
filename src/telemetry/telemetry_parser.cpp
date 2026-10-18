#include "netpulse/telemetry/telemetry_parser.hpp"

#include <cmath>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace netpulse::telemetry {

namespace {

constexpr std::size_t kMaximumDeviceIdLength{64};

constexpr double kMinimumTemperature{-100.0};
constexpr double kMaximumTemperature{200.0};

constexpr double kMinimumHumidity{0.0};
constexpr double kMaximumHumidity{100.0};

/// @brief 构造一个不包含遥测记录的失败结果。
///
/// @param[in] status 具体的解析失败状态。
/// @param[in] message 面向日志或错误响应的失败原因。
///
/// @return 不包含 `TelemetryRecord` 的解析结果。
[[nodiscard]] TelemetryParseResult makeError(
    TelemetryParseStatus status,
    std::string message)
{
    return {
        status,
        std::nullopt,
        std::move(message)
    };
}

} // namespace

TelemetryParseResult parseTelemetry(
    std::string_view payload)
{
    using Json = nlohmann::json;

    if (payload.empty()) {
        return makeError(
            TelemetryParseStatus::InvalidJson,
            "payload is empty");
    }

    // 禁用解析异常，使客户端提交的非法 JSON 成为普通业务错误，
    // 避免异常穿过工作线程边界。
    const Json document = Json::parse(
        payload.begin(),
        payload.end(),
        nullptr,
        false);

    if (document.is_discarded()) {
        return makeError(
            TelemetryParseStatus::InvalidJson,
            "payload is not valid JSON");
    }

    // 遥测协议要求根节点是对象，数组和标量无法表达具名字段。
    if (!document.is_object()) {
        return makeError(
            TelemetryParseStatus::ExpectedObject,
            "JSON root must be an object");
    }

    const auto device_id_iterator{
        document.find("device_id")
    };

    const auto temperature_iterator{
        document.find("temperature")
    };

    const auto humidity_iterator{
        document.find("humidity")
    };

    if (device_id_iterator == document.end()) {
        return makeError(
            TelemetryParseStatus::MissingField,
            "missing required field: device_id");
    }

    if (temperature_iterator == document.end()) {
        return makeError(
            TelemetryParseStatus::MissingField,
            "missing required field: temperature");
    }

    if (humidity_iterator == document.end()) {
        return makeError(
            TelemetryParseStatus::MissingField,
            "missing required field: humidity");
    }

    if (!device_id_iterator->is_string()) {
        return makeError(
            TelemetryParseStatus::InvalidFieldType,
            "device_id must be a string");
    }

    if (!temperature_iterator->is_number()) {
        return makeError(
            TelemetryParseStatus::InvalidFieldType,
            "temperature must be a number");
    }

    if (!humidity_iterator->is_number()) {
        return makeError(
            TelemetryParseStatus::InvalidFieldType,
            "humidity must be a number");
    }

    const std::string device_id{
        device_id_iterator->get<std::string>()
    };

    const double temperature{
        temperature_iterator->get<double>()
    };

    const double humidity{
        humidity_iterator->get<double>()
    };

    if (device_id.empty()
        || device_id.size() > kMaximumDeviceIdLength) {
        return makeError(
            TelemetryParseStatus::InvalidDeviceId,
            "device_id length must be between 1 and 64 bytes");
    }

    // isfinite() 同时拒绝 NaN 和正负无穷，避免异常浮点值
    // 进入数据库或参与后续统计。
    if (!std::isfinite(temperature)
        || temperature < kMinimumTemperature
        || temperature > kMaximumTemperature) {
        return makeError(
            TelemetryParseStatus::TemperatureOutOfRange,
            "temperature must be between -100 and 200");
    }

    if (!std::isfinite(humidity)
        || humidity < kMinimumHumidity
        || humidity > kMaximumHumidity) {
        return makeError(
            TelemetryParseStatus::HumidityOutOfRange,
            "humidity must be between 0 and 100");
    }

    return {
        TelemetryParseStatus::Success,
        std::optional<TelemetryRecord>{
            TelemetryRecord{
                device_id,
                temperature,
                humidity
            }
        },
        {}
    };
}

} // namespace netpulse::telemetry