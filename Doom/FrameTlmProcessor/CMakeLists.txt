####
# FrameTlmProcessor component: passive telemetry processor emitting one
# FrameRow telemetry channel per scanline of the downsampled frame.
####

set(SOURCE_FILES
    "${CMAKE_CURRENT_LIST_DIR}/FrameTlmProcessor.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/FrameTlmProcessor.cpp"
)
set(HEADER_FILES
    "${CMAKE_CURRENT_LIST_DIR}/FrameTlmProcessor.hpp"
)
register_fprime_module()

if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameTlmProcessorTester.cpp")
    set(UT_SOURCE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/FrameTlmProcessor.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameTlmProcessorTester.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameTlmProcessorTests.cpp"
    )
    set(UT_HEADER_FILES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameTlmProcessorTester.hpp"
    )
    set(UT_AUTO_HELPERS ON)
    register_fprime_ut()
endif()
