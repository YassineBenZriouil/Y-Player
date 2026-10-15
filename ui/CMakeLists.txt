add_library(yp_ui STATIC
  DiagnosticsDialog.cpp DiagnosticsDialog.h
  Icons.cpp Icons.h
  MainWindow.cpp MainWindowMenus.cpp MainWindow.h
  PlaylistModel.cpp PlaylistModel.h
  PlaylistPanel.cpp PlaylistPanel.h
  SeekSlider.cpp SeekSlider.h
  SettingsDialog.cpp SettingsDialog.h
  ShortcutDialog.cpp ShortcutDialog.h
  ShortcutMap.cpp ShortcutMap.h
  SubtitleStyleDialog.cpp SubtitleStyleDialog.h
  VideoWidget.cpp VideoWidget.h)
target_link_libraries(yp_ui PUBLIC Qt6::Widgets yp_core PRIVATE yp_platform_win)
yp_target_defaults(yp_ui)
