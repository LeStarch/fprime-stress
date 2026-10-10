// ======================================================================
// \title  WadFile.hpp
// \author lestarch-autobot
// \brief  doomgeneric WAD file access through Os::File
//
// Provides the wad_file_class_t that stock doomgeneric (w_file.c) calls
// stdc_wad_file, so the engine reads WADs through the F Prime OSAL rather
// than C stdio, which not every F Prime platform supplies. Open files
// live in a fixed pool: no dynamic allocation.
// ======================================================================
#ifndef Doom_DoomEngine_WadFile_HPP
#define Doom_DoomEngine_WadFile_HPP

#include <Fw/FPrimeBasicTypes.hpp>

namespace Doom {
namespace WadFile {

//! Number of WAD files that may be open at once (the IWAD plus PWADs)
static constexpr FwSizeType MAX_OPEN = 4;

//! \brief number of pool slots currently holding an open file
FwSizeType openCount();

}  // namespace WadFile
}  // namespace Doom

#endif
