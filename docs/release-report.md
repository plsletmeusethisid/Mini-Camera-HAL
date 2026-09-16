# Feature completion report

This release implements the seven additions required to make Mini Camera HAL a stronger camera and
native-systems portfolio project.

| Requirement | Implementation | Evidence |
|---|---|---|
| Reusable buffer pool | Fixed-capacity `BufferPool`; RAII leases use a custom deleter and shared pool state | Reuse, high-water, outliving-facade, and blocked-shutdown tests |
| Allocation benchmark | `buffer_benchmark` compares per-frame construction and pooled reuse at 1080p | Raw five-run record plus median analysis in `performance.md` |
| Image processing | Manual RGB-to-gray, lookup-table gamma, and bilinear resize | Known-value and shape tests; processing benchmark |
| OpenCV adapter | PImpl-based webcam/video adapter fills ordinary or pooled buffers | Optional CMake target; adapter compile/missing-file test in CI |
| Runtime metrics | Thread-safe FPS and mean/P50/P95/P99/max capture latency; pool high-water statistics | CLI summary and percentile test |
| GitHub Actions | Release build with OpenCV, CTest, benchmark smoke; ASan/UBSan and TSan jobs | `.github/workflows/ci.yml` |
| Measured optimization | Buffer reuse and fixed-point grayscale | 2.05x full-frame-fill and 4.14x grayscale speedups on recorded host |

## Operability

The dependency-free path builds with GCC 13.3 and runs the deterministic asynchronous capture demo,
all 17 tests, both benchmarks, ASan/UBSan, and TSan. The demo accepts 1–1000 frames and reports frame
identity, checksum, FPS, latency percentiles, request outcomes, and buffer-pool usage.

The OpenCV source path is designed for CMake environments with OpenCV `core`, `imgproc`, and `videoio`.
It supports `--source webcam --device N` and `--video FILE`. No camera hardware or OpenCV development
package exists in the managed verification runtime, so hardware capture is not represented as locally
validated. The GitHub Actions release job installs `libopencv-dev`, compiles the adapter, and runs its
non-hardware test. A real webcam/video smoke run remains a host-level acceptance check.

## Known scope boundaries

- This remains a Camera HAL-inspired simulator, not Android Camera HAL or Pixel hardware integration.
- The current worker preserves capture order; it is not yet a multi-worker processing scheduler.
- Image-processing functions are real CPU implementations but are not yet a dynamically configured
  per-request processing graph.
- Benchmark results describe the recorded host and workloads only.
