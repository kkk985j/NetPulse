#include "netpulse/storage/database_config.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>

using netpulse::storage::DatabaseConfigStatus;
using netpulse::storage::
    loadDatabaseConfigFromEnvironment;

namespace {

void require(
    bool condition,
    const char* message)
{
    if (!condition) {
        throw std::runtime_error{message};
    }
}

void setEnvironmentVariable(
    const char* name,
    const char* value)
{
    require(
        ::setenv(name, value, 1) == 0,
        "setenv() failed");
}

void setValidEnvironment()
{
    // 环境变量属于进程全局状态，因此每个测试开始前都恢复
    // 一组完整的有效配置，防止测试之间相互影响。
    setEnvironmentVariable(
        "NETPULSE_DB_HOST",
        "127.0.0.1");

    setEnvironmentVariable(
        "NETPULSE_DB_PORT",
        "3306");

    setEnvironmentVariable(
        "NETPULSE_DB_NAME",
        "netpulse");

    setEnvironmentVariable(
        "NETPULSE_DB_USER",
        "netpulse_app");

    setEnvironmentVariable(
        "NETPULSE_DB_PASSWORD",
        "test-password");
}

void testLoadsValidConfiguration()
{
    setValidEnvironment();

    const auto result{
        loadDatabaseConfigFromEnvironment()
    };

    require(
        static_cast<bool>(result),
        "valid environment must load successfully");

    require(
        result.config->host == "127.0.0.1",
        "host must be loaded");

    require(
        result.config->port == 3306,
        "port must be loaded");

    require(
        result.config->database == "netpulse",
        "database name must be loaded");

    require(
        result.config->user == "netpulse_app",
        "user must be loaded");

    require(
        result.config->password == "test-password",
        "password must be loaded");
}

void testRejectsMissingHost()
{
    setValidEnvironment();
    ::unsetenv("NETPULSE_DB_HOST");

    const auto result{
        loadDatabaseConfigFromEnvironment()
    };

    require(
        result.status ==
            DatabaseConfigStatus::MissingHost,
        "missing host must be rejected");

    require(
        !result.config.has_value(),
        "failed result must not contain configuration");
}

void testRejectsMissingPassword()
{
    setValidEnvironment();
    ::unsetenv("NETPULSE_DB_PASSWORD");

    const auto result{
        loadDatabaseConfigFromEnvironment()
    };

    require(
        result.status ==
            DatabaseConfigStatus::MissingPassword,
        "missing password must be rejected");
}

void testRejectsInvalidPort()
{
    setValidEnvironment();

    setEnvironmentVariable(
        "NETPULSE_DB_PORT",
        "70000");

    const auto result{
        loadDatabaseConfigFromEnvironment()
    };

    require(
        result.status ==
            DatabaseConfigStatus::InvalidPort,
        "out-of-range port must be rejected");
}

void testRejectsPartiallyNumericPort()
{
    setValidEnvironment();

    setEnvironmentVariable(
        "NETPULSE_DB_PORT",
        "3306abc");

    const auto result{
        loadDatabaseConfigFromEnvironment()
    };

    require(
        result.status ==
            DatabaseConfigStatus::InvalidPort,
        "partially numeric port must be rejected");
}

} // namespace

int main()
{
    try {
        testLoadsValidConfiguration();
        testRejectsMissingHost();
        testRejectsMissingPassword();
        testRejectsInvalidPort();
        testRejectsPartiallyNumericPort();
    }
    catch (const std::exception& exception) {
        std::cerr
            << "database_config_test failed: "
            << exception.what()
            << '\n';

        return 1;
    }

    std::cout
        << "database_config_test passed\n";

    return 0;
}