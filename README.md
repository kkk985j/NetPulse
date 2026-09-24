# NetPulse

NetPulse 是一个基于 C++20、Linux `epoll` 和非阻塞 TCP 的设备遥测服务端项目，支持多客户端连接、长度前缀消息协议以及异步业务处理。

> 当前状态：核心网络链路和异步处理框架已完成，项目仍处于开发阶段。

## 功能特性

- 使用 RAII 管理 Socket、`epoll` 和 `eventfd` 文件描述符
- 基于 `epoll` 实现非阻塞 TCP Reactor
- 支持多个设备客户端并发连接
- 实现可靠 Socket 读写和非阻塞发送缓冲区
- 使用 4 字节网络字节序长度前缀解决 TCP 粘包、拆包问题
- 支持增量解析不完整数据帧和连续数据帧
- 使用 `ThreadPool` 将业务处理从 I/O 线程中分离
- 使用有界任务队列限制待处理任务数量
- 使用 `eventfd` 将工作线程结果通知给 `epoll` 事件循环
- 使用 `ConnectionId` 识别连接生命周期，避免文件描述符复用导致响应错发
- 提供设备模拟器和 13 项单元测试

## 技术栈

| 类别 | 技术 |
|---|---|
| 编程语言 | C++20 |
| 操作系统 | Linux |
| 网络编程 | POSIX Socket、非阻塞 I/O |
| I/O 多路复用 | `epoll` |
| 线程通信 | `eventfd` |
| 并发模型 | `std::thread`、`std::mutex`、`std::condition_variable` |
| 构建工具 | CMake 3.22+、Ninja |
| 测试工具 | CTest |

## 快速开始

### 环境要求

- Linux
- 支持 C++20 的编译器，例如 GCC 11+
- CMake 3.22+
- Ninja

Ubuntu/Debian 可以使用以下命令安装构建工具：

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build
```

### 编译项目

在项目根目录执行：

```bash
cmake -S . -B build -G Ninja
cmake --build build --parallel 2
```

### 运行测试

```bash
ctest --test-dir build --output-on-failure
```

当前项目包含以下测试：

- `socket_raii`
- `reliable_io`
- `message_framing`
- `nonblocking_socket`
- `epoll_wrapper`
- `incremental_frame_decoder`
- `nonblocking_frame_writer`
- `connection_state`
- `bounded_queue`
- `thread_pool`
- `event_fd`
- `frame_processing`
- `processing_dispatcher`

当前验证结果：

```text
100% tests passed, 0 tests failed out of 13
```

## 运行项目

### 启动服务端

```bash
./build/netpulse_server
```

默认监听地址：

```text
0.0.0.0:9000
```

### 启动设备模拟器

打开另一个终端，在项目根目录运行：

```bash
./build/device_simulator
```

模拟器会连接服务端并发送遥测数据，例如：

```json
{
  "device_id": "sensor-001",
  "temperature": 25.4,
  "humidity": 61
}
```

当前业务处理完成后，服务端返回：

```text
ACK from NetPulse!
```

### 并发客户端验证

可以同时启动 5 个设备模拟器：

```bash
for client in 1 2 3 4 5; do
    ./build/device_simulator &
done

wait
```

## 通信协议

NetPulse 当前使用长度前缀二进制帧协议：

```text
+----------------------------+----------------------+
| 4-byte payload length      | payload              |
| uint32, network byte order | variable-length data |
+----------------------------+----------------------+
```

- 帧头长度：4 字节
- 字节序：网络字节序
- Payload 最大长度：1 MiB
- Payload 当前由设备模拟器发送 JSON 文本
- 服务端支持拆包、粘包和增量解析

## 核心处理流程

1. `EventLoop` 通过 `epoll_wait()` 等待网络事件。
2. 监听 Socket 就绪后，循环调用 `accept4()` 接收客户端。
3. `Connection` 从非阻塞 Socket 读取数据。
4. `FrameDecoder` 将字节流解析为完整业务帧。
5. `ProcessingDispatcher` 将帧提交到 `ThreadPool`。
6. 工作线程调用 `processFrame()` 执行业务处理。
7. 处理结果写入结果队列，并通过 `eventfd` 通知 I/O 线程。
8. `EventLoop` 读取处理结果并验证 `ConnectionId`。
9. 响应进入 `FrameWriteBuffer`，由非阻塞写流程发送给客户端。

## 项目结构

```text
NetPulse/
├── apps/
│   └── server/
│       └── main.cpp                  # 服务端入口
├── include/
│   └── netpulse/
│       ├── concurrency/
│       │   ├── bounded_queue.hpp     # 线程安全有界队列
│       │   └── thread_pool.hpp       # 线程池接口
│       ├── network/
│       │   ├── epoll.hpp             # epoll RAII 封装
│       │   ├── event_fd.hpp          # eventfd RAII 封装
│       │   ├── io.hpp                # 可靠 Socket I/O
│       │   ├── nonblocking.hpp       # 非阻塞模式设置
│       │   └── socket.hpp            # Socket RAII 封装
│       ├── protocol/
│       │   ├── frame.hpp             # 长度前缀帧编解码
│       │   ├── frame_decoder.hpp     # 增量帧解析器
│       │   └── frame_write_buffer.hpp # 非阻塞发送缓冲区
│       └── server/
│           ├── connection.hpp        # 单连接状态管理
│           ├── event_loop.hpp        # Reactor 事件循环
│           ├── processing.hpp        # 业务任务与结果定义
│           └── processing_dispatcher.hpp # 异步任务调度
├── src/
│   ├── concurrency/
│   │   └── thread_pool.cpp
│   ├── network/
│   │   ├── epoll.cpp
│   │   ├── event_fd.cpp
│   │   ├── io.cpp
│   │   ├── nonblocking.cpp
│   │   └── socket.cpp
│   ├── protocol/
│   │   ├── frame.cpp
│   │   ├── frame_decoder.cpp
│   │   └── frame_write_buffer.cpp
│   └── server/
│       ├── connection.cpp
│       ├── event_loop.cpp
│       ├── processing.cpp
│       └── processing_dispatcher.cpp
├── tests/
│   └── unit/                         # 单元测试
├── tools/
│   └── device_simulator/
│       └── main.cpp                  # 设备客户端模拟器
├── CMakeLists.txt
└── README.md
```

## 配置说明

当前配置通过源码常量定义，尚未接入配置文件或环境变量。

| 配置项 | 当前值 | 说明 |
|---|---:|---|
| 服务端监听地址 | `0.0.0.0` | 接收所有网络接口的连接 |
| 服务端端口 | `9000` | TCP 监听端口 |
| Listen backlog | `128` | 监听队列长度 |
| 模拟器目标地址 | `127.0.0.1` | 设备模拟器连接地址 |
| Worker 数量 | `4` | 业务工作线程数量 |
| 任务队列容量 | `1024` | 最大待处理任务数量 |
| 单次 epoll 事件数 | `64` | 每轮最多处理的事件数量 |
| 最大 Payload | `1 MiB` | 单个协议帧最大负载 |

当前没有必须设置的环境变量。

## 最近更新

### 2026-09-24

- 新增线程安全的 `BoundedQueue<T>`。
- 新增固定 Worker 数量的 `ThreadPool`。
- 新增 `EventFd`，用于工作线程向 Reactor 线程发送完成通知。
- 新增 `ProcessingTask`、`ProcessingResult` 和 `processFrame()`。
- 新增 `ProcessingDispatcher`，完成任务提交、异步处理和结果回收。
- 将业务处理从 `epoll` I/O 线程中分离，避免业务逻辑阻塞网络事件循环。
- 为连接引入 `ConnectionId`，避免文件描述符复用造成响应发送到错误连接。
- 完成单客户端和 5 客户端并发通信验证。
- 单元测试增加至 13 项，并全部通过。

## 当前限制

- `processFrame()` 当前主要返回固定 ACK，尚未实现完整遥测业务逻辑。
- 尚未实现 JSON 字段校验、设备注册和数据持久化。
- 配置仍以源码常量形式存在。
- 任务队列满时采用快速失败策略。
- 尚未实现 TLS、身份认证和访问控制。
- 尚未完成长期压力测试与性能基准测试。
- 尚未实现完整的 `SIGINT`/`SIGTERM` 优雅停机流程。

## 贡献指南

欢迎通过 Issue 或 Pull Request 参与项目改进。

建议流程：

```bash
git checkout -b feature/your-feature
```

完成修改后执行：

```bash
cmake -S . -B build -G Ninja
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
git diff --check
```

提交代码：

```bash
git add .
git commit -m "feat: describe your change"
```

提交 Pull Request 前请确保：

- 项目能够正常编译
- 所有测试通过
- 新功能包含相应测试
- 没有引入编译警告
- Commit 信息清晰、职责单一

## 联系方式

- 仓库地址：`TODO: 补充 GitHub 仓库地址`
- 作者：`TODO: 补充作者名称`
- 联系方式：`TODO: 补充邮箱或其他联系方式`

## 许可证

`TODO: 补充许可证类型，例如 MIT License。`