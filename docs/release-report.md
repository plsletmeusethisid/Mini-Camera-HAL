# Feature completion report

This release implements the feature set and lifecycle hardening required to make Mini Camera HAL a
stronger camera and native-systems portfolio project.

| Requirement | Implementation | Evidence |
|---|---|---|
| Lifecycle safety | Explicit single-use state machine; callback-safe join ownership | Three lifecycle regression tests |
| Shutdown cancellation | Session shutdown propagates to blocked buffer acquisition | Exhausted-pool regression test |
| Reusable buffer pool | Fixed-capacity `BufferPool`; RAII leases use a custom deleter and shared pool state | Reuse, high-water, outliving-facade, and blocked-shutdown tests |
| Allocation benchmark | `buffer_benchmark` compares per-frame construction and pooled reuse at 1080p | Raw five-run record plus median analysis in `performance.md` |
| Image processing | Manual RGB-to-gray, lookup-table gamma, and bilinear resize | Known-value and shape tests; processing benchmark |
| OpenCV adapter | PImpl-based webcam/video adapter fills ordinary or pooled buffers | Optional CMake target; adapter compile/missing-file test in CI |
| Runtime metrics | Thread-safe FPS and rolling-window mean/P50/P95/P99/max latency; pool high-water statistics | Percentile and bounded-memory tests |
| GitHub Actions | Release build with OpenCV, CTest, benchmark smoke; ASan/UBSan and TSan jobs | `.github/workflows/ci.yml` |
| Integration and soak | Generated MJPG input test and 10,000-frame async run | CTest targets with timeouts |
| Measured optimization | Buffer reuse and fixed-point grayscale | 2.05x full-frame-fill and 4.14x grayscale speedups on recorded host |

## Operability

The dependency-free path builds with GCC 13.3 and runs the deterministic asynchronous capture demo,
all 21 behavioral tests, the 10,000-frame soak, both benchmarks, ASan/UBSan, and TSan. The demo accepts 1–1000 frames and reports frame
identity, checksum, FPS, latency percentiles, request outcomes, and buffer-pool usage.

The OpenCV source path is designed for CMake environments with OpenCV `core`, `imgproc`, and `videoio`.
It supports `--source webcam --device N` and `--video FILE`. No camera hardware or OpenCV development
package exists in the managed verification runtime, so OpenCV compilation is not represented as
locally validated. The GitHub Actions release job installs `libopencv-dev`, compiles the adapter, and
runs an integration test that generates an MJPG video, opens it through the adapter, and validates a
captured RGB frame. A real webcam smoke run remains a host-level acceptance check.

The async lifecycle is explicit and single-use. Shutdown cancels blocked buffer leases, can be
requested from the result callback without self-joining, and is covered by regression tests for both
paths. Restart is intentionally rejected and documented rather than partially supported.

## Known scope boundaries

- This remains a Camera HAL-inspired simulator, not Android Camera HAL or Pixel hardware integration.
- The current worker preserves capture order; it is not yet a multi-worker processing scheduler.
- Image-processing functions are real CPU implementations but are not yet a dynamically configured
  per-request processing graph.
- Benchmark results describe the recorded host and workloads only.
