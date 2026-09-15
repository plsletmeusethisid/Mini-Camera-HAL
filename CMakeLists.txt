cmake_minimum_required(VERSION 3.20)

project(mini_camera_hal VERSION 0.1.0 LANGUAGES CXX)

option(MCH_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(MCH_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

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
  src/CameraSession.cpp
  src/FrameBuffer.cpp
  src/MockCameraDevice.cpp
  src/RequestQueue.cpp
)
target_include_directories(camera_core PUBLIC include)
find_package(Threads REQUIRED)
target_link_libraries(camera_core PUBLIC Threads::Threads)
mch_set_warnings(camera_core)
mch_set_sanitizers(camera_core)

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
