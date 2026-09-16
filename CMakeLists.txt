cmake_minimum_required(VERSION 3.20)

project(mini_camera_hal VERSION 0.3.0 LANGUAGES CXX)

option(MCH_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(MCH_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(MCH_ENABLE_OPENCV "Build the OpenCV camera adapter when OpenCV is available" ON)

if(MCH_ENABLE_ASAN AND MCH_ENABLE_TSAN)
  message(FATAL_ERROR "ASan and TSan must be enabled in separate builds")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(mch_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  endif()
endfunction()

function(mch_set_sanitizers target)
  if(MCH_ENABLE_ASAN)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  elseif(MCH_ENABLE_TSAN)
    target_compile_options(${target} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=thread)
  endif()
endfunction()

add_library(camera_core
  src/AsyncCameraSession.cpp
  src/BufferPool.cpp
  src/CameraSession.cpp
  src/FrameBuffer.cpp
  src/ImageProcessing.cpp
  src/MetricsCollector.cpp
  src/MockCameraDevice.cpp
  src/RequestQueue.cpp
)
target_include_directories(camera_core PUBLIC include)
find_package(Threads REQUIRED)
target_link_libraries(camera_core PUBLIC Threads::Threads)
mch_set_warnings(camera_core)
mch_set_sanitizers(camera_core)

if(MCH_ENABLE_OPENCV)
  find_package(OpenCV QUIET COMPONENTS core imgproc videoio)
  if(OpenCV_FOUND)
    target_sources(camera_core PRIVATE src/OpenCVCameraDevice.cpp)
    target_include_directories(camera_core PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(camera_core PUBLIC ${OpenCV_LIBS})
    target_compile_definitions(camera_core PUBLIC MCH_HAS_OPENCV=1)
    message(STATUS "OpenCV camera adapter enabled")
  else()
    message(STATUS "OpenCV not found; camera adapter disabled")
  endif()
endif()

add_executable(mini_camera_hal src/main.cpp)
target_link_libraries(mini_camera_hal PRIVATE camera_core)
mch_set_warnings(mini_camera_hal)
mch_set_sanitizers(mini_camera_hal)

include(CTest)
if(BUILD_TESTING)
  add_executable(camera_tests tests/camera_tests.cpp)
  target_link_libraries(camera_tests PRIVATE camera_core)
  mch_set_warnings(camera_tests)
  mch_set_sanitizers(camera_tests)
  add_test(NAME camera_tests COMMAND camera_tests)
endif()

add_executable(buffer_benchmark benchmarks/buffer_benchmark.cpp)
target_link_libraries(buffer_benchmark PRIVATE camera_core)
mch_set_warnings(buffer_benchmark)
mch_set_sanitizers(buffer_benchmark)

add_executable(processing_benchmark benchmarks/processing_benchmark.cpp)
target_link_libraries(processing_benchmark PRIVATE camera_core)
mch_set_warnings(processing_benchmark)
mch_set_sanitizers(processing_benchmark)
