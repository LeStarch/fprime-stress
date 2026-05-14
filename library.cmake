####
# library.cmake: F Prime library manifest for fprime-stress.
#
# Loaded by F Prime's `fprime_setup_included_code` when this directory
# is registered as a library_locations entry. The CMakeLists.txt at
# the repo root remains usable when this directory is opened
# standalone (e.g. for `cmake -B build` outside an F Prime
# deployment), but during a normal F Prime deployment build it's this
# manifest file that gets included so the Doom component and
# DoomSubtopology end up in the F Prime module index.
####
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Doom/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/DoomSubtopology/")
