#include "netpulse/storage/database_config.hpp"
#include "netpulse/storage/mysql_connection.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using netpulse::storage::DatabaseConfig;
using netpulse::storage::MySqlConnection;

namespace {

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

/// @brief 将端口环境变量转换为整数。
/// @param value 端口字符串。
/// @return 有效的 TCP 端口号。
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

/// @brief 从测试进程的环境变量构造数据库配置。
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

/// @brief 验证 MySQL 连接能够成功建立并通过健康检查。
void testConnectsAndRespondsToPing()
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

    require(
        connection.nativeHandle() != nullptr,
        "valid connection must expose a MYSQL handle");

    if (!connection.ping())
    {
        throw std::runtime_error{
            "MySQL ping failed: "
                + connection.errorMessage()
        };
    }

    require(
        connection.errorNumber() == 0,
        "successful ping must clear the error number");
}

/// @brief 验证连接对象可以安全转移所有权。
void testMovesConnectionOwnership()
{
    const DatabaseConfig config{
        loadTestConfiguration()
    };

    MySqlConnection original{config};

    require(
        original.valid(),
        "original connection must be valid");

    MySqlConnection moved{
        std::move(original)
    };

    require(
        !original.valid(),
        "moved-from connection must become invalid");

    require(
        moved.valid(),
        "moved connection must remain valid");

    require(
        moved.ping(),
        "moved connection must remain usable");
}

} // namespace

int main()
{
    try
    {
        testConnectsAndRespondsToPing();
        testMovesConnectionOwnership();
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "mysql_connection_test failed: "
            << exception.what()
            << '\n';

        return 1;
    }

    std::cout
        << "mysql_connection_test passed\n";

    return 0;
}