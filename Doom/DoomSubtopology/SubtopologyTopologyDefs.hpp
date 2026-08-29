// ======================================================================
// \title  SubtopologyTopologyDefs.hpp
// \brief  Definitions that the topology autocoder pulls in when the
//         enclosing deployment includes `instance DoomSubtopology...`.
// ======================================================================
#ifndef DoomSubtopology_SubtopologyTopologyDefs_HPP
#define DoomSubtopology_SubtopologyTopologyDefs_HPP

#include <Fw/Types/MallocAllocator.hpp>
#include <Svc/BufferManager/BufferManager.hpp>

#include "Doom/DoomSubtopology/DoomSubtopologyConfig/DoomSubtopologyConfig.hpp"

namespace DoomSubtopology {

//! State the enclosing deployment may supply when configuring the
//! Doom subtopology. Currently empty - the WAD path is configured
//! directly against the doom instance by the deployment after
//! topology setup.
struct SubtopologyState {};

//! Topology-state wrapper matching the F Prime subtopology pattern.
struct TopologyState {
    SubtopologyState doom;
};

}  // namespace DoomSubtopology

#endif
