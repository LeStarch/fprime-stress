####
# DoomSubtopology: wraps the Doom component in a reusable subtopology
# block so any F Prime deployment can absorb the whole DOOM subsystem
# by adding a single `instance DoomSubtopology.Subtopology` line.
####
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/DoomSubtopologyConfig/")

register_fprime_module(
    EXCLUDE_FROM_ALL
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/DoomSubtopology.fpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/SubtopologyTopologyDefs.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/PingEntries.hpp"
    INTERFACE
    DEPENDS
        Doom_DoomConfig
        Doom_DoomEngine
        Doom_FrameDownsampler
        Doom_FrameTlmProcessor
        Doom_DoomSubtopology_DoomSubtopologyConfig
)
