####
# DoomConfig: single compile-time configuration file for the DOOM
# library. Deployments override DoomConfig.fpp to retune bandwidth
# (downsample factor) and memory (melt ring capacity).
####
register_fprime_config(
    EXCLUDE_FROM_ALL
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/DoomConfig.fpp"
    INTERFACE
)
