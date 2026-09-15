#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir="${project_dir}/out"
compiler="${CXX:-g++}"

mkdir -p "${output_dir}"

common_flags=(
  -std=c++20
  -Wall
  -Wextra
  -Wpedantic
  -Wconversion
  -Wshadow
  -I"${project_dir}/include"
)

sanitizer_flags=()
case "${MCH_SANITIZE:-none}" in
  none) ;;
  address) sanitizer_flags=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) sanitizer_flags=(-fsanitize=thread -fno-omit-frame-pointer) ;;
  *) echo "MCH_SANITIZE must be one of: none, address, thread" >&2; exit 64 ;;
esac

core_sources=(
  "${project_dir}/src/CameraSession.cpp"
  "${project_dir}/src/AsyncCameraSession.cpp"
  "${project_dir}/src/FrameBuffer.cpp"
  "${project_dir}/src/MockCameraDevice.cpp"
  "${project_dir}/src/RequestQueue.cpp"
)

"${compiler}" "${common_flags[@]}" "${sanitizer_flags[@]}" "${core_sources[@]}" \
  "${project_dir}/src/main.cpp" -pthread -o "${output_dir}/mini_camera_hal"

"${compiler}" "${common_flags[@]}" "${sanitizer_flags[@]}" "${core_sources[@]}" \
  "${project_dir}/tests/camera_tests.cpp" -pthread -o "${output_dir}/camera_tests"

"${output_dir}/camera_tests"
"${output_dir}/mini_camera_hal" --frames 3
