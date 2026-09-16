# Verification record

## Current release

Environment: Ubuntu 24.04 runtime, GCC 13.3.0, C++20.

| Check | Result |
|---|---|
| Warning-clean direct build (`-Wall -Wextra -Wpedantic -Wconversion -Wshadow`) | Pass |
| Behavioral test suite | 21/21 pass |
| Lifecycle regression tests | Pass: blocked pool cancellation, callback shutdown, restart rejection |
| 10,000-frame soak | Pass: 10,000 completed; 0 failures, rejects, drops, or callback failures |
| Metrics memory bound | Pass: 4,096 retained samples after 10,000 captures |
| AddressSanitizer + UndefinedBehaviorSanitizer | Pass with leak detection disabled |
| ThreadSanitizer | Pass |
| Deterministic CLI capture | Pass |
| CMake/CTest execution | Pass: Release build with OpenCV, 3/3 CTest targets |
| Generated-video OpenCV integration | Pass: generated MJPG AVI opened and captured as a 32x24 RGB frame |
| GitHub Actions run | Pass: PR #1 run #3; build-test, ASan/UBSan, and TSan jobs succeeded |
| Buffer and processing benchmarks | Pass; results recorded in `docs/performance.md` |

LeakSanitizer cannot inspect `/proc` under this managed runtime. ASan/UBSan were therefore rerun with
`ASAN_OPTIONS=detect_leaks=0`. A full leak check remains required in CI or a normal Linux host. This
is an environment limitation, not a claimed pass.

The recorded non-sanitized soak completed in 0.060 seconds on this runtime. ASan/UBSan completed it
in 0.104 seconds and TSan in 0.245 seconds. Timing is diagnostic only, not a performance claim.
