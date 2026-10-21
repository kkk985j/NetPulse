#include "netpulse/storage/mysql_connection_pool.hpp"

#include <stdexcept>
#include <utility>

namespace netpulse::storage {

MySqlConnectionPool::MySqlConnectionPool(
    const DatabaseConfig& config,
    std::size_t pool_size)
    : capacity_{pool_size}
{
    if (pool_size == 0)
    {
        error_message_ =
            "MySQL connection pool size must be greater than zero";

        return;
    }

    available_.reserve(pool_size);

    for (std::size_t index = 0;
         index < pool_size;
         ++index)
    {
        auto connection{
            std::make_unique<MySqlConnection>(
                config)
        };

        if (!connection->valid())
        {
            error_number_ =
                connection->errorNumber();

            error_message_ =
                connection->errorMessage();

            available_.clear();
            return;
        }

        available_.push_back(
            std::move(connection));
    }

    initialized_ = true;
}

bool MySqlConnectionPool::valid() const noexcept
{
    return initialized_;
}

unsigned int MySqlConnectionPool::errorNumber()
    const noexcept
{
    return error_number_;
}

const std::string&
MySqlConnectionPool::errorMessage() const noexcept
{
    return error_message_;
}

std::size_t MySqlConnectionPool::capacity()
    const noexcept
{
    return capacity_;
}

std::size_t MySqlConnectionPool::availableCount()
    const
{
    std::lock_guard lock{mutex_};
    return available_.size();
}

std::optional<MySqlConnectionPool::Lease>
MySqlConnectionPool::acquire()
{
    if (!valid())
    {
        return std::nullopt;
    }

    std::unique_lock lock{mutex_};

    connection_available_.wait(
        lock,
        [this]
        {
            return !available_.empty();
        });

    auto connection{
        std::move(available_.back())
    };

    available_.pop_back();

    return std::optional<Lease>{
        Lease{
            this,
            std::move(connection)}
    };
}

void MySqlConnectionPool::release(
    std::unique_ptr<MySqlConnection>
        connection) noexcept
{
    if (connection == nullptr)
    {
        return;
    }

    {
        std::lock_guard lock{mutex_};

        available_.push_back(
            std::move(connection));
    }

    connection_available_.notify_one();
}

MySqlConnectionPool::Lease::Lease(
    MySqlConnectionPool* pool,
    std::unique_ptr<MySqlConnection>
        connection) noexcept
    : pool_{pool}
    , connection_{std::move(connection)}
{
}

MySqlConnectionPool::Lease::~Lease()
{
    reset();
}

MySqlConnectionPool::Lease::Lease(
    Lease&& other) noexcept
    : pool_{
          std::exchange(
              other.pool_,
              nullptr)}
    , connection_{
          std::move(other.connection_)}
{
}

MySqlConnectionPool::Lease&
MySqlConnectionPool::Lease::operator=(
    Lease&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    reset();

    pool_ = std::exchange(
        other.pool_,
        nullptr);

    connection_ =
        std::move(other.connection_);

    return *this;
}

bool MySqlConnectionPool::Lease::valid()
    const noexcept
{
    return pool_ != nullptr
        && connection_ != nullptr
        && connection_->valid();
}

MySqlConnection&
MySqlConnectionPool::Lease::connection()
{
    if (!valid())
    {
        throw std::logic_error{
            "MySQL connection lease is empty"
        };
    }

    return *connection_;
}

MySqlConnection*
MySqlConnectionPool::Lease::operator->()
    noexcept
{
    return connection_.get();
}

MySqlConnection&
MySqlConnectionPool::Lease::operator*()
{
    return connection();
}

void MySqlConnectionPool::Lease::reset()
    noexcept
{
    if (pool_ == nullptr
        || connection_ == nullptr)
    {
        pool_ = nullptr;
        connection_.reset();
        return;
    }

    pool_->release(
        std::move(connection_));

    pool_ = nullptr;
}

} // namespace netpulse::storage