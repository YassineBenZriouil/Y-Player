# Shared compile settings for every Y-Player target.

if(YP_CLANG_TIDY)
  find_program(YP_CLANG_TIDY_EXE NAMES clang-tidy HINTS "$ENV{ProgramFiles}/LLVM/bin" "C:/Program Files/LLVM/bin" REQUIRED)
  message(STATUS "clang-tidy: ${YP_CLANG_TIDY_EXE}")
  # AUTOMOC output lives in the build tree; this config switches clang-tidy off for it.
  file(WRITE "${CMAKE_BINARY_DIR}/.clang-tidy" "Checks: '-*,misc-definitions-in-headers'\n")
endif()

function(yp_target_defaults target)
  target_compile_features(${target} PUBLIC cxx_std_20)
  target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}")
  target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
  target_compile_definitions(${target} PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN QT_NO_KEYWORDS
    YP_VERSION="${PROJECT_VERSION}")
  if(YP_CLANG_TIDY)
    set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${YP_CLANG_TIDY_EXE}")
  endif()
endfunction()
