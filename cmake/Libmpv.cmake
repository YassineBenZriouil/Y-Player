# Imported target Mpv::Mpv for the prebuilt libmpv in deps/mpv (fetched by scripts/fetch_deps.py).

set(YP_MPV_DIR "${PROJECT_SOURCE_DIR}/deps/mpv" CACHE PATH "Directory containing libmpv-2.dll and include/mpv")
set(_mpv_dll "${YP_MPV_DIR}/libmpv-2.dll")
if(NOT EXISTS "${_mpv_dll}")
  message(FATAL_ERROR "libmpv not found in ${YP_MPV_DIR}. Run: python scripts/fetch_deps.py")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_mpv_dll}")

# The package ships a MinGW import lib (.dll.a); build an MSVC one from the DLL's exports.
set(_mpv_implib "${CMAKE_BINARY_DIR}/libmpv/mpv.lib")
if(NOT EXISTS "${_mpv_implib}" OR "${_mpv_dll}" IS_NEWER_THAN "${_mpv_implib}")
  get_filename_component(_msvc_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
  find_program(YP_DUMPBIN dumpbin HINTS "${_msvc_bin}" REQUIRED)
  find_program(YP_LIB_TOOL lib HINTS "${_msvc_bin}" REQUIRED)
  execute_process(COMMAND "${YP_DUMPBIN}" /exports "${_mpv_dll}" OUTPUT_VARIABLE _dump COMMAND_ERROR_IS_FATAL ANY)
  string(REGEX MATCHALL "[ \t][0-9A-F]+[ \t]+(mpv_[A-Za-z0-9_]+)" _lines "${_dump}")
  set(_def "LIBRARY libmpv-2.dll\nEXPORTS\n")
  foreach(_line IN LISTS _lines)
    string(REGEX REPLACE ".*[ \t](mpv_[A-Za-z0-9_]+)$" "\\1" _sym "${_line}")
    string(APPEND _def "  ${_sym}\n")
  endforeach()
  if(NOT _lines)
    message(FATAL_ERROR "No mpv_* exports found in ${_mpv_dll}")
  endif()
  file(WRITE "${CMAKE_BINARY_DIR}/libmpv/mpv.def" "${_def}")
  execute_process(
    COMMAND "${YP_LIB_TOOL}" /nologo /machine:x64 "/def:${CMAKE_BINARY_DIR}/libmpv/mpv.def" "/out:${_mpv_implib}"
    OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
endif()

add_library(Mpv::Mpv SHARED IMPORTED)
set_target_properties(Mpv::Mpv PROPERTIES
  IMPORTED_LOCATION "${_mpv_dll}"
  IMPORTED_IMPLIB "${_mpv_implib}"
  INTERFACE_INCLUDE_DIRECTORIES "${YP_MPV_DIR}/include")

# Everything runs from bin/, so the DLL lives there for the app and the tests.
file(COPY "${_mpv_dll}" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
