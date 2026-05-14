#include "DoomSubtopologyConfig.hpp"

namespace DoomSubtopology {
namespace Allocation {
// MallocAllocator pre-allocates each bin once during setup and never
// reallocates - the runtime allocation pattern of the BufferManager
// itself is what handles the "truly unpredictable" dynamic memory.
Fw::MallocAllocator mallocatorInstance;
Fw::MemAllocator& memAllocator = mallocatorInstance;
}  // namespace Allocation
}  // namespace DoomSubtopology
