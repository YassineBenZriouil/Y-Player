find_package(Qt6 REQUIRED COMPONENTS Test)

function(yp_add_test name)
  qt_add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE Qt6::Test ${ARGN})
  yp_target_defaults(${name})
  add_test(NAME ${name} COMMAND ${name})
  # Qt DLLs are only deployed next to the app; point tests at the Qt bin dir instead.
  set_tests_properties(${name} PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
endfunction()

yp_add_test(tst_logging yp_core)
yp_add_test(tst_mpvinfo yp_engine_mpv)
yp_add_test(tst_systeminfo yp_platform_win)
