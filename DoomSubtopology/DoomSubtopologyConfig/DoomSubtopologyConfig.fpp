module DoomSubtopologyConfig {

    @ Base ID for the Doom subtopology. Deployments should offset other
    @ subtopologies away from this range. The "0D" prefix is chosen so
    @ the slot reads as "DOOM" in hex.
    constant BASE_ID = 0x0D000000

    module QueueSizes {
        @ DoomEngine queue depth. schedIn is sync so the queue only
        @ carries async command messages; 32 is ample headroom.
        constant doomEngine = 32
    }

    module StackSizes {
        constant doomEngine = 64 * 1024
    }

    module Priorities {
        constant doomEngine = 30
    }

    @ BufferManager parameters. The Doom subtopology hosts its own
    @ BufferManager so that any unpredictable dynamic allocation that
    @ may emerge (e.g. captured input traces, telemetry packet copies)
    @ can be served out of a single managed pool rather than calling
    @ malloc directly at runtime. Init-time fixed-size allocations
    @ (such as the cmdSeq load buffer at the deployment layer) continue
    @ to use Fw::MallocAllocator since they're predictable at startup.
    module BuffMgr {
        @ Per-buffer size of the general-purpose Doom pool.
        constant doomBuffSize  = 4096
        @ Number of buffers in the general-purpose Doom pool.
        constant doomBuffCount = 16
        @ BufferManager identifier (any unique value within the
        @ deployment is fine; the Doom subtopology uses 0x0D for
        @ symmetry with its BASE_ID).
        constant doomBuffMgrId = 0x0D
    }
}
