#pragma once

#include "netpulse/telemetry/telemetry_record.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace netpulse::telemetry {

/// @brief 遥测 JSON 数据的解析状态。
///
/// 用于区分 JSON 语法错误、字段缺失、字段类型错误和业务数据
/// 校验失败等不同情况。
enum class TelemetryParseStatus
{
    Success,                ///< 解析及全部字段校验成功。
    InvalidJson,            ///< 输入不是合法的 JSON 文本。
    ExpectedObject,         ///< JSON 根节点不是对象。
    MissingField,           ///< 缺少必需字段。
    InvalidFieldType,       ///< 字段的数据类型不符合要求。
    InvalidDeviceId,        ///< `device_id` 为空或长度超过限制。
    TemperatureOutOfRange,  ///< `temperature` 超出允许范围。
    HumidityOutOfRange      ///< `humidity` 不在 0～100 范围内。
};

/// @brief 遥测 JSON 数据的解析结果。
///
/// 解析成功时，`record` 保存有效的 `TelemetryRecord`，
/// `error_message` 为空；解析失败时，`record` 为空，
/// `status` 和 `error_message` 描述失败原因。
struct TelemetryParseResult
{
    /// @brief 解析状态。
    ///
    /// 默认设为 `InvalidJson`，避免默认构造的结果被误认为成功。
    TelemetryParseStatus status{
        TelemetryParseStatus::InvalidJson
    };

    /// @brief 解析成功后生成的遥测记录。
    ///
    /// 只有当 `status` 为 `Success` 时才应包含数据。
    std::optional<TelemetryRecord> record{};

    /// @brief 面向日志或错误响应的失败原因。
    ///
    /// 解析成功时该字符串为空。
    std::string error_message{};

    /// @brief 判断解析和业务校验是否成功。
    ///
    /// @return 当状态为 `Success` 且 `record` 包含数据时返回
    /// `true`，否则返回 `false`。
    [[nodiscard]] bool ok() const noexcept
    {
        return status == TelemetryParseStatus::Success
            && record.has_value();
    }

    /// @brief 将解析结果显式转换为布尔值。
    ///
    /// 支持通过 `if (result)` 判断解析是否成功，同时使用
    /// `explicit` 防止发生非预期的隐式类型转换。
    ///
    /// @return 与 `ok()` 相同的判断结果。
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return ok();
    }
};

/// @brief 将 JSON 文本解析为遥测业务记录。
///
/// 输入 JSON 必须是一个对象，并包含以下字段：
///
/// - `device_id`：字符串，长度为 1～64 个字节；
/// - `temperature`：数值，允许范围为 -100～200；
/// - `humidity`：数值，允许范围为 0～100。
///
/// JSON 中额外存在的字段不会影响解析。
///
/// @param[in] payload 待解析的 JSON 文本。该函数不会持有
/// `payload` 指向的数据。
///
/// @return 包含解析状态、遥测记录或错误信息的
/// `TelemetryParseResult`。
[[nodiscard]] TelemetryParseResult parseTelemetry(
    std::string_view payload);

} // namespace netpulse::telemetry