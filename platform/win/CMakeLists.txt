add_library(yp_platform_win STATIC
  SystemInfo.cpp SystemInfo.h
  WindowStyle.cpp WindowStyle.h)
target_link_libraries(yp_platform_win PUBLIC Qt6::Core PRIVATE dxgi)
yp_target_defaults(yp_platform_win)
