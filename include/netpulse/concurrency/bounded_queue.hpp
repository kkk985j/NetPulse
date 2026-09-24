#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace netpulse::concurrency {

enum class QueuePushStatus
{
    Success,
    Full,
    Closed
};

template <typename T>
class BoundedQueue
{
public:
    explicit BoundedQueue(std::size_t capacity)
        : capacity_{capacity}
    {
        if (capacity_ == 0)
        {
            throw std::invalid_argument{
                "BoundedQueue capacity must be greater than zero"
            };
        }
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    [[nodiscard]] QueuePushStatus tryPush(T value)
    {
        {
            std::lock_guard lock{mutex_};

            if (closed_)
            {
                return QueuePushStatus::Closed;
            }

            if (queue_.size() >= capacity_)
            {
                return QueuePushStatus::Full;
            }

            queue_.push_back(std::move(value));
        }

        not_empty_.notify_one();
        return QueuePushStatus::Success;
    }

    [[nodiscard]] std::optional<T> waitPop()
    {
        std::unique_lock lock{mutex_};

        not_empty_.wait(
            lock,
            [this]
            {
                return closed_ || !queue_.empty();
            });

        if (queue_.empty())
        {
            return std::nullopt;
        }

        T value{std::move(queue_.front())};
        queue_.pop_front();

        return value;
    }

    void close()
    {
        {
            std::lock_guard lock{mutex_};
            closed_ = true;
        }

        not_empty_.notify_all();
    }

    [[nodiscard]] std::size_t size() const
    {
        std::lock_guard lock{mutex_};
        return queue_.size();
    }

    [[nodiscard]] std::size_t capacity() const noexcept
    {
        return capacity_;
    }

    [[nodiscard]] bool closed() const
    {
        std::lock_guard lock{mutex_};
        return closed_;
    }

private:
    const std::size_t capacity_;

    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::deque<T> queue_;

    bool closed_{false};
};

} // namespace netpulse::concurrency