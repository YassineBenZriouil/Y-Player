add_library(yp_platform_win STATIC
  FileAssociations.cpp FileAssociations.h
  SystemInfo.cpp SystemInfo.h
  WindowStyle.cpp WindowStyle.h)
target_link_libraries(yp_platform_win PUBLIC Qt6::Core PRIVATE dxgi shell32)
yp_target_defaults(yp_platform_win)
