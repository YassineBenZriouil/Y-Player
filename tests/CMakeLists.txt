find_package(Qt6 REQUIRED COMPONENTS Test)

get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_prefix "${_qmake}/../.." ABSOLUTE)

function(yp_add_test name)
  qt_add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE Qt6::Test ${ARGN})
  yp_target_defaults(${name})
  add_test(NAME ${name} COMMAND ${name})
  # Qt DLLs/plugins are only deployed next to the app; point tests at the Qt install instead.
  set_tests_properties(${name} PROPERTIES ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QT_PLUGIN_PATH=set:${_qt_prefix}/plugins")
endfunction()

yp_add_test(tst_logging yp_core)
yp_add_test(tst_playbackstate yp_core)
yp_add_test(tst_timeformat yp_core)
yp_add_test(tst_playlist yp_core)
yp_add_test(tst_playlistio yp_core)
yp_add_test(tst_library yp_core)
yp_add_test(tst_shortcutmap yp_ui)
yp_add_test(tst_mpvinfo yp_engine_mpv)
yp_add_test(tst_systeminfo yp_platform_win)

# Integration: real libmpv playback in a small native window (muted). Media lives in tests/media.
yp_add_test(tst_mpvengine yp_engine_mpv Qt6::Widgets ole32)
target_compile_definitions(tst_mpvengine PRIVATE YP_TEST_MEDIA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/media")
set_tests_properties(tst_mpvengine PROPERTIES TIMEOUT 600 LABELS integration)
