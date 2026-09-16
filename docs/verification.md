# Verification record

## Current release

Environment: Ubuntu 24.04 runtime, GCC 13.3.0, C++20.

| Check | Result |
|---|---|
| Warning-clean direct build (`-Wall -Wextra -Wpedantic -Wconversion -Wshadow`) | Pass |
| Behavioral test suite | 17/17 pass |
| AddressSanitizer + UndefinedBehaviorSanitizer | Pass with leak detection disabled |
| ThreadSanitizer | Pass |
| Deterministic CLI capture | Pass |
| CMake/CTest execution | Not run: CMake unavailable in this runtime |
| OpenCV adapter compilation | Deferred to GitHub Actions: OpenCV unavailable locally |
| Buffer and processing benchmarks | Pass; results recorded in `docs/performance.md` |

LeakSanitizer cannot inspect `/proc` under this managed runtime. ASan/UBSan were therefore rerun with
`ASAN_OPTIONS=detect_leaks=0`. A full leak check remains required in CI or a normal Linux host. This
is an environment limitation, not a claimed pass.
