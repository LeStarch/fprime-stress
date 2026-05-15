####
# Doom component: wraps the open-source DOOM engine in a single F Prime
# active component. The upstream DOOM C source (located in doomgeneric/)
# is vendored untouched and compiled into a static library, which the
# component links against. The component itself provides the DG_* hooks
# the engine expects (DG_Init, DG_DrawFrame, DG_SleepMs, DG_GetTicksMs,
# DG_GetKey, DG_SetWindowTitle).
####

# ----------------------------------------------------------------------
# Upstream DOOM source compiled as a separate static library so we can
# scope the relaxed warning flags it requires to that target alone.
# ----------------------------------------------------------------------
set(DOOMGENERIC_DIR "${CMAKE_CURRENT_LIST_DIR}/doomgeneric")
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
    "${DOOMGENERIC_DIR}/w_file.c"
    "${DOOMGENERIC_DIR}/w_file_stdc.c"
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
# We force RESX/RESY to the canonical 320x200 so the screen buffer is
# small and matches the FRAME_WIDTH/FRAME_HEIGHT constants in Doom.fpp.
target_compile_definitions(fprime_stress_doomgeneric
    PUBLIC
        CMAP256
        DOOMGENERIC_RESX=320
        DOOMGENERIC_RESY=200
        NORMALUNIX
        LINUX
        _DEFAULT_SOURCE
)
# Disable warnings that would otherwise fire on the upstream source.
# We must not modify the upstream code so we silence at the compiler
# level for this target only.
target_compile_options(fprime_stress_doomgeneric
    PRIVATE
        -w
)

# ----------------------------------------------------------------------
# F Prime Doom component
# ----------------------------------------------------------------------
set(SOURCE_FILES
    "${CMAKE_CURRENT_LIST_DIR}/Doom.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/DoomEngine.cpp"
)
set(HEADER_FILES
    "${CMAKE_CURRENT_LIST_DIR}/DoomEngine.hpp"
)
set(MOD_DEPS
    Os
    Fw_Logger
    fprime_stress_doomgeneric
)
register_fprime_module()

# Unit tests live under test/ut/ and are registered when the test
# subdirectory exists. Keep this guarded so a fresh check-out without
# tests still builds the production target.
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/test/ut/DoomEngineTester.cpp")
    set(UT_SOURCE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/Doom.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/DoomEngineTester.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/DoomTestMain.cpp"
    )
    set(UT_HEADER_FILES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/DoomEngineTester.hpp"
    )
    set(UT_AUTO_HELPERS ON)
    register_fprime_ut()
endif()
