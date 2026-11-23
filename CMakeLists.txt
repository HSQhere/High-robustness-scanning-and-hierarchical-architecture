cmake_minimum_required(VERSION 3.16)
project(qrscan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# OpenCV 来源可替换；本机示例：
#   -DOpenCV_DIR=C:/vcpkg/installed/x64-mingw-dynamic/share/opencv4
if(NOT OpenCV_DIR AND DEFINED ENV{QRSCAN_OPENCV_DIR})
  set(OpenCV_DIR "$ENV{QRSCAN_OPENCV_DIR}")
endif()
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV: ${OpenCV_VERSION} (${OpenCV_DIR})")

# 构建时记录 commit；没有 git 时写 unknown。
execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE QRSCAN_GIT_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
if(NOT QRSCAN_GIT_COMMIT)
  set(QRSCAN_GIT_COMMIT "unknown")
endif()

add_library(qrscan_common INTERFACE)
target_include_directories(qrscan_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/common/include)

add_library(qrscan_binarize STATIC
  layers/04_binarize/src/scale_estimator_scanline.cpp
  layers/04_binarize/src/scale_estimator_canny.cpp
  layers/04_binarize/src/binarizer.cpp
)
target_include_directories(qrscan_binarize PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include
  ${CMAKE_CURRENT_SOURCE_DIR}/layers/04_binarize/include
)
target_link_libraries(qrscan_binarize PUBLIC qrscan_common ${OpenCV_LIBS})
target_compile_definitions(qrscan_binarize PUBLIC
  QRSCAN_GIT_COMMIT="${QRSCAN_GIT_COMMIT}"
  QRSCAN_VERSION="0.1.0"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(qrscan_binarize PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(qrscan_cli apps/qrscan_cli/src/main.cpp)
target_link_libraries(qrscan_cli PRIVATE qrscan_binarize)

add_executable(qrscan_eval apps/qrscan_eval/src/main.cpp)
target_link_libraries(qrscan_eval PRIVATE qrscan_binarize)

enable_testing()

option(QRSCAN_ENABLE_GEOMETRY "Build the native-resolution ROI crop helper" OFF)
if(QRSCAN_ENABLE_GEOMETRY)
  add_subdirectory(layers/02_geometry)
endif()

add_executable(test_binarize layers/04_binarize/tests/test_binarize.cpp)
target_link_libraries(test_binarize PRIVATE qrscan_binarize)
target_compile_definitions(test_binarize PRIVATE
  QRSCAN_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/layers/04_binarize/data/input/qr_samples"
)
add_test(NAME binarize_tests COMMAND test_binarize)
