qt_add_executable(y-player WIN32 main.cpp BenchMode.cpp BenchMode.h y-player.rc)
qt_add_resources(y-player "branding" PREFIX "/icons" BASE resources FILES resources/y-player.png)
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

# Translations (ADR-019): i18n/*.ts compiled to .qm and embedded at :/i18n. `cmake --build . --target
# update_translations` refreshes the .ts from the sources; i18n/fill_fr.py re-applies the French text.
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS y-player
  SOURCE_TARGETS y-player yp_ui yp_core yp_engine_mpv yp_platform_win
  TS_FILES "${PROJECT_SOURCE_DIR}/i18n/yplayer_fr.ts"
  RESOURCE_PREFIX "/i18n"
  LUPDATE_OPTIONS -no-obsolete -locations relative)
