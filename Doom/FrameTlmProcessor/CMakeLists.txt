####
# FrameTlmProcessor component: passive telemetry processor emitting one
# FrameRow telemetry channel per scanline of the downsampled frame.
####

register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/FrameTlmProcessor.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/FrameTlmProcessor.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/FrameTlmProcessor.hpp"
)

### UTs ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/FrameTlmProcessor.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameTlmProcessorTester.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameTlmProcessorTests.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameTlmProcessorTester.hpp"
    UT_AUTO_HELPERS
)
