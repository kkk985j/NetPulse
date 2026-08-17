# NetPulse

NetPulse is a high-concurrency device telemetry and fault replay platform 
built with Modern C++ and Linux network programing.

## Requirements

- Ubuntu 24.04 LTS
- GCC 13+
- CMake 3.22+
- Ninja
- C++20

```bash
cmake -S . -B build -G Ninja
cmake --build build --parallel 2
