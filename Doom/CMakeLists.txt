####
# Doom: shared FPP types (structs, enums, ports) used by the
# DoomEngine, FrameDownsampler, and FrameTlmProcessor components, each
# of which lives in its own subdirectory alongside the DoomSubtopology
# that wires them together. All compile-time configuration lives in
# DoomConfig/DoomConfig.fpp.
####

register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Doom.fpp"
    DEPENDS
        Doom_DoomConfig
)

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/DoomConfig/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/DoomEngine/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FrameDownsampler/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FrameTlmProcessor/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/DoomSubtopology/")
