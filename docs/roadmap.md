# Build roadmap

The demo is built as verified vertical slices. Every milestone must compile, run, and leave the main
branch in a demonstrable state.

| Milestone | Demonstration | Exit criteria |
|---|---|---|
| 1. Synchronous foundation | C++20 API design, RAII, request validation, deterministic capture | CLI captures 100 frames; tests pass; ASan clean |
| 2. Async request/result | Producer-consumer concurrency and safe shutdown | **Implemented:** callback API, bounded request queue, draining shutdown; TSan validation pending |
| 3. Buffer pool | Reuse, ownership leases, backpressure | **Implemented:** fixed-capacity pool, RAII return, statistics, benchmark |
| 4. Processing pipeline | Image memory layout and modular processing | **Implemented:** manual grayscale/gamma/resize with tests |
| 5. Camera adapters | Interface isolation and systems integration | **Implemented:** optional OpenCV webcam/video adapter without core coupling |
| 6. Preview scheduling | Repeating requests and multiple in-flight frames | Start/stop preview, one-shot interruption, documented ordering |
| 7. Metrics and benchmark | Measure-optimize-verify loop | **Implemented:** FPS, latency percentiles, pool high-water mark, checked-in runs |
| 8. Fault recovery | Production stability under partial failure | Timeout, disconnect, corruption, overflow, worker failure scenarios |
| 9. Performance study | Evidence-backed systems optimization | Profile, hypothesis, change, measured result on recorded hardware |
| 10. Portfolio release | Reviewer-friendly communication | CI, architecture docs, demo recording, honest Android concept mapping |

## Next implementation slice

Milestone 6 adds repeating preview scheduling. After that, fault injection will cover timeouts,
disconnects, corrupt frames, queue overflow, and buffer exhaustion without terminating the worker.
