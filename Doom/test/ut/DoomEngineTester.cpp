// ======================================================================
// \title  DoomEngineTester.cpp
// \brief  Unit-test harness for the DoomEngine component.
// ======================================================================

#include "Doom/test/ut/DoomEngineTester.hpp"
#include "Doom/FppConstantsAc.hpp"

extern "C" {
#include "Doom/doomgeneric/doomgeneric.h"
}

namespace Doom {

DoomEngineTester::DoomEngineTester()
    : DoomEngineGTestBase("DoomEngineTester", DoomEngineTester::MAX_HISTORY_SIZE),
      component("DoomEngine") {
    this->initComponents();
    this->connectPorts();
}

DoomEngineTester::~DoomEngineTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

FwSizeType DoomEngineTester::drainKeys(bool* pressedOut, U8* codeOut, FwSizeType maxEvents) {
    FwSizeType drained = 0;
    while (drained < maxEvents) {
        bool pressed = false;
        U8 code = 0;
        if (!this->component.platformGetKey(pressed, code)) {
            break;
        }
        pressedOut[drained] = pressed;
        codeOut[drained] = code;
        ++drained;
    }
    return drained;
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void DoomEngineTester::testCommandsEnqueueKeys() {
    const U32 cmdSeq = 1;

    // The component is passive, so sync command handlers run
    // immediately on sendCmd_*; there is no queue to drain.
    this->sendCmd_KeyTap(TEST_INSTANCE_ID, cmdSeq, Doom::DoomKey::FIRE);
    this->sendCmd_KeyDown(TEST_INSTANCE_ID, cmdSeq + 1, Doom::DoomKey::UP);
    this->sendCmd_KeyUp(TEST_INSTANCE_ID, cmdSeq + 2, Doom::DoomKey::UP);
    this->sendCmd_RawKey(TEST_INSTANCE_ID, cmdSeq + 3, true, static_cast<U8>(0x42));

    // KeyTap -> (true, FIRE), (false, FIRE)
    // KeyDown -> (true, UP)
    // KeyUp -> (false, UP)
    // RawKey -> (true, 0x42)
    bool pressed[8] = {false};
    U8 code[8] = {0};
    const FwSizeType drained = this->drainKeys(pressed, code, 8);
    ASSERT_EQ(drained, 5u);

    ASSERT_TRUE(pressed[0]);
    ASSERT_EQ(code[0], static_cast<U8>(Doom::DoomKey::FIRE));
    ASSERT_FALSE(pressed[1]);
    ASSERT_EQ(code[1], static_cast<U8>(Doom::DoomKey::FIRE));
    ASSERT_TRUE(pressed[2]);
    ASSERT_EQ(code[2], static_cast<U8>(Doom::DoomKey::UP));
    ASSERT_FALSE(pressed[3]);
    ASSERT_EQ(code[3], static_cast<U8>(Doom::DoomKey::UP));
    ASSERT_TRUE(pressed[4]);
    ASSERT_EQ(code[4], 0x42);

    ASSERT_CMD_RESPONSE_SIZE(4);
    ASSERT_EVENTS_KeyQueueOverflow_SIZE(0);
}

void DoomEngineTester::testParallelPortsEnqueueKeys() {
    Doom::DoomKey use_key(Doom::DoomKey::USE);
    this->invoke_to_keyTapIn(0, use_key);

    Doom::DoomKey shift_key(Doom::DoomKey::SHIFT);
    this->invoke_to_keyDownIn(0, shift_key);
    this->invoke_to_keyUpIn(0, shift_key);

    this->invoke_to_rawKeyIn(0, false, static_cast<U8>(0x7F));

    bool pressed[8] = {false};
    U8 code[8] = {0};
    const FwSizeType drained = this->drainKeys(pressed, code, 8);
    ASSERT_EQ(drained, 5u);

    ASSERT_TRUE(pressed[0]);
    ASSERT_EQ(code[0], static_cast<U8>(Doom::DoomKey::USE));
    ASSERT_FALSE(pressed[1]);
    ASSERT_EQ(code[1], static_cast<U8>(Doom::DoomKey::USE));
    ASSERT_TRUE(pressed[2]);
    ASSERT_EQ(code[2], static_cast<U8>(Doom::DoomKey::SHIFT));
    ASSERT_FALSE(pressed[3]);
    ASSERT_EQ(code[3], static_cast<U8>(Doom::DoomKey::SHIFT));
    ASSERT_FALSE(pressed[4]);
    ASSERT_EQ(code[4], 0x7F);

    ASSERT_EVENTS_KeyQueueOverflow_SIZE(0);
}

void DoomEngineTester::testOverflowEmitsEvent() {
    const FwSizeType cap = DoomEngine::KEY_QUEUE_CAPACITY;
    for (FwSizeType i = 0; i < cap; ++i) {
        this->invoke_to_rawKeyIn(0, true, static_cast<U8>(i & 0xFFu));
    }
    ASSERT_EVENTS_KeyQueueOverflow_SIZE(0);

    this->invoke_to_rawKeyIn(0, true, static_cast<U8>(0xAA));
    ASSERT_EVENTS_KeyQueueOverflow_SIZE(1);

    this->invoke_to_rawKeyIn(0, true, static_cast<U8>(0xBB));
    ASSERT_EVENTS_KeyQueueOverflow_SIZE(1);

    bool pressed[DoomEngine::KEY_QUEUE_CAPACITY] = {false};
    U8 code[DoomEngine::KEY_QUEUE_CAPACITY] = {0};
    const FwSizeType drained = this->drainKeys(pressed, code, cap);
    ASSERT_EQ(drained, cap);

    bool extraPressed = false;
    U8 extraCode = 0;
    ASSERT_FALSE(this->component.platformGetKey(extraPressed, extraCode));
}

void DoomEngineTester::testStopCommandResponds() {
    this->sendCmd_Stop(TEST_INSTANCE_ID, 0);
    ASSERT_CMD_RESPONSE_SIZE(1);
}

void DoomEngineTester::testSchedInWhenEngineOff() {
    this->invoke_to_schedIn(0, 0);
    // No FrameOutNN channel should be emitted when the engine is off.
    // FrameOut is multiplexed across 80 channel ids; FrameOut00 is the
    // representative cell.
    ASSERT_TLM_FrameOut00_SIZE(0);
    ASSERT_EVENTS_EngineStarted_SIZE(0);
    ASSERT_EVENTS_EngineStopped_SIZE(0);
}

void DoomEngineTester::testVirtualSleepAdvancesTicks() {
    // Engine not started: the reported clock is exactly the virtual
    // time accumulated by platformSleepMs.
    ASSERT_EQ(this->component.platformGetTicksMs(), 0U);
    this->component.platformSleepMs(7U);
    this->component.platformSleepMs(3U);
    ASSERT_EQ(this->component.platformGetTicksMs(), 10U);
}

void DoomEngineTester::testDrawFrameEmitsFirstDrawAndBuffersMelt() {
    static U8 buf[DoomEngine::FRAME_BYTES];
    for (U32 i = 0; i < DoomEngine::FRAME_BYTES; ++i) {
        buf[i] = static_cast<U8>(i & 0xFFU);
    }
    DG_ScreenBuffer = reinterpret_cast<pixel_t*>(buf);

    // First draw of a tick is emitted live.
    this->component.platformDrawFrame();
    ASSERT_TLM_FrameOut00_SIZE(1);
    ASSERT_EQ(this->tlmHistory_FrameOut00->at(0).arg.get_frame(), 1U);

    // Second draw of the same tick is buffered, not emitted.
    this->component.platformDrawFrame();
    ASSERT_TLM_FrameOut00_SIZE(1);
    ASSERT_EQ(this->component.m_meltCount, 1U);
    ASSERT_EQ(this->component.m_meltFrameNumbers[0], 2U);

    DG_ScreenBuffer = nullptr;
}

void DoomEngineTester::testSchedInPlaysBackMeltFrames() {
    // Position-dependent pattern so a chunk-offset bug cannot pass.
    static U8 buf[DoomEngine::FRAME_BYTES];
    for (U32 i = 0; i < DoomEngine::FRAME_BYTES; ++i) {
        buf[i] = static_cast<U8>(i % 251U);
    }
    DG_ScreenBuffer = reinterpret_cast<pixel_t*>(buf);

    // Capture one melt frame (first draw emits, second buffers).
    this->component.platformDrawFrame();
    this->component.platformDrawFrame();
    ASSERT_TLM_FrameOut00_SIZE(1);
    ASSERT_EQ(this->component.m_meltCount, 1U);

    // Drive schedIn with the engine "running" via the test seam: the
    // melt branch plays back the buffered frame without ticking DOOM.
    this->component.m_engineRunning.store(true, std::memory_order_release);
    this->invoke_to_schedIn(0, 0);
    this->component.m_engineRunning.store(false, std::memory_order_release);

    ASSERT_TLM_FrameOut00_SIZE(2);
    const Doom::FrameChunk& replayed = this->tlmHistory_FrameOut00->at(1).arg;
    ASSERT_EQ(replayed.get_frame(), 2U);
    // The replayed chunks must carry the buffered pixel payload at the
    // correct per-chunk offsets (first and last chunks checked).
    const U8* const first = replayed.get_pixels();
    ASSERT_TLM_FrameOut79_SIZE(2);
    const Doom::FrameChunk& last = this->tlmHistory_FrameOut79->at(1).arg;
    ASSERT_EQ(last.get_frame(), 2U);
    const U8* const lastPix = last.get_pixels();
    const U32 chunkBytes = static_cast<U32>(Doom::FRAME_CHUNK_BYTES);
    const U32 lastOffset = 79U * chunkBytes;
    for (U32 i = 0; i < chunkBytes; ++i) {
        ASSERT_EQ(first[i], buf[i]) << "chunk 0 pixel " << i;
        ASSERT_EQ(lastPix[i], buf[lastOffset + i]) << "chunk 79 pixel " << i;
    }
    ASSERT_EQ(this->component.m_meltCount, 0U);

    DG_ScreenBuffer = nullptr;
}

void DoomEngineTester::testMeltOverflowCountsDroppedFrames() {
    static U8 buf[DoomEngine::FRAME_BYTES];
    (void)::memset(buf, 0x11, sizeof(buf));
    DG_ScreenBuffer = reinterpret_cast<pixel_t*>(buf);

    // One live draw, then fill the melt ring to capacity, then two more
    // draws that must be dropped and counted.
    const FwSizeType cap = DoomEngine::MELT_QUEUE_CAPACITY;
    this->component.platformDrawFrame();
    for (FwSizeType i = 0; i < cap; ++i) {
        this->component.platformDrawFrame();
    }
    ASSERT_EQ(this->component.m_meltCount, cap);
    ASSERT_EQ(this->component.m_framesDropped, 0U);

    this->component.platformDrawFrame();
    this->component.platformDrawFrame();
    ASSERT_EQ(this->component.m_meltCount, cap);
    ASSERT_EQ(this->component.m_framesDropped, 2U);

    // schedIn publishes the drop count on the FramesDropped channel.
    this->component.m_engineRunning.store(true, std::memory_order_release);
    this->invoke_to_schedIn(0, 0);
    this->component.m_engineRunning.store(false, std::memory_order_release);
    ASSERT_TLM_FramesDropped_SIZE(1);
    ASSERT_TLM_FramesDropped(0, 2U);

    DG_ScreenBuffer = nullptr;
}

void DoomEngineTester::testForceStartBusyRendezvousTimesOut() {
    // Simulate a rate-group tick that never finishes: forceStart must
    // give up after its bounded wait, emit StartBusy, and leave the
    // engine stopped without touching engine state.
    this->component.m_tickInProgress.store(true);
    ASSERT_FALSE(this->component.forceStart());
    this->component.m_tickInProgress.store(false);

    ASSERT_EVENTS_StartBusy_SIZE(1);
    ASSERT_EVENTS_AlreadyRunning_SIZE(0);
    ASSERT_FALSE(this->component.m_engineRunning.load());
}

}  // namespace Doom
