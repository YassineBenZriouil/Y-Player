add_library(yp_core STATIC
  Logging.cpp Logging.h
  PlaybackEngine.h
  PlaybackStateMachine.cpp PlaybackStateMachine.h
  TimeFormat.cpp TimeFormat.h)
target_link_libraries(yp_core PUBLIC Qt6::Core)
yp_target_defaults(yp_core)
