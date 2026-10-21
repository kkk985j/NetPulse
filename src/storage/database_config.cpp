#include "netpulse/storage/database_config.hpp"

#include <charconv>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace netpulse::storage {

namespace {

std::optional<std::string> readEnvironmentVariable(
    const char* name)
{
    const char* value{std::getenv(name)};

    if (value == nullptr || *value == '\0') {
        return std::nullopt;
    }

    return std::string{value};
}

DatabaseConfigResult makeError(
    DatabaseConfigStatus status,
    std::string message)
{
    return {
        status,
        std::nullopt,
        std::move(message)
    };
}

} // namespace

DatabaseConfigResult
loadDatabaseConfigFromEnvironment()
{
    auto host{
        readEnvironmentVariable(
            "NETPULSE_DB_HOST")
    };

    if (!host) {
        return makeError(
            DatabaseConfigStatus::MissingHost,
            "NETPULSE_DB_HOST is not configured");
    }

    auto port_text{
        readEnvironmentVariable(
            "NETPULSE_DB_PORT")
    };

    if (!port_text) {
        return makeError(
            DatabaseConfigStatus::MissingPort,
            "NETPULSE_DB_PORT is not configured");
    }

    auto database{
        readEnvironmentVariable(
            "NETPULSE_DB_NAME")
    };

    if (!database) {
        return makeError(
            DatabaseConfigStatus::MissingDatabaseName,
            "NETPULSE_DB_NAME is not configured");
    }

    auto user{
        readEnvironmentVariable(
            "NETPULSE_DB_USER")
    };

    if (!user) {
        return makeError(
            DatabaseConfigStatus::MissingUser,
            "NETPULSE_DB_USER is not configured");
    }

    auto password{
        readEnvironmentVariable(
            "NETPULSE_DB_PASSWORD")
    };

    if (!password) {
        return makeError(
            DatabaseConfigStatus::MissingPassword,
            "NETPULSE_DB_PASSWORD is not configured");
    }

    unsigned int parsed_port{0};

    const auto [end, error]{
        std::from_chars(
            port_text->data(),
            port_text->data() + port_text->size(),
            parsed_port)
    };

    // from_chars() 可能只解析字符串前半部分，因此还必须确认
    // 整个端口字符串都已被消费。
    if (error != std::errc{}
        || end != port_text->data()
            + port_text->size()
        || parsed_port == 0
        || parsed_port
            > std::numeric_limits<std::uint16_t>::max()) {
        return makeError(
            DatabaseConfigStatus::InvalidPort,
            "NETPULSE_DB_PORT must be an integer "
            "between 1 and 65535");
    }

    return {
        DatabaseConfigStatus::Success,
        DatabaseConfig{
            std::move(*host),
            static_cast<std::uint16_t>(
                parsed_port),
            std::move(*database),
            std::move(*user),
            std::move(*password)
        },
        {}
    };
}

} // namespace netpulse::storage