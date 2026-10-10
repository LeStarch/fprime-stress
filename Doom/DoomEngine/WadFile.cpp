// ======================================================================
// \title  WadFile.cpp
// \author lestarch-autobot
// \brief  doomgeneric WAD file access through Os::File
// ======================================================================
#include "Doom/DoomEngine/WadFile.hpp"

#include <Fw/Types/Assert.hpp>
#include <Os/File.hpp>

#include <limits>

extern "C" {
#include "Doom/DoomEngine/doomgeneric/doomgeneric/w_file.h"
extern wad_file_class_t stdc_wad_file;
}

namespace {

struct Slot {
    wad_file_t wad;
    Os::File file;
    bool inUse;
};

Slot s_pool[Doom::WadFile::MAX_OPEN];

//! \brief the pool slot owning a wad_file_t handed out by openFile, or nullptr
Slot* slotOf(const wad_file_t* wad) {
    for (FwSizeType i = 0; i < Doom::WadFile::MAX_OPEN; i++) {
        if (&s_pool[i].wad == wad) {
            return &s_pool[i];
        }
    }
    return nullptr;
}

wad_file_t* openFile(char* path) {
    if (path == nullptr) {
        return nullptr;
    }
    for (FwSizeType i = 0; i < Doom::WadFile::MAX_OPEN; i++) {
        Slot& slot = s_pool[i];
        if (slot.inUse) {
            continue;
        }
        if (slot.file.open(path, Os::File::OPEN_READ) != Os::File::OP_OK) {
            return nullptr;
        }
        FwSizeType length = 0;
        if ((slot.file.size(length) != Os::File::OP_OK) ||
            (length > static_cast<FwSizeType>(std::numeric_limits<unsigned int>::max()))) {
            slot.file.close();
            return nullptr;
        }
        slot.inUse = true;
        slot.wad.file_class = &stdc_wad_file;
        slot.wad.mapped = nullptr;
        slot.wad.length = static_cast<unsigned int>(length);
        return &slot.wad;
    }
    return nullptr;  // pool exhausted
}

void closeFile(wad_file_t* wad) {
    Slot* const slot = slotOf(wad);
    FW_ASSERT(slot != nullptr);
    FW_ASSERT(slot->inUse);
    slot->file.close();
    slot->inUse = false;
}

size_t readFile(wad_file_t* wad, unsigned int offset, void* buffer, size_t buffer_len) {
    FW_ASSERT(buffer != nullptr);
    Slot* const slot = slotOf(wad);
    FW_ASSERT(slot != nullptr);
    FW_ASSERT(slot->inUse);
    if (slot->file.seek(static_cast<FwSignedSizeType>(offset), Os::File::SeekType::ABSOLUTE) != Os::File::OP_OK) {
        return 0;
    }
    U8* const out = static_cast<U8*>(buffer);
    size_t total = 0;
    // Each pass reads at least one byte or ends the loop, so passes <= buffer_len
    for (size_t pass = 0; (pass < buffer_len) && (total < buffer_len); pass++) {
        FwSizeType chunk = static_cast<FwSizeType>(buffer_len - total);
        if ((slot->file.read(out + total, chunk) != Os::File::OP_OK) || (chunk == 0)) {
            break;  // error or end of file: doomgeneric treats a short count as such
        }
        total += static_cast<size_t>(chunk);
    }
    return total;
}

}  // namespace

namespace Doom {
namespace WadFile {

FwSizeType openCount() {
    FwSizeType count = 0;
    for (FwSizeType i = 0; i < MAX_OPEN; i++) {
        count += s_pool[i].inUse ? 1 : 0;
    }
    return count;
}

}  // namespace WadFile
}  // namespace Doom

wad_file_class_t stdc_wad_file = {
    openFile,
    closeFile,
    readFile,
};
