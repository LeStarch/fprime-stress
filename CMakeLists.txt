####
# fprime-stress: an out-of-tree F Prime library containing a single
# passive component that wraps the open-source DOOM engine, plus a
# DoomSubtopology that lets any deployment absorb the whole
# subsystem in one `instance Doom.Subtopology` declaration.
#
# This is library-only - it does NOT register a deployment. To
# exercise it as a runnable binary use the companion deployment in
# https://github.com/JPL-Devin/fprime-stress-reference.
####
cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0048 NEW)

project(fprime_stress
    VERSION 0.1.0
    LANGUAGES C CXX
)

# Resolve the F Prime framework. When loaded via `add_fprime_subdirectory`
# from a parent deployment, FPRIME_FRAMEWORK_PATH is already set and we
# can skip the explicit find_package step.
if (NOT FPRIME_FRAMEWORK_PATH)
    if (NOT DEFINED FPRIME_LIBRARY_AS_TOPLEVEL)
        set(FPRIME_LIBRARY_AS_TOPLEVEL ON)
    endif()
    find_package(FPrime REQUIRED)
endif()

# Both modules sit at the project root - register them on the F Prime
# configuration before any deployment hooks them up.
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Doom/")
