# Architecture

## What this project demonstrates

Mini Camera HAL is a portable camera-system simulator for demonstrating production-oriented C++
engineering: request/result APIs, explicit buffer ownership, asynchronous execution, backpressure,
latency measurement, and recovery from partial failure. It uses deterministic synthetic frames so
core behavior remains testable without camera hardware.

It is inspired by camera HAL concepts but does **not** implement Android Camera HAL, AIDL, Binder, or
device-specific drivers.

## Target architecture

```mermaid
flowchart TD
  App["Demo client"] -->|CaptureRequest| Session["CameraSession"]
  Session --> Queue["Bounded request queue"]
  Queue --> Device["Camera device adapter"]
  Device --> Pool["Reusable buffer pool"]
  Pool --> Pipeline["Processing pipeline"]
  Pipeline -->|CaptureResult callback| App
```

The synchronous engine implements this boundary:

```text
CaptureRequest -> CameraSession -> ICameraDevice -> FrameBuffer -> CaptureResult
```

That gives later concurrency work a tested semantic baseline.

`AsyncCameraSession` now places a bounded `RequestQueue` in front of that engine. A dedicated worker
drains accepted requests and moves each result into the client callback. Callback exceptions are
contained and counted so consumer failure cannot silently terminate capture.

Shutdown is draining: new submissions are rejected, blocked waiters wake, queued requests finish,
the worker joins, and only then does the device close.

## Ownership model: milestone 1

`FrameBuffer` is move-only. A device constructs and exclusively owns a buffer during capture, then
moves it into `DeviceCapture`. `CameraSession` moves it into `CaptureResult`, and the client owns the
result. There is no aliasing and destruction follows normal RAII.

The buffer-pool milestone will intentionally change this boundary to a lease whose deleter returns
the buffer to its pool. That ownership change must be benchmarked and tested rather than introduced
prematurely.

## Boundary validation

`CameraSession` validates every request before calling a device. It rejects zero/oversized
dimensions, invalid frame IDs, invalid exposure/gain, and incompatible YUV420 dimensions. Device
exceptions are converted into typed capture failures so hardware adapters cannot crash the demo.

## Design decisions

- The core currently has no OpenCV dependency. OpenCV will be isolated in a future adapter.
- Deterministic pixel generation makes correctness checks reproducible.
- Synchronous capture comes first to define behavior before thread scheduling complicates failures.
- Status codes and messages are both returned: code for control flow, message for diagnosis.
- Maximum resolution is bounded at the session boundary to prevent accidental giant allocations.
