#include "netpulse/storage/mysql_connection.hpp"

#include <utility>

namespace netpulse::storage {

namespace {

constexpr unsigned int kConnectTimeoutSeconds{5};

} // namespace

MySqlConnection::MySqlConnection(
    const DatabaseConfig& config)
{
    handle_ = ::mysql_init(nullptr);

    if (handle_ == nullptr)
    {
        error_message_ = "mysql_init() failed";
        return;
    }

    unsigned int connect_timeout{
        kConnectTimeoutSeconds
    };

    if (::mysql_options(
            handle_,
            MYSQL_OPT_CONNECT_TIMEOUT,
            &connect_timeout) != 0)
    {
        captureError();
        close();
        return;
    }

    MYSQL* connected_handle{
        ::mysql_real_connect(
            handle_,
            config.host.c_str(),
            config.user.c_str(),
            config.password.c_str(),
            config.database.c_str(),
            static_cast<unsigned int>(config.port),
            nullptr,
            0)
    };

    if (connected_handle == nullptr)
    {
        captureError();
        close();
        return;
    }

    // 显式使用 utf8mb4，避免遥测文本中的 Unicode 数据被截断或错误转换。
    if (::mysql_set_character_set(
            handle_,
            "utf8mb4") != 0)
    {
        captureError();
        close();
        return;
    }

    error_number_ = 0;
    error_message_.clear();
}

MySqlConnection::~MySqlConnection()
{
    close();
}

MySqlConnection::MySqlConnection(
    MySqlConnection&& other) noexcept
    : handle_{
          std::exchange(
              other.handle_,
              nullptr)}
    , error_number_{
          std::exchange(
              other.error_number_,
              0)}
    , error_message_{
          std::move(other.error_message_)}
{
}

MySqlConnection& MySqlConnection::operator=(
    MySqlConnection&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    close();

    handle_ = std::exchange(
        other.handle_,
        nullptr);

    error_number_ = std::exchange(
        other.error_number_,
        0);

    error_message_ = std::move(
        other.error_message_);

    return *this;
}

bool MySqlConnection::valid() const noexcept
{
    return handle_ != nullptr;
}

bool MySqlConnection::ping()
{
    if (handle_ == nullptr)
    {
        if (error_message_.empty())
        {
            error_message_ =
                "MySQL connection is not initialized";
        }

        return false;
    }

    if (::mysql_ping(handle_) != 0)
    {
        captureError();
        return false;
    }

    error_number_ = 0;
    error_message_.clear();

    return true;
}

unsigned int MySqlConnection::errorNumber()
    const noexcept
{
    return error_number_;
}

const std::string& MySqlConnection::errorMessage()
    const noexcept
{
    return error_message_;
}

MYSQL* MySqlConnection::nativeHandle() noexcept
{
    return handle_;
}

void MySqlConnection::captureError()
{
    if (handle_ == nullptr)
    {
        error_number_ = 0;
        error_message_ =
            "MySQL handle is not initialized";

        return;
    }

    error_number_ = ::mysql_errno(handle_);

    const char* message{
        ::mysql_error(handle_)
    };

    error_message_ =
        message != nullptr
            ? message
            : "Unknown MySQL client error";
}

void MySqlConnection::close() noexcept
{
    if (handle_ == nullptr)
    {
        return;
    }

    ::mysql_close(handle_);
    handle_ = nullptr;
}

} // namespace netpulse::storage