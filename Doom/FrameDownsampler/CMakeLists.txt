####
# FrameDownsampler component: passive in-place frame decimator sitting
# between the DoomEngine's raw frame output and telemetry processing.
####

set(SOURCE_FILES
    "${CMAKE_CURRENT_LIST_DIR}/FrameDownsampler.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/FrameDownsampler.cpp"
)
set(HEADER_FILES
    "${CMAKE_CURRENT_LIST_DIR}/FrameDownsampler.hpp"
)
register_fprime_module()

if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameDownsamplerTester.cpp")
    set(UT_SOURCE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/FrameDownsampler.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameDownsamplerTester.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameDownsamplerTests.cpp"
    )
    set(UT_HEADER_FILES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameDownsamplerTester.hpp"
    )
    set(UT_AUTO_HELPERS ON)
    register_fprime_ut()
endif()
