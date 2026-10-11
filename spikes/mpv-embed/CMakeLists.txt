cmake_minimum_required(VERSION 3.25)
project(mpv_embed_spike CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)

set(MPV_DIR "${CMAKE_CURRENT_LIST_DIR}/../../deps/mpv")  # fetched by scripts/fetch_deps.py

# libmpv ships a MinGW import lib; generate an MSVC one from the DLL's exports.
set(MPV_LIB "${CMAKE_BINARY_DIR}/mpv.lib")
if(NOT EXISTS "${MPV_LIB}")
  execute_process(COMMAND dumpbin /exports "${MPV_DIR}/libmpv-2.dll" OUTPUT_VARIABLE dump)
  string(REGEX MATCHALL "[ \t]+[0-9]+[ \t]+[0-9A-F]+[ \t]+[0-9A-F]+[ \t]+(mpv_[A-Za-z0-9_]+)" lines "${dump}")
  set(def "LIBRARY libmpv-2.dll\nEXPORTS\n")
  foreach(l IN LISTS lines)
    string(REGEX REPLACE ".*[ \t](mpv_[A-Za-z0-9_]+)$" "\\1" sym "${l}")
    string(APPEND def "  ${sym}\n")
  endforeach()
  file(WRITE "${CMAKE_BINARY_DIR}/mpv.def" "${def}")
  execute_process(COMMAND lib /nologo /machine:x64 "/def:${CMAKE_BINARY_DIR}/mpv.def" "/out:${MPV_LIB}" COMMAND_ERROR_IS_FATAL ANY)
endif()

add_executable(mpv-embed-spike main.cpp)
target_include_directories(mpv-embed-spike PRIVATE "${MPV_DIR}/include")
target_link_libraries(mpv-embed-spike PRIVATE Qt6::Widgets Qt6::OpenGLWidgets "${MPV_LIB}" psapi)
target_compile_options(mpv-embed-spike PRIVATE /W4 /permissive-)

add_custom_command(TARGET mpv-embed-spike POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MPV_DIR}/libmpv-2.dll" $<TARGET_FILE_DIR:mpv-embed-spike>)
