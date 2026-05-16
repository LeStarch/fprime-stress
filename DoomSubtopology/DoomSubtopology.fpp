# ======================================================================
# Doom subtopology
#
# Wraps the Doom component plus a dedicated BufferManager so that any
# deployment can drop the whole game-engine subsystem in by adding a
# single `instance DoomSubtopology.Subtopology` to its topology.
#
# Deployments are expected to wire:
#   * Subtopology.schedIn   <- a RateGroup member out port (~30 Hz)
#   * The standard F Prime command / event / telemetry / time
#     interfaces are imported by the doom instance via Fw.Command /
#     Fw.Event / Fw.Channel / time get port; the enclosing topology's
#     dispatcher / event manager / tlm chain / time provider connect
#     to those automatically.
# ======================================================================

module DoomSubtopology {

    # ------------------------------------------------------------------
    # Component instances
    # ------------------------------------------------------------------

    @ Singleton Doom engine wrapper. Passive: the component owns no
    @ thread; schedIn runs on the rate-group thread and command
    @ handlers run on the command-dispatch thread.
    instance doom: Doom.DoomEngine base id DoomSubtopologyConfig.BASE_ID + 0x00000

    @ Dedicated BufferManager for the Doom subtopology. Reserved for
    @ truly unpredictable dynamic allocations originating from within
    @ this subsystem; init-time predictable allocations should continue
    @ to use Fw::MallocAllocator (e.g. cmdSeq at the deployment layer).
    instance doomBufferManager: Svc.BufferManager base id DoomSubtopologyConfig.BASE_ID + 0x01000 \
    {
        phase Fpp.ToCpp.Phases.configObjects """
        Svc::BufferManager::BufferBins bins;
        """

        phase Fpp.ToCpp.Phases.configComponents """
        memset(&ConfigObjects::DoomSubtopology_doomBufferManager::bins, 0,
               sizeof(ConfigObjects::DoomSubtopology_doomBufferManager::bins));
        ConfigObjects::DoomSubtopology_doomBufferManager::bins.bins[0].bufferSize =
            DoomSubtopologyConfig::BuffMgr::doomBuffSize;
        ConfigObjects::DoomSubtopology_doomBufferManager::bins.bins[0].numBuffers =
            DoomSubtopologyConfig::BuffMgr::doomBuffCount;
        DoomSubtopology::doomBufferManager.setup(
            DoomSubtopologyConfig::BuffMgr::doomBuffMgrId,
            0,
            DoomSubtopology::Allocation::memAllocator,
            ConfigObjects::DoomSubtopology_doomBufferManager::bins
        );
        """

        phase Fpp.ToCpp.Phases.tearDownComponents """
        DoomSubtopology::doomBufferManager.cleanup();
        """
    }

    # ------------------------------------------------------------------
    # Subtopology - declares the instances participating and the ports
    # the enclosing deployment connects.
    # ------------------------------------------------------------------

    topology Subtopology {
        instance doom
        instance doomBufferManager

        # --------------------------------------------------------------
        # Exposed ports - the enclosing deployment wires these.
        # --------------------------------------------------------------

        @ Rate-group input that drives one doomgeneric_Tick per pulse.
        port schedIn = doom.schedIn

        @ Low-rate housekeeping tick for the dedicated BufferManager.
        @ Wire from a rate group on the order of 1 Hz.
        port bufferManagerSchedIn = doomBufferManager.schedIn

        @ Buffer pool allocate / deallocate endpoints. Reserved for
        @ future Doom-side users that need a managed memory chunk
        @ (e.g. a captured input trace) and don't want to call malloc
        @ directly.
        port bufferGetCallee = doomBufferManager.bufferGetCallee
        port bufferSendIn    = doomBufferManager.bufferSendIn

    } # end topology
} # end DoomSubtopology
