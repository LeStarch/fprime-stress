####
# DoomSubtopologyConfig: parameters and allocator wiring used by the
# Doom subtopology.
####
register_fprime_config(
    EXCLUDE_FROM_ALL
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/DoomSubtopologyConfig.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/DoomSubtopologyConfig.hpp"
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/DoomSubtopologyConfig.fpp"
    DEPENDS
        Fw_Types
)
