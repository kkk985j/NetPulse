# NetPulse

NetPulse 是一个基于 C++20、Linux `epoll`、非阻塞 TCP 和 MySQL 的设备遥测服务端项目，支持多客户端连接、长度前缀消息协议、异步业务处理、JSON 数据校验及遥测数据持久化。

> 当前状态：核心网络链路、异步处理框架和 MySQL 持久化链路已经完成，项目仍处于开发阶段。

## 功能特性

- 使用 RAII 管理 Socket、`epoll`、`eventfd` 和 MySQL 连接
- 基于 `epoll` 实现非阻塞 TCP Reactor
- 支持多个设备客户端并发连接
- 实现可靠 Socket 读写和非阻塞发送缓冲区
- 使用 4 字节网络字节序长度前缀解决 TCP 粘包、拆包问题
- 支持增量解析不完整数据帧和连续数据帧
- 使用 `ThreadPool` 将业务处理从 I/O 线程中分离
- 使用有界任务队列限制待处理任务数量
- 使用 `eventfd` 将工作线程结果通知给 `epoll` 事件循环
- 使用 `ConnectionId` 识别连接生命周期，避免文件描述符复用导致响应错发
- 使用 `nlohmann/json` 解析遥测 JSON
- 支持设备 ID、温度和湿度字段校验
- 使用 Prepared Statement 防止 SQL 注入
- 将合法遥测记录持久化到 MySQL
- 使用固定容量 MySQL 连接池复用数据库连接
- 提供设备模拟器
- 提供 15 项普通测试和 3 项 MySQL 集成测试

## 技术栈

| 类别 | 技术 |
|---|---|
| 编程语言 | C++20 |
| 操作系统 | Linux |
| 网络编程 | POSIX Socket、非阻塞 I/O |
| I/O 多路复用 | `epoll` |
| 线程通信 | `eventfd` |
| 并发模型 | `std::thread`、`std::mutex`、`std::condition_variable` |
| 数据格式 | JSON、`nlohmann/json` |
| 数据库 | MySQL 8、MySQL C API |
| 数据访问 | Prepared Statement、连接池 |
| 构建工具 | CMake 3.22+、Ninja、PkgConfig |
| 测试工具 | CTest |

## 快速开始

### 环境要求

- Linux
- 支持 C++20 的编译器，例如 GCC 11+
- CMake 3.22+
- Ninja
- PkgConfig
- MySQL 8
- MySQL Client 开发库
- `nlohmann/json`

Ubuntu/Debian 可以使用以下命令安装依赖：

```bash
sudo apt update

sudo apt install \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    nlohmann-json3-dev \
    libmysqlclient-dev \
    mysql-server
```

确认 MySQL 服务已经启动：

```bash
systemctl is-active mysql
```

如果没有启动：

```bash
sudo systemctl start mysql
```

### 配置数据库

项目需要名为 `netpulse` 的数据库和 `telemetry_records` 表。

进入 MySQL：

```bash
sudo mysql
```

创建数据库：

```sql
CREATE DATABASE IF NOT EXISTS netpulse
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_0900_ai_ci;

USE netpulse;
```

创建遥测记录表：

```sql
CREATE TABLE IF NOT EXISTS telemetry_records
(
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    device_id VARCHAR(64) NOT NULL,
    temperature DOUBLE NOT NULL,
    humidity DOUBLE NOT NULL,
    received_at TIMESTAMP(6)
        NOT NULL
        DEFAULT CURRENT_TIMESTAMP(6),

    PRIMARY KEY (id),

    KEY idx_device_received_at
        (device_id, received_at),

    CONSTRAINT chk_humidity_range
        CHECK (humidity >= 0 AND humidity <= 100)
);
```

创建应用用户时，请自行设置本地开发密码：

```sql
CREATE USER IF NOT EXISTS
    'netpulse_app'@'localhost'
    IDENTIFIED BY 'TODO: 替换为本地开发密码';

CREATE USER IF NOT EXISTS
    'netpulse_app'@'127.0.0.1'
    IDENTIFIED BY 'TODO: 替换为本地开发密码';

GRANT SELECT, INSERT, UPDATE, DELETE
    ON netpulse.*
    TO 'netpulse_app'@'localhost';

GRANT SELECT, INSERT, UPDATE, DELETE
    ON netpulse.*
    TO 'netpulse_app'@'127.0.0.1';

FLUSH PRIVILEGES;

EXIT;
```

不要把真实数据库密码提交到 Git 仓库。

### 配置环境变量

创建本地配置目录：

```bash
mkdir -p ~/.config/netpulse
chmod 700 ~/.config/netpulse
```

创建配置文件：

```bash
code ~/.config/netpulse/database.env
```

写入：

```bash
NETPULSE_DB_HOST=127.0.0.1
NETPULSE_DB_PORT=3306
NETPULSE_DB_NAME=netpulse
NETPULSE_DB_USER=netpulse_app
NETPULSE_DB_PASSWORD='TODO: 替换为本地开发密码'
```

限制文件权限：

```bash
chmod 600 ~/.config/netpulse/database.env
```

每次打开新终端后，需要重新加载环境变量：

```bash
set -a
source ~/.config/netpulse/database.env
set +a
```

### 编译项目

只编译项目和普通测试：

```bash
cmake -S . -B build -G Ninja
cmake --build build --parallel 2
```

启用需要真实 MySQL 服务的集成测试：

```bash
cmake -S . -B build -G Ninja \
    -DNETPULSE_ENABLE_INTEGRATION_TESTS=ON

cmake --build build --parallel 2
```

## 运行测试

### 普通测试

没有启用集成测试时，可以运行 15 项普通测试：

```bash
ctest \
    --test-dir build \
    --output-on-failure
```

### MySQL 集成测试

先加载数据库环境变量：

```bash
set -a
source ~/.config/netpulse/database.env
set +a
```

运行标记为 `integration` 的测试：

```bash
ctest \
    --test-dir build \
    -L integration \
    --output-on-failure
```

### 全部测试

```bash
ctest \
    --test-dir build \
    --output-on-failure
```

当前项目包含以下测试：

| 测试名称 | 类型 | 覆盖内容 |
|---|---|---|
| `socket_raii` | 普通测试 | Socket RAII 生命周期 |
| `reliable_io` | 普通测试 | 可靠 Socket 读写 |
| `message_framing` | 普通测试 | 长度前缀帧协议 |
| `nonblocking_socket` | 普通测试 | 非阻塞模式设置 |
| `epoll_wrapper` | 普通测试 | `epoll` RAII 封装 |
| `incremental_frame_decoder` | 普通测试 | 增量帧解析 |
| `nonblocking_frame_writer` | 普通测试 | 非阻塞发送缓冲区 |
| `connection_state` | 普通测试 | 单连接状态管理 |
| `bounded_queue` | 普通测试 | 线程安全有界队列 |
| `thread_pool` | 普通测试 | 线程池任务执行与停止 |
| `event_fd` | 普通测试 | `eventfd` 通知机制 |
| `frame_processing` | 普通测试 | 遥测业务帧处理 |
| `processing_dispatcher` | 普通测试 | 异步任务提交与结果回收 |
| `telemetry_parser` | 普通测试 | JSON 解析和业务校验 |
| `database_config` | 普通测试 | 数据库环境变量配置 |
| `mysql_connection` | 集成测试 | 真实 MySQL 连接和健康检查 |
| `telemetry_repository` | 集成测试 | 遥测记录写入及查询验证 |
| `mysql_connection_pool` | 集成测试 | 连接获取、等待、移动及自动归还 |

当前全部测试验证结果：

```text
100% tests passed, 0 tests failed out of 18
```

## 运行项目

### 启动服务端

每次打开新终端后，先加载数据库配置：

```bash
cd ~/projects/NetPulse

set -a
source ~/.config/netpulse/database.env
set +a
```

启动服务端：

```bash
./build/netpulse_server
```

默认监听地址：

```text
0.0.0.0:9000
```

正常启动时会输出：

```text
NetPulse epoll server listening on 0.0.0.0:9000, workers=4, task_queue_capacity=1024, telemetry_persistence=enabled
```

服务端启动时会：

1. 加载数据库环境变量；
2. 验证 MySQL 连接；
3. 创建固定容量的数据库连接池；
4. 创建工作线程；
5. 启动 `epoll` Reactor。

如果数据库配置缺失或者 MySQL 无法连接，服务端会快速失败，不会开始监听设备连接。

### 启动设备模拟器

打开另一个终端，在项目根目录运行：

```bash
cd ~/projects/NetPulse
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

只有在 JSON 解析、业务校验和数据库写入全部成功后，服务端才返回：

```text
ACK from NetPulse!
```

处理失败时会返回以 `ERROR:` 开头的响应。

### 并发客户端验证

可以同时启动 5 个设备模拟器：

```bash
for client in 1 2 3 4 5; do
    ./build/device_simulator &
done

wait
```

### 查询遥测记录

```bash
sudo mysql -e "
SELECT
    id,
    device_id,
    temperature,
    humidity,
    received_at
FROM netpulse.telemetry_records
ORDER BY id DESC
LIMIT 10;
"
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
- Payload 当前为 UTF-8 JSON 文本
- 服务端支持拆包、粘包和增量解析

遥测 JSON 当前包含以下字段：

| 字段 | 类型 | 约束 |
|---|---|---|
| `device_id` | string | 长度为 1～64 个字节 |
| `temperature` | number | 范围为 -100～200 |
| `humidity` | number | 范围为 0～100 |

## 核心处理流程

1. `EventLoop` 通过 `epoll_wait()` 等待网络事件。
2. 监听 Socket 就绪后，循环调用 `accept4()` 接收客户端。
3. `Connection` 从非阻塞 Socket 读取数据。
4. `FrameDecoder` 将字节流解析为完整业务帧。
5. `ProcessingDispatcher` 将帧提交给 `ThreadPool`。
6. Worker 从 `MySqlConnectionPool` 获取数据库连接。
7. `parseTelemetry()` 解析 JSON 并进行业务字段校验。
8. `TelemetryRepository` 使用 Prepared Statement 写入 MySQL。
9. 数据库连接通过 `Lease` 自动归还连接池。
10. 处理结果写入完成队列，并通过 `eventfd` 通知 I/O 线程。
11. `EventLoop` 读取处理结果并验证 `ConnectionId`。
12. 响应进入 `FrameWriteBuffer`，由非阻塞写流程发送给客户端。

数据库操作只在 Worker 线程中执行，不会阻塞 `epoll` I/O 线程。

## 项目结构

```text
NetPulse/
├── apps/
│   └── server/
│       └── main.cpp                         # 服务端入口
├── include/
│   └── netpulse/
│       ├── concurrency/
│       │   ├── bounded_queue.hpp            # 线程安全有界队列
│       │   └── thread_pool.hpp              # 固定 Worker 线程池
│       ├── network/
│       │   ├── epoll.hpp                    # epoll RAII 封装
│       │   ├── event_fd.hpp                 # eventfd RAII 封装
│       │   ├── io.hpp                       # 可靠 Socket I/O
│       │   ├── nonblocking.hpp              # 非阻塞模式设置
│       │   └── socket.hpp                   # Socket RAII 封装
│       ├── protocol/
│       │   ├── frame.hpp                    # 长度前缀帧编解码
│       │   ├── frame_decoder.hpp            # 增量帧解析器
│       │   └── frame_write_buffer.hpp       # 非阻塞发送缓冲区
│       ├── server/
│       │   ├── connection.hpp               # 单连接状态管理
│       │   ├── event_loop.hpp               # Reactor 事件循环
│       │   ├── processing.hpp               # 业务任务与结果定义
│       │   └── processing_dispatcher.hpp    # 异步任务调度
│       ├── storage/
│       │   ├── database_config.hpp          # 数据库环境配置
│       │   ├── mysql_connection.hpp         # MySQL RAII 连接
│       │   ├── mysql_connection_pool.hpp    # MySQL 连接池
│       │   └── telemetry_repository.hpp     # 遥测数据仓储
│       └── telemetry/
│           ├── telemetry_parser.hpp         # 遥测 JSON 解析
│           └── telemetry_record.hpp         # 遥测业务数据结构
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
│   ├── server/
│   │   ├── connection.cpp
│   │   ├── event_loop.cpp
│   │   ├── processing.cpp
│   │   └── processing_dispatcher.cpp
│   ├── storage/
│   │   ├── database_config.cpp
│   │   ├── mysql_connection.cpp
│   │   ├── mysql_connection_pool.cpp
│   │   └── telemetry_repository.cpp
│   └── telemetry/
│       └── telemetry_parser.cpp
├── tests/
│   ├── integration/                         # 需要真实 MySQL 的集成测试
│   │   ├── mysql_connection_pool_test.cpp
│   │   ├── mysql_connection_test.cpp
│   │   └── telemetry_repository_test.cpp
│   └── unit/                                # 不依赖外部服务的测试
├── tools/
│   └── device_simulator/
│       └── main.cpp                         # 设备客户端模拟器
├── CMakeLists.txt
└── README.md
```

## 配置说明

网络和并发参数目前仍由源码常量定义：

| 配置项 | 当前值 | 说明 |
|---|---:|---|
| 服务端监听地址 | `0.0.0.0` | 接收所有网络接口的连接 |
| 服务端端口 | `9000` | TCP 监听端口 |
| Listen backlog | `128` | 监听队列长度 |
| 模拟器目标地址 | `127.0.0.1` | 设备模拟器连接地址 |
| Worker 数量 | `4` | 业务工作线程数量 |
| MySQL 连接池容量 | `4` | 当前与 Worker 数量一致 |
| 任务队列容量 | `1024` | 最大等待任务数量 |
| 单次 epoll 事件数 | `64` | 每轮最多处理的事件数量 |
| 最大 Payload | `1 MiB` | 单个协议帧最大负载 |

数据库配置通过环境变量提供：

| 环境变量 | 必需 | 说明 |
|---|---|---|
| `NETPULSE_DB_HOST` | 是 | MySQL 服务地址 |
| `NETPULSE_DB_PORT` | 是 | MySQL TCP 端口，范围为 1～65535 |
| `NETPULSE_DB_NAME` | 是 | 数据库名称 |
| `NETPULSE_DB_USER` | 是 | 数据库用户名 |
| `NETPULSE_DB_PASSWORD` | 是 | 数据库密码 |

数据库密码不得写入源码、README、CMake 或提交到 Git 仓库。

## 最近更新

### 2026-10-21

- 新增 `TelemetryRecord` 遥测业务数据结构。
- 新增基于 `nlohmann/json` 的遥测 JSON 解析和字段校验。
- 新增 `DatabaseConfig`，从环境变量安全加载数据库配置。
- 新增 `MySqlConnection`，使用 RAII 管理 MySQL 连接。
- 新增 `TelemetryRepository`，使用 Prepared Statement 写入遥测记录。
- 新增固定容量 `MySqlConnectionPool`。
- 使用 `Lease` 自动归还数据库连接。
- 将连接池接入 `ProcessingDispatcher` 和后台 Worker。
- 数据库操作与 `epoll` I/O 线程保持分离。
- 完成 5 客户端并发入库验证。
- 测试增加至 18 项并全部通过。

### 2026-09-24

- 新增线程安全的 `BoundedQueue<T>`。
- 新增固定 Worker 数量的 `ThreadPool`。
- 新增 `EventFd`，用于工作线程向 Reactor 线程发送完成通知。
- 新增 `ProcessingTask`、`ProcessingResult` 和 `processFrame()`。
- 新增 `ProcessingDispatcher`，完成任务提交、异步处理和结果回收。
- 将业务处理从 `epoll` I/O 线程中分离，避免业务逻辑阻塞网络事件循环。
- 为连接引入 `ConnectionId`，避免文件描述符复用造成响应发送到错误连接。
- 完成单客户端和 5 客户端并发通信验证。
- 单元测试增加至 13 项并全部通过。

## 当前限制

- 尚未实现设备注册和设备身份认证。
- 尚未实现 TLS 加密和访问控制。
- 网络端口、Worker 数量和队列容量仍以源码常量定义。
- 数据库表结构尚未接入自动迁移工具。
- 连接池连接失效后尚未实现自动重建。
- 连接池等待连接时尚未实现超时和取消机制。
- 任务队列满时采用快速失败策略。
- 目前使用标准输出和标准错误记录日志，尚未接入结构化日志系统。
- 尚未完成长期压力测试与性能基准测试。
- 尚未实现完整的 `SIGINT`/`SIGTERM` 优雅停机流程。

## 代码注释规范

本项目统一使用 [Doxygen](https://www.doxygen.nl/manual/docblocks.html) 风格编写 C++ 接口文档。

### 基本约定

- 注释使用中文，代码标识符、类型名和函数名保持英文。
- 头文件中的公开类型、函数和方法使用 `///` 文档注释。
- 函数注释应说明作用、参数、返回值、错误情况和必要的使用约束。
- 枚举项及简单数据成员使用 `///<` 行尾注释。
- `.cpp` 文件中的实现注释使用普通 `//`。
- 实现注释主要解释设计原因、边界条件、资源所有权、并发约束和错误处理。
- 不为明显的语法、简单赋值、`#include` 或大括号添加重复代码含义的注释。
- 修改代码行为时，必须同步更新相关注释。

### 常用 Doxygen 标记

| 标记 | 用途 |
|---|---|
| `@brief` | 简要说明类型或函数的作用 |
| `@param[in]` | 输入参数 |
| `@param[out]` | 输出参数 |
| `@param[in,out]` | 同时作为输入和输出的参数 |
| `@return` | 说明函数返回值 |
| `@retval` | 说明某个具体返回值的含义 |
| `@note` | 补充使用说明 |
| `@warning` | 标记重要风险或限制 |
| `@throws` | 说明可能抛出的异常 |

### 头文件接口示例

```cpp
/// @brief 将 JSON 文本解析为遥测业务记录。
///
/// @param[in] payload 待解析的 JSON 文本；函数不会持有该数据。
///
/// @return 包含解析状态、遥测记录或错误信息的解析结果。
[[nodiscard]] TelemetryParseResult parseTelemetry(
    std::string_view payload);
```

### 枚举与成员示例

```cpp
/// @brief 遥测数据的解析状态。
enum class TelemetryParseStatus
{
    Success,           ///< 解析及业务校验成功。
    InvalidJson,       ///< 输入不是合法 JSON。
    MissingField,      ///< 缺少必需字段。
    InvalidFieldType   ///< 字段类型不符合要求。
};

struct TelemetryRecord
{
    std::string device_id{};  ///< 设备标识。
    double temperature{0.0};  ///< 温度值。
    double humidity{0.0};     ///< 相对湿度。
};
```

### 实现注释示例

```cpp
// 在访问具体字段前检查根节点类型，避免将数组或标量误判为遥测对象。
if (!document.is_object())
{
    return makeError(
        TelemetryParseStatus::ExpectedObject,
        "JSON root must be an object");
}
```

实现注释应解释为什么需要该判断，而不是简单重复代码：

```cpp
// 不推荐：检查 document 是否为 object。
if (!document.is_object())
{
    // ...
}
```

新增或修改公开接口时，应同时检查对应的 Doxygen 注释是否仍与实际行为一致。

## 贡献指南

欢迎通过 Issue 或 Pull Request 参与项目改进。

建议流程：

```bash
git checkout -b feature/your-feature
```

完成普通测试修改后执行：

```bash
cmake -S . -B build -G Ninja
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
git diff --check
```

需要运行 MySQL 集成测试时：

```bash
set -a
source ~/.config/netpulse/database.env
set +a

cmake -S . -B build -G Ninja \
    -DNETPULSE_ENABLE_INTEGRATION_TESTS=ON

cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
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
- 没有提交数据库密码或其他凭据
- Commit 信息清晰、职责单一

## 联系方式

- 仓库地址：`TODO: 补充 GitHub 仓库地址`
- 作者：`TODO: 补充作者名称`
- 联系方式：`TODO: 补充邮箱或其他联系方式`

## 许可证

`TODO: 补充许可证类型，例如 MIT License。`