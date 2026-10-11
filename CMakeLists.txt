cmake_minimum_required(VERSION 3.25)
project(YPlayer VERSION 0.1.0 DESCRIPTION "High-performance Windows video player" LANGUAGES CXX)

if(NOT MSVC)
  message(FATAL_ERROR "Y-Player builds with MSVC only (DECISIONS.md ADR-004)")
endif()

option(YP_BUILD_TESTS "Build unit tests" ON)
option(YP_CLANG_TIDY "Run clang-tidy on project sources (warnings are errors)" OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup()

include(cmake/YpTargetDefaults.cmake)
include(cmake/Libmpv.cmake)

add_subdirectory(core)
add_subdirectory(engine/mpv)
add_subdirectory(platform/win)
add_subdirectory(ui)
add_subdirectory(app)

if(YP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
