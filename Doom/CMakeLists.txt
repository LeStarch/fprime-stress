####
# Doom: shared FPP types (constants, structs, enums, ports) used by the
# DoomEngine, FrameDownsampler, and FrameTlmProcessor components, each
# of which lives in its own subdirectory alongside the DoomSubtopology
# that wires them together.
####

set(SOURCE_FILES
    "${CMAKE_CURRENT_LIST_DIR}/Doom.fpp"
)
register_fprime_module()

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/DoomEngine/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FrameDownsampler/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FrameTlmProcessor/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/DoomSubtopology/")
