# ======================================================================
# DoomConfig.fpp
# Single compile-time configuration file for the DOOM stress-test
# library. Deployments override this file (register_fprime_config) to
# retune the pipeline for their bandwidth and memory budgets.
# ======================================================================

module Doom {

  # ----------------------------------------------------------------------
  # Engine geometry. DOOM renders 320x200 natively; doomgeneric can
  # upscale that by an integer factor (DOOM_FRAME_SCALE, a CMake cache
  # variable read by Doom/DoomEngine/CMakeLists.txt, default 1). These
  # constants must equal 320 * DOOM_FRAME_SCALE x 200 * DOOM_FRAME_SCALE;
  # DoomEngine.cpp static_asserts the match. Keep the native geometry on
  # small targets: the upscale writes FRAME_WIDTH * FRAME_HEIGHT bytes per
  # frame and can cost more than the renderer itself when the frame lives
  # in slow external memory.
  # ----------------------------------------------------------------------

  @ Width of the full-resolution DOOM frame in pixels (320 * DOOM_FRAME_SCALE).
  constant FRAME_WIDTH = 320

  @ Height of the full-resolution DOOM frame in scanlines (200 * DOOM_FRAME_SCALE).
  constant FRAME_HEIGHT = 200

  @ Number of palette bytes (256 entries * 3 bytes per RGB triple).
  constant PALETTE_BYTES = 768

  # ----------------------------------------------------------------------
  # Downsampling. The factor is fixed at build time so the FrameRow
  # pixel array (and therefore the on-wire row size) is exactly the
  # downsampled width: size for the available bandwidth here, and let
  # the com layer (e.g. ComQueue) shed frames if bandwidth drops.
  # ----------------------------------------------------------------------

  @ Downsample factor applied to each frame dimension. Must divide
  @ FRAME_WIDTH and FRAME_HEIGHT evenly: 1, 2, 4, 8, or 16 (at the
  @ native 320x200 geometry: 1, 2, 4 or 8).
  constant DOWNSAMPLE_FACTOR = 1

  @ Width in pixels of the downsampled frame (and of each FrameRow).
  constant DOWNSAMPLED_WIDTH = FRAME_WIDTH / DOWNSAMPLE_FACTOR

  @ Height in scanlines of the downsampled frame: FrameRow channels
  @ 0 .. DOWNSAMPLED_HEIGHT-1 are emitted per frame.
  constant DOWNSAMPLED_HEIGHT = FRAME_HEIGHT / DOWNSAMPLE_FACTOR

  # ----------------------------------------------------------------------
  # Engine memory
  # ----------------------------------------------------------------------

  @ Capacity (in frames) of the screen-wipe melt playback ring. The
  @ melt animation spans roughly 40-70 engine draws; each slot costs
  @ FRAME_WIDTH * FRAME_HEIGHT bytes (80 -> 5 MB at 320x200). Reduce on
  @ memory-constrained systems: overflowing frames are dropped and
  @ counted, and the wipe cuts to the live frame early.
  constant MELT_QUEUE_CAPACITY = 80

}

module DoomSubtopologyConfig {

  @ Base ID for the Doom subtopology. Deployments should offset other
  @ subtopologies away from this range. The "0D" prefix is chosen so
  @ the slot reads as "DOOM" in hex.
  constant BASE_ID = 0x0D000000

  @ BufferManager pool sizing for the Doom subtopology.
  module BuffMgr {
    @ Per-buffer size of the general-purpose Doom pool.
    constant doomBuffSize  = 4096
    @ Number of buffers in the general-purpose Doom pool.
    constant doomBuffCount = 16
    @ BufferManager identifier (unique within the deployment).
    constant doomBuffMgrId = 0x0D
  }

}
