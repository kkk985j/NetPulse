#pragma once

#include <string>

namespace netpulse::telemetry {

/// @brief 一条经过解析和业务校验的设备遥测记录。
///
/// 该类型只保存业务数据，不负责 JSON 解析、网络通信或数据库访问。
struct TelemetryRecord
{
    std::string device_id{};  ///< 设备标识，长度为 1～64 个字节。
    double temperature{0.0};  ///< 温度值，允许范围为 -100～200。
    double humidity{0.0};     ///< 相对湿度，允许范围为 0～100。
};

} // namespace netpulse::telemetry