// ======================================================================
// \title  PingEntries.hpp
// \brief  Health-ping thresholds for Doom subtopology instances.
// ======================================================================
#ifndef DOOMSUBTOPOLOGY_PINGENTRIES_HPP
#define DOOMSUBTOPOLOGY_PINGENTRIES_HPP

namespace PingEntries {
struct DoomSubtopology_doom {
    enum { WARN = 3, FATAL = 5 };
};
}  // namespace PingEntries

#endif
