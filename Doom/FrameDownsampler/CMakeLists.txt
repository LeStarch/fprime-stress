####
# FrameDownsampler component: passive in-place frame decimator sitting
# between the DoomEngine's raw frame output and telemetry processing.
####

register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/FrameDownsampler.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/FrameDownsampler.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/FrameDownsampler.hpp"
)

### UTs ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/FrameDownsampler.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameDownsamplerTester.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameDownsamplerTests.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameDownsamplerTester.hpp"
    UT_AUTO_HELPERS
)
