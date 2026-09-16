# Codex engineering guide

## Product intent

Build a portable C++20 camera-system simulator that demonstrates request/result APIs, explicit
memory ownership, bounded concurrency, performance measurement, and fault recovery. It is inspired
by camera HAL architecture but must never claim to implement Android's Camera HAL interface.

## Non-negotiable invariants

- A submitted frame number is preserved through its result.
- Buffers are move-only until the buffer-pool milestone introduces leased shared ownership.
- Invalid requests fail at the session boundary before reaching a device.
- No error path may leak a buffer, strand a waiter, or terminate a worker thread.
- Async lifecycle transitions must be explicit; stopped sessions are non-restartable.
- Shutdown must cancel blocked buffer acquisition and must never self-join from a callback.
- Metrics retention must remain bounded independently of capture duration.
- Queue capacity and overflow behavior must be explicit and observable.
- Performance claims require checked-in benchmark evidence and environment details.
- `main.cpp` is a demo client; business logic belongs in `camera_core`.

## Working method

1. Keep each milestone executable and tested.
2. Write or update tests with behavior changes.
3. Prefer standard-library code in the core; isolate OpenCV behind device or processing adapters.
4. Compile with warnings enabled; do not suppress a warning without documenting why.
5. Run the normal tests and the relevant sanitizer before marking a milestone complete.

## Commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

cmake -S . -B build-asan -DMCH_ENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure

./build/buffer_benchmark --iterations 500
./build/processing_benchmark
```

If CMake is unavailable in a constrained environment, `./scripts/build_direct.sh` verifies the same
sources using a C++20 compiler. Set `MCH_SANITIZE=address` or `MCH_SANITIZE=thread` for sanitizer
verification. Set `MCH_SKIP_BENCHMARKS=1` to shorten sanitizer-only builds.
