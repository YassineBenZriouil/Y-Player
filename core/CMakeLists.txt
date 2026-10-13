add_library(yp_core STATIC
  Adjustments.cpp Adjustments.h
  Logging.cpp Logging.h
  PlaybackEngine.h
  PlaybackStateMachine.cpp PlaybackStateMachine.h
  Playlist.cpp Playlist.h
  PlaylistIO.cpp PlaylistIO.h
  RecentFiles.cpp RecentFiles.h
  ResumeStore.cpp ResumeStore.h
  TimeFormat.cpp TimeFormat.h)
target_link_libraries(yp_core PUBLIC Qt6::Core)
yp_target_defaults(yp_core)
