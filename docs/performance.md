# Performance study

Performance claims in this project are tied to a specific executable, workload, compiler, and host.
They should not be generalized to camera hardware without remeasurement.

## Environment

- Date: 2026-09-16
- CPU: AMD EPYC 9V74 (managed virtualized runtime, 9 visible cores)
- OS: Ubuntu 24.04, x86-64
- Compiler: GCC 13.3.0
- Language/flags: C++20, `-O2`
- Image: 1920x1080 RGB888 (6,220,800 bytes)

## Optimization 1: per-frame allocation to reusable buffer

`buffer_benchmark` compares constructing a new `FrameBuffer` for every frame with acquiring an RAII
lease from a preallocated pool. The timed fill scenario writes every byte, approximating a device
overwriting an output frame. Five runs of 500 frames were recorded.

| Metric | Allocate every frame | Reuse one pooled buffer | Change |
|---|---:|---:|---:|
| Median acquisition-only time | 38.420 ms | 0.042 ms | 914.8x faster |
| Median full-frame-fill time | 70.228 ms | 34.285 ms | 2.05x faster |
| Heap-backed frame allocations | 500 | 1 | 99.8% fewer |

The acquisition-only number isolates allocation and zero-initialization overhead; it is not a claim
of 914x end-to-end camera throughput. The full-fill comparison is the representative result.

An earlier experiment used an eight-buffer pool for a sequential workload and regressed full-fill
time because it rotated through about 50 MB instead of reusing one cache-hot frame. The checked-in
benchmark uses capacity one because only one frame is in flight. Real concurrent workloads require a
larger pool and trade cache locality for in-flight capacity.

## Optimization 2: floating-point to fixed-point grayscale

`processing_benchmark` compares the reference formula using floating-point multiplication and
`lround` against the production fixed-point approximation:

```text
Y = (77R + 150G + 29B + 128) >> 8
```

Three runs processed 100 1080p frames each.

| Implementation | Median total time | Derived throughput | Relative speed |
|---|---:|---:|---:|
| Floating-point reference | 755.517 ms | 132.4 frames/s | 1.00x |
| Fixed-point implementation | 182.494 ms | 548.0 frames/s | 4.14x |

Both implementations produced the same sampled output (`206`) for the deterministic input. Unit
tests also validate known red, green, and blue luma values. This optimization changes the coefficient
representation, so exact output can differ from a floating-point reference by one intensity level.

## Reproduce

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

for run in 1 2 3 4 5; do
  ./build/buffer_benchmark --iterations 500
done

for run in 1 2 3; do
  ./build/processing_benchmark
done
```

Record new results separately when the compiler, flags, hardware, pool capacity, resolution, or
number of in-flight frames changes.
