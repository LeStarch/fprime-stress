####
# DoomSubtopologyConfig: allocator wiring used by the Doom subtopology.
# FPP constants live in Doom/DoomConfig/DoomConfig.fpp.
####
register_fprime_config(
    EXCLUDE_FROM_ALL
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/DoomSubtopologyConfig.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/DoomSubtopologyConfig.hpp"
    DEPENDS
        Fw_Types
)
