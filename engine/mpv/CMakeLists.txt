# libmpv backend. MpvEngine (implements yp::IPlaybackEngine) arrives in Phase 2.
add_library(yp_engine_mpv STATIC MpvInfo.cpp MpvInfo.h)
target_link_libraries(yp_engine_mpv PUBLIC yp_core PRIVATE Mpv::Mpv)
yp_target_defaults(yp_engine_mpv)
