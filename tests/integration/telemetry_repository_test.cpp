#include "netpulse/storage/database_config.hpp"
#include "netpulse/storage/mysql_connection.hpp"
#include "netpulse/storage/telemetry_repository.hpp"
#include "netpulse/telemetry/telemetry_record.hpp"

#include <mysql/mysql.h>

#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

using netpulse::storage::DatabaseConfig;
using netpulse::storage::MySqlConnection;
using netpulse::storage::TelemetryInsertStatus;
using netpulse::storage::TelemetryRepository;
using netpulse::telemetry::TelemetryRecord;

namespace {

constexpr double kFloatingPointTolerance{0.000001};

/// @brief 验证测试条件。
/// @param condition 必须满足的条件。
/// @param message 条件不满足时输出的错误信息。
void require(
    bool condition,
    const char* message)
{
    if (!condition)
    {
        throw std::runtime_error{message};
    }
}

/// @brief 读取必须存在的环境变量。
/// @param name 环境变量名称。
/// @return 环境变量的字符串值。
std::string requireEnvironmentVariable(
    const char* name)
{
    const char* value{
        std::getenv(name)
    };

    if (value == nullptr || *value == '\0')
    {
        throw std::runtime_error{
            std::string{"Missing environment variable: "}
                + name
        };
    }

    return value;
}

/// @brief 将端口字符串转换为有效的 TCP 端口号。
/// @param value 端口字符串。
/// @return 转换后的端口号。
int parsePort(std::string_view value)
{
    std::size_t parsed_characters{0};
    int port{0};

    try
    {
        port = std::stoi(
            std::string{value},
            &parsed_characters);
    }
    catch (const std::exception&)
    {
        throw std::runtime_error{
            "NETPULSE_DB_PORT must be an integer"
        };
    }

    require(
        parsed_characters == value.size(),
        "NETPULSE_DB_PORT contains invalid characters");

    require(
        port >= 1 && port <= 65535,
        "NETPULSE_DB_PORT is outside the valid range");

    return port;
}

/// @brief 从当前进程环境变量加载数据库配置。
/// @return 数据库连接配置。
DatabaseConfig loadTestConfiguration()
{
    const std::string port_text{
        requireEnvironmentVariable(
            "NETPULSE_DB_PORT")
    };

    return DatabaseConfig{
        requireEnvironmentVariable(
            "NETPULSE_DB_HOST"),
        parsePort(port_text),
        requireEnvironmentVariable(
            "NETPULSE_DB_NAME"),
        requireEnvironmentVariable(
            "NETPULSE_DB_USER"),
        requireEnvironmentVariable(
            "NETPULSE_DB_PASSWORD")
    };
}

/// @brief 自动删除集成测试插入的数据库记录。
class RecordCleanup
{
public:
    RecordCleanup(
        MYSQL* connection,
        std::uint64_t record_id) noexcept
        : connection_{connection}
        , record_id_{record_id}
    {
    }

    ~RecordCleanup()
    {
        if (connection_ == nullptr || record_id_ == 0)
        {
            return;
        }

        const std::string query{
            "DELETE FROM telemetry_records WHERE id = "
                + std::to_string(record_id_)
        };

        // 清理失败不应从析构函数抛出异常。
        static_cast<void>(
            ::mysql_real_query(
                connection_,
                query.data(),
                static_cast<unsigned long>(
                    query.size())));
    }

    RecordCleanup(
        const RecordCleanup&) = delete;

    RecordCleanup& operator=(
        const RecordCleanup&) = delete;

private:
    MYSQL* connection_{nullptr};
    std::uint64_t record_id_{0};
};

/// @brief 验证遥测记录能够写入并从数据库读取。
void testInsertsTelemetryRecord()
{
    const DatabaseConfig config{
        loadTestConfiguration()
    };

    MySqlConnection connection{config};

    if (!connection.valid())
    {
        throw std::runtime_error{
            "MySQL connection failed: "
                + connection.errorMessage()
        };
    }

    TelemetryRepository repository{connection};

    const TelemetryRecord expected{
        "repository-test-device",
        26.75,
        58.25
    };

    const auto insert_result{
        repository.insert(expected)
    };

    if (!insert_result)
    {
        throw std::runtime_error{
            "Telemetry insert failed: "
                + insert_result.error_message
        };
    }

    require(
        insert_result.status ==
            TelemetryInsertStatus::Success,
        "insert status must be Success");

    require(
        insert_result.record_id > 0,
        "insert must return a valid record id");

    RecordCleanup cleanup{
        connection.nativeHandle(),
        insert_result.record_id
    };

    const std::string query{
        "SELECT device_id, temperature, humidity "
        "FROM telemetry_records WHERE id = "
            + std::to_string(insert_result.record_id)
    };

    if (::mysql_real_query(
            connection.nativeHandle(),
            query.data(),
            static_cast<unsigned long>(
                query.size())) != 0)
    {
        throw std::runtime_error{
            std::string{"SELECT failed: "}
                + ::mysql_error(
                    connection.nativeHandle())
        };
    }

    MYSQL_RES* result{
        ::mysql_store_result(
            connection.nativeHandle())
    };

    if (result == nullptr)
    {
        throw std::runtime_error{
            std::string{"mysql_store_result() failed: "}
                + ::mysql_error(
                    connection.nativeHandle())
        };
    }

    MYSQL_ROW row{
        ::mysql_fetch_row(result)
    };

    if (row == nullptr)
    {
        ::mysql_free_result(result);

        throw std::runtime_error{
            "inserted telemetry record was not found"
        };
    }

    const std::string actual_device_id{
        row[0] != nullptr ? row[0] : ""
    };

    const double actual_temperature{
        row[1] != nullptr
            ? std::stod(row[1])
            : 0.0
    };

    const double actual_humidity{
        row[2] != nullptr
            ? std::stod(row[2])
            : 0.0
    };

    require(
        actual_device_id == expected.device_id,
        "stored device id does not match");

    require(
        std::abs(
            actual_temperature
                - expected.temperature)
            < kFloatingPointTolerance,
        "stored temperature does not match");

    require(
        std::abs(
            actual_humidity
                - expected.humidity)
            < kFloatingPointTolerance,
        "stored humidity does not match");

    require(
        ::mysql_fetch_row(result) == nullptr,
        "query must return exactly one row");

    ::mysql_free_result(result);
}

} // namespace

int main()
{
    try
    {
        testInsertsTelemetryRecord();
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "telemetry_repository_test failed: "
            << exception.what()
            << '\n';

        return 1;
    }

    std::cout
        << "telemetry_repository_test passed\n";

    return 0;
}