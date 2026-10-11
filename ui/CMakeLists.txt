add_library(yp_ui STATIC
  MainWindow.cpp MainWindow.h
  SeekSlider.cpp SeekSlider.h
  VideoWidget.cpp VideoWidget.h)
target_link_libraries(yp_ui PUBLIC Qt6::Widgets yp_core)
yp_target_defaults(yp_ui)
