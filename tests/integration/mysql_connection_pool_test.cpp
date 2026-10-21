#include "netpulse/storage/database_config.hpp"
#include "netpulse/storage/mysql_connection_pool.hpp"

#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <stdexcept>
#include <utility>

using netpulse::storage::DatabaseConfig;
using netpulse::storage::MySqlConnectionPool;
using netpulse::storage::
    loadDatabaseConfigFromEnvironment;

namespace {

using namespace std::chrono_literals;

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

/// @brief 加载集成测试使用的数据库配置。
/// @return 有效的数据库连接配置。
DatabaseConfig loadTestConfiguration()
{
    auto result{
        loadDatabaseConfigFromEnvironment()
    };

    if (!result)
    {
        throw std::runtime_error{
            "Failed to load database configuration: "
                + result.error_message
        };
    }

    return std::move(
        result.config.value());
}

/// @brief 验证容量为零的连接池会被拒绝。
void testRejectsZeroCapacity()
{
    const DatabaseConfig config{};

    MySqlConnectionPool pool{
        config,
        0
    };

    require(
        !pool.valid(),
        "zero-capacity pool must be invalid");

    require(
        pool.capacity() == 0,
        "zero-capacity pool must report zero capacity");

    require(
        !pool.errorMessage().empty(),
        "invalid pool must provide an error message");

    require(
        !pool.acquire().has_value(),
        "invalid pool must not provide a connection");
}

/// @brief 验证连接池能够建立固定数量的连接。
void testCreatesConfiguredConnections(
    const DatabaseConfig& config)
{
    MySqlConnectionPool pool{
        config,
        2
    };

    if (!pool.valid())
    {
        throw std::runtime_error{
            "Failed to initialize connection pool: "
                + pool.errorMessage()
        };
    }

    require(
        pool.capacity() == 2,
        "pool capacity must match requested size");

    require(
        pool.availableCount() == 2,
        "all connections must initially be available");
}

/// @brief 验证 Lease 析构后会自动归还连接。
void testLeaseReturnsConnection(
    const DatabaseConfig& config)
{
    MySqlConnectionPool pool{
        config,
        2
    };

    require(
        pool.valid(),
        "connection pool must be valid");

    {
        auto lease{
            pool.acquire()
        };

        require(
            lease.has_value(),
            "pool must provide a connection");

        require(
            lease->valid(),
            "lease must hold a valid connection");

        require(
            pool.availableCount() == 1,
            "acquiring one connection must reduce availability");

        require(
            lease->connection().ping(),
            "leased connection must respond to ping");
    }

    require(
        pool.availableCount() == 2,
        "destroying lease must return the connection");
}

/// @brief 验证移动 Lease 不会复制或重复归还连接。
void testMovesLeaseOwnership(
    const DatabaseConfig& config)
{
    MySqlConnectionPool pool{
        config,
        1
    };

    require(
        pool.valid(),
        "connection pool must be valid");

    {
        auto lease{
            pool.acquire()
        };

        require(
            lease.has_value(),
            "pool must provide a connection");

        MySqlConnectionPool::Lease moved{
            std::move(lease.value())
        };

        require(
            !lease->valid(),
            "moved-from lease must become invalid");

        require(
            moved.valid(),
            "moved lease must remain valid");

        require(
            pool.availableCount() == 0,
            "moved lease must still own the connection");

        require(
            moved->ping(),
            "moved lease connection must remain usable");
    }

    require(
        pool.availableCount() == 1,
        "moved lease must return exactly one connection");
}

/// @brief 验证连接耗尽时 acquire() 会等待连接归还。
void testWaitsForReturnedConnection(
    const DatabaseConfig& config)
{
    MySqlConnectionPool pool{
        config,
        1
    };

    require(
        pool.valid(),
        "connection pool must be valid");

    auto held_lease{
        pool.acquire()
    };

    require(
        held_lease.has_value(),
        "first connection acquisition must succeed");

    auto waiting_acquisition{
        std::async(
            std::launch::async,
            [&pool]
            {
                auto lease{
                    pool.acquire()
                };

                if (!lease.has_value())
                {
                    return false;
                }

                return lease->valid()
                    && lease->connection().ping();
            })
    };

    const auto state_while_exhausted{
        waiting_acquisition.wait_for(100ms)
    };

    // 先归还连接，再进行可能抛出异常的断言，避免后台任务永久等待。
    held_lease.reset();

    const auto state_after_return{
        waiting_acquisition.wait_for(2s)
    };

    const bool acquisition_succeeded{
        state_after_return == std::future_status::ready
            ? waiting_acquisition.get()
            : false
    };

    require(
        state_while_exhausted
            == std::future_status::timeout,
        "acquire must wait while the pool is exhausted");

    require(
        state_after_return
            == std::future_status::ready,
        "waiting acquire must resume after connection return");

    require(
        acquisition_succeeded,
        "connection acquired after waiting must be usable");

    require(
        pool.availableCount() == 1,
        "completed worker must return its connection");
}

} // namespace

int main()
{
    try
    {
        testRejectsZeroCapacity();

        const DatabaseConfig config{
            loadTestConfiguration()
        };

        testCreatesConfiguredConnections(config);
        testLeaseReturnsConnection(config);
        testMovesLeaseOwnership(config);
        testWaitsForReturnedConnection(config);
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "mysql_connection_pool_test failed: "
            << exception.what()
            << '\n';

        return 1;
    }

    std::cout
        << "mysql_connection_pool_test passed\n";

    return 0;
}