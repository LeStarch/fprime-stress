#ifndef DOOMSUBTOPOLOGY_CONFIG_HPP
#define DOOMSUBTOPOLOGY_CONFIG_HPP

#include "Fw/Types/MallocAllocator.hpp"

namespace DoomSubtopology {
namespace Allocation {
//! The allocator used by the Doom subtopology to back its
//! BufferManager bins. Swap for a different Fw::MemAllocator if a
//! deployment wants a non-malloc-backed pool.
extern Fw::MemAllocator& memAllocator;
}  // namespace Allocation
}  // namespace DoomSubtopology

#endif
