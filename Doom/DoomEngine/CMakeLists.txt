####
# DoomEngine component: wraps the open-source DOOM engine in a single
# F Prime passive component. The upstream DOOM C source (the
# doomgeneric/ git submodule, ozkl/doomgeneric) is compiled untouched
# into a static library, which the component links against. The component itself
# provides the DG_* hooks the engine expects (DG_Init, DG_DrawFrame,
# DG_SleepMs, DG_GetTicksMs, DG_GetKey, DG_SetWindowTitle).
####

# ----------------------------------------------------------------------
# Upstream DOOM source compiled as a separate static library so we can
# scope the relaxed warning flags it requires to that target alone.
# ----------------------------------------------------------------------
set(DOOMGENERIC_DIR "${CMAKE_CURRENT_LIST_DIR}/doomgeneric/doomgeneric")
set(DOOMGENERIC_SOURCES
    "${DOOMGENERIC_DIR}/am_map.c"
    "${DOOMGENERIC_DIR}/d_event.c"
    "${DOOMGENERIC_DIR}/d_items.c"
    "${DOOMGENERIC_DIR}/d_iwad.c"
    "${DOOMGENERIC_DIR}/d_loop.c"
    "${DOOMGENERIC_DIR}/d_main.c"
    "${DOOMGENERIC_DIR}/d_mode.c"
    "${DOOMGENERIC_DIR}/d_net.c"
    "${DOOMGENERIC_DIR}/doomdef.c"
    "${DOOMGENERIC_DIR}/doomgeneric.c"
    "${DOOMGENERIC_DIR}/doomstat.c"
    "${DOOMGENERIC_DIR}/dstrings.c"
    "${DOOMGENERIC_DIR}/dummy.c"
    "${DOOMGENERIC_DIR}/f_finale.c"
    "${DOOMGENERIC_DIR}/f_wipe.c"
    "${DOOMGENERIC_DIR}/g_game.c"
    "${DOOMGENERIC_DIR}/gusconf.c"
    "${DOOMGENERIC_DIR}/hu_lib.c"
    "${DOOMGENERIC_DIR}/hu_stuff.c"
    "${DOOMGENERIC_DIR}/i_cdmus.c"
    "${DOOMGENERIC_DIR}/i_endoom.c"
    "${DOOMGENERIC_DIR}/i_input.c"
    "${DOOMGENERIC_DIR}/i_joystick.c"
    "${DOOMGENERIC_DIR}/i_scale.c"
    "${DOOMGENERIC_DIR}/i_sound.c"
    "${DOOMGENERIC_DIR}/i_system.c"
    "${DOOMGENERIC_DIR}/i_timer.c"
    "${DOOMGENERIC_DIR}/i_video.c"
    "${DOOMGENERIC_DIR}/icon.c"
    "${DOOMGENERIC_DIR}/info.c"
    "${DOOMGENERIC_DIR}/m_argv.c"
    "${DOOMGENERIC_DIR}/m_bbox.c"
    "${DOOMGENERIC_DIR}/m_cheat.c"
    "${DOOMGENERIC_DIR}/m_config.c"
    "${DOOMGENERIC_DIR}/m_controls.c"
    "${DOOMGENERIC_DIR}/m_fixed.c"
    "${DOOMGENERIC_DIR}/m_menu.c"
    "${DOOMGENERIC_DIR}/m_misc.c"
    "${DOOMGENERIC_DIR}/m_random.c"
    "${DOOMGENERIC_DIR}/memio.c"
    "${DOOMGENERIC_DIR}/mus2mid.c"
    "${DOOMGENERIC_DIR}/p_ceilng.c"
    "${DOOMGENERIC_DIR}/p_doors.c"
    "${DOOMGENERIC_DIR}/p_enemy.c"
    "${DOOMGENERIC_DIR}/p_floor.c"
    "${DOOMGENERIC_DIR}/p_inter.c"
    "${DOOMGENERIC_DIR}/p_lights.c"
    "${DOOMGENERIC_DIR}/p_map.c"
    "${DOOMGENERIC_DIR}/p_maputl.c"
    "${DOOMGENERIC_DIR}/p_mobj.c"
    "${DOOMGENERIC_DIR}/p_plats.c"
    "${DOOMGENERIC_DIR}/p_pspr.c"
    "${DOOMGENERIC_DIR}/p_saveg.c"
    "${DOOMGENERIC_DIR}/p_setup.c"
    "${DOOMGENERIC_DIR}/p_sight.c"
    "${DOOMGENERIC_DIR}/p_spec.c"
    "${DOOMGENERIC_DIR}/p_switch.c"
    "${DOOMGENERIC_DIR}/p_telept.c"
    "${DOOMGENERIC_DIR}/p_tick.c"
    "${DOOMGENERIC_DIR}/p_user.c"
    "${DOOMGENERIC_DIR}/r_bsp.c"
    "${DOOMGENERIC_DIR}/r_data.c"
    "${DOOMGENERIC_DIR}/r_draw.c"
    "${DOOMGENERIC_DIR}/r_main.c"
    "${DOOMGENERIC_DIR}/r_plane.c"
    "${DOOMGENERIC_DIR}/r_segs.c"
    "${DOOMGENERIC_DIR}/r_sky.c"
    "${DOOMGENERIC_DIR}/r_things.c"
    "${DOOMGENERIC_DIR}/s_sound.c"
    "${DOOMGENERIC_DIR}/sha1.c"
    "${DOOMGENERIC_DIR}/sounds.c"
    "${DOOMGENERIC_DIR}/st_lib.c"
    "${DOOMGENERIC_DIR}/st_stuff.c"
    "${DOOMGENERIC_DIR}/statdump.c"
    "${DOOMGENERIC_DIR}/tables.c"
    "${DOOMGENERIC_DIR}/v_video.c"
    "${DOOMGENERIC_DIR}/w_checksum.c"
    # w_file_stdc.c (C stdio) is replaced by the Os::File wad_file_class_t in WadFile.cpp
    "${DOOMGENERIC_DIR}/w_file.c"
    "${DOOMGENERIC_DIR}/w_main.c"
    "${DOOMGENERIC_DIR}/w_wad.c"
    "${DOOMGENERIC_DIR}/wi_stuff.c"
    "${DOOMGENERIC_DIR}/z_zone.c"
)

# The library target is given a fully-qualified, hyphenated name that
# does not collide with any source directory under the project root.
# This keeps fprime's `resolve_path_variables(LIST_DEPENDS)` happy:
# it must NOT find a matching directory on disk, otherwise the name
# gets rewritten to a get_module_name'd path (e.g. Doom_doomgeneric).
add_library(fprime_stress_doomgeneric STATIC ${DOOMGENERIC_SOURCES})
target_include_directories(fprime_stress_doomgeneric
    PUBLIC
        "${DOOMGENERIC_DIR}"
)
# Upstream DOOM uses CMAP256 to render a palette-indexed 8-bit frame.
# DOOM renders 320x200; doomgeneric upscales by an integer factor in
# I_FinishUpdate. DOOM_FRAME_SCALE=1 (default) keeps the native 64 kB frame;
# 2 gives 640x400 (256 kB / frame * 35 Hz = 9.0 MB/s raw) for larger
# processors. Doom.FRAME_WIDTH/HEIGHT in DoomConfig.fpp must match
# (DoomEngine.cpp static_asserts it).
set(DOOM_FRAME_SCALE "1" CACHE STRING
    "Integer upscale of DOOM's native 320x200 frame (must match Doom.FRAME_WIDTH/HEIGHT)")
if(NOT DOOM_FRAME_SCALE MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "DOOM_FRAME_SCALE must be a positive integer, got '${DOOM_FRAME_SCALE}'")
endif()
math(EXPR DOOMGENERIC_RESX "320 * ${DOOM_FRAME_SCALE}")
math(EXPR DOOMGENERIC_RESY "200 * ${DOOM_FRAME_SCALE}")
target_compile_definitions(fprime_stress_doomgeneric
    PUBLIC
        CMAP256
        DOOMGENERIC_RESX=${DOOMGENERIC_RESX}
        DOOMGENERIC_RESY=${DOOMGENERIC_RESY}
        NORMALUNIX
        LINUX
        _DEFAULT_SOURCE
)
# Disable warnings that would otherwise fire on the upstream source.
# We must not modify the upstream code so we silence at the compiler
# level for this target only. Compile the engine optimized regardless
# of the project build type: DOOM's level load and screen-wipe setup
# run inside a single game tic, and unoptimized builds can exceed the
# 28.57 ms rate-group budget at demo level transitions.
target_compile_options(fprime_stress_doomgeneric
    PRIVATE
        -w
        -O2
)
# Upstream I_Error/I_Quit exit() the process. Rename their definitions
# in i_system.c so every other engine TU links against the component's
# fault trampoline in DoomEngine.cpp instead (source stays untouched).
# Calls made inside i_system.c still reach the renamed bodies, so exit()
# itself is also redirected into the trampoline for that TU.
set_source_files_properties("${DOOMGENERIC_DIR}/i_system.c"
    PROPERTIES COMPILE_DEFINITIONS
        "I_Error=doomgeneric_upstream_I_Error;I_Quit=doomgeneric_upstream_I_Quit;exit=fprime_doom_upstream_exit"
)

# ----------------------------------------------------------------------
# F Prime DoomEngine component
# ----------------------------------------------------------------------
register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/DoomEngine.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/DoomEngine.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/WadFile.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/DoomEngine.hpp"
    DEPENDS
        Os
        Fw_Logger
        fprime_stress_doomgeneric
)

# w_file.c resolves stdc_wad_file from WadFile.cpp, so the two static libraries depend on
# each other; declaring the cycle makes CMake repeat them on the link line.
target_link_libraries(fprime_stress_doomgeneric INTERFACE Doom_DoomEngine)

### UTs ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/DoomEngine.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/DoomEngineTester.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/DoomTestMain.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/DoomEngineTester.hpp"
    UT_AUTO_HELPERS
)
