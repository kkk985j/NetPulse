#pragma once

#include <cstddef>
#include <span>

//嵌套命名空间 'netpulse::network'，隔离网络模块代码，防止全局命名冲突。
namespace netpulse::network {
    
/*
    1. 'completed'：IO 正常完成；发送 / 接收全部字节成功
    2. 'peer_closed'：对端关闭连接（TCP 对端 close）
    3. 'would_block'：非阻塞 socket 专属：现在没有数据 / 缓冲区满，现在不能读写，需要 epoll 等待事件，不是错误
    4. 'error'：发生真正系统错误
*/    
    enum class IoStatus {
        completed,
        peer_closed,
        would_block,
        error
    };
/*
    1. `status`：上面枚举，本次 IO 是什么情况，默认初始化为 completed
    2. `bytes_transferred`：实际读写多少字节
    3. `error_number`：系统 errno 错误码，
        只有 status=error 的时候才有意义，其余时候为 0
*/
    struct IoResult {
        IoStatus status{ IoStatus::completed };
        std::size_t bytes_transferred{0};
        int error_number{0};
        
        //快捷判断本次 IO 是否完整成功。
        /*
            `[[nodiscard]]`：调用这个函数如果不接收返回值，编译器警告。
                提醒不要忽略判断是否成功。
            `const`：不修改对象成员
            `noexcept`：保证该函数 不会抛出异常
        */
        [[nodiscard]] bool completed() const noexcept
        {
            return status == IoStatus::completed;
        }
    };

    [[nodiscard]] IoResult sendAll(
        int fd,
        std::span<const std::byte> data
    )noexcept;

    [[nodiscard]] IoResult receiveExact(
        int fd,
        std::span<std::byte> buffer
    ) noexcept;

}// namespace netpulse::network

/*
    std::span 是 C++20 的非拥有型连续内存视图，不负责释放内存；
    bytes_transferred 记录实际完成的字节数；
    error_number 保存系统错误码；
    [[nodiscard]] 提醒调用者不能随意忽略网络操作结果；
    noexcept 表示函数通过返回值报告错误，不抛出异常。
*/



