qt_add_executable(y-player WIN32 main.cpp)
set_target_properties(y-player PROPERTIES OUTPUT_NAME "Y-Player")
target_link_libraries(y-player PRIVATE yp_core yp_engine_mpv yp_platform_win yp_ui)
yp_target_defaults(y-player)

# Make bin/ runnable in place: Qt DLLs + plugins next to the exe (libmpv is copied by Libmpv.cmake).
get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin "${_qmake}" DIRECTORY)
add_custom_command(TARGET y-player POST_BUILD
  COMMAND "${_qt_bin}/windeployqt.exe" --no-translations --no-system-d3d-compiler --no-opengl-sw
          --no-compiler-runtime --verbose 0 "$<TARGET_FILE:y-player>"
  VERBATIM)
