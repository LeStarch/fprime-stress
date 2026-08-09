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
    ASSERT_CMD_RESPONSE(0, DoomEngine::OPCODE_KEYTAP, cmdSeq, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(1, DoomEngine::OPCODE_KEYDOWN, cmdSeq + 1, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(2, DoomEngine::OPCODE_KEYUP, cmdSeq + 2, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(3, DoomEngine::OPCODE_RAWKEY, cmdSeq + 3, Fw::CmdResponse::OK);
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
    // Stop with the engine already off is a no-op that still responds
    // OK and publishes the OFF state.
    this->sendCmd_Stop(TEST_INSTANCE_ID, 0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, DoomEngine::OPCODE_STOP, 0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_EngineStopped_SIZE(0);
    ASSERT_TLM_State_SIZE(1);
    ASSERT_TLM_State(0, Doom::EngineState::OFF);
}

void DoomEngineTester::testStopWhileRunning() {
    // Stop on a running engine clears the flag, emits EngineStopped,
    // publishes OFF, and responds OK.
    this->component.m_engineRunning.store(true);
    this->sendCmd_Stop(TEST_INSTANCE_ID, 0);
    ASSERT_FALSE(this->component.m_engineRunning.load());
    ASSERT_EVENTS_EngineStopped_SIZE(1);
    ASSERT_TLM_State_SIZE(1);
    ASSERT_TLM_State(0, Doom::EngineState::OFF);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, DoomEngine::OPCODE_STOP, 0, Fw::CmdResponse::OK);
}

void DoomEngineTester::testResetRejectsBeforeStart() {
    // Reset before the engine was ever created has no game state to
    // reset: EXECUTION_ERROR, ResetNotStarted, and no pending request.
    this->sendCmd_Reset(TEST_INSTANCE_ID, 0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, DoomEngine::OPCODE_RESET, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_ResetNotStarted_SIZE(1);
    ASSERT_EVENTS_EngineReset_SIZE(0);
    ASSERT_FALSE(this->component.m_resetRequested.load());
}

void DoomEngineTester::testResetCommandSetsFlag() {
    // Reset on a created engine latches the request for the rate-group
    // thread and responds OK; the EngineReset event is emitted only
    // when the tick applies it.
    this->component.m_engineCreated = true;
    this->sendCmd_Reset(TEST_INSTANCE_ID, 0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, DoomEngine::OPCODE_RESET, 0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_ResetNotStarted_SIZE(0);
    ASSERT_EVENTS_EngineReset_SIZE(0);
    ASSERT_TRUE(this->component.m_resetRequested.load());
    this->component.m_resetRequested.store(false);
    this->component.m_engineCreated = false;
}

void DoomEngineTester::testSchedInAppliesReset() {
    // A latched reset is applied at the top of the next running tick:
    // the key queue is flushed, melt playback is discarded, EngineReset
    // is emitted, and no frame is played back or ticked that cycle.
    // D_StartTitle only latches engine-side flags, so the path is safe
    // to drive without a created engine.
    this->invoke_to_rawKeyIn(0, true, static_cast<U8>(0x10));
    this->invoke_to_rawKeyIn(0, false, static_cast<U8>(0x10));
    ASSERT_EQ(this->component.m_keyQueueCount, 2u);
    this->component.m_meltCount = 2U;
    this->component.m_meltHead = 1U;
    this->component.m_resetRequested.store(true);

    this->component.m_engineRunning.store(true);
    this->invoke_to_schedIn(0, 0);
    this->component.m_engineRunning.store(false);

    ASSERT_EVENTS_EngineReset_SIZE(1);
    ASSERT_FALSE(this->component.m_resetRequested.load());
    ASSERT_EQ(this->component.m_keyQueueCount, 0u);
    ASSERT_EQ(this->component.m_meltCount, 0u);
    ASSERT_EQ(this->component.m_meltHead, 0u);
    // The reset tick neither plays back a melt frame nor ticks DOOM.
    ASSERT_TLM_FrameOut00_SIZE(0);
    ASSERT_TLM_KeyQueueDepth_SIZE(1);
    ASSERT_TLM_KeyQueueDepth(0, 0u);

    // A second reset request while stopped stays latched until the
    // next running tick, then applies exactly once.
    this->component.m_resetRequested.store(true);
    this->invoke_to_schedIn(0, 0);
    ASSERT_EVENTS_EngineReset_SIZE(1);
    ASSERT_TRUE(this->component.m_resetRequested.load());

    // The latched request is applied on the first running tick after
    // the next Start.
    this->component.m_engineRunning.store(true);
    this->invoke_to_schedIn(0, 0);
    this->component.m_engineRunning.store(false);
    ASSERT_EVENTS_EngineReset_SIZE(2);
    ASSERT_FALSE(this->component.m_resetRequested.load());
}

void DoomEngineTester::testSchedInWhenEngineOff() {
    this->invoke_to_schedIn(0, 0);
    // No FrameOutNN channel should be emitted when the engine is off.
    // FrameOut is multiplexed across 80 channel ids; FrameOut00 is the
    // representative cell.
    ASSERT_TLM_FrameOut00_SIZE(0);
    ASSERT_EVENTS_EngineStarted_SIZE(0);
    ASSERT_EVENTS_EngineStopped_SIZE(0);
    // The handler still publishes the OFF-state heartbeat.
    ASSERT_TLM_State_SIZE(1);
    ASSERT_TLM_State(0, Doom::EngineState::OFF);
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

    // First draw of a tick is emitted live, with the active palette.
    this->component.platformDrawFrame();
    ASSERT_TLM_FrameOut00_SIZE(1);
    ASSERT_EQ(this->tlmHistory_FrameOut00->at(0).arg.get_frame(), 1U);
    ASSERT_TLM_PaletteOut_SIZE(1);

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
    this->component.m_engineRunning.store(true);
    this->invoke_to_schedIn(0, 0);
    this->component.m_engineRunning.store(false);

    ASSERT_TLM_FrameOut00_SIZE(2);
    // The palette is re-emitted with every frame, replays included.
    ASSERT_TLM_PaletteOut_SIZE(2);
    ASSERT_EQ(this->tlmHistory_PaletteOut->at(1).arg.get_generation(),
              this->component.m_paletteGeneration);
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
    this->component.m_engineRunning.store(true);
    this->invoke_to_schedIn(0, 0);
    this->component.m_engineRunning.store(false);
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
    // The timeout path bails out before any State telemetry.
    ASSERT_TLM_State_SIZE(0);
}

void DoomEngineTester::testKeyTapAllOrNothing() {
    // Fill the queue to capacity-1: a tap needs 2 slots, so it must
    // enqueue neither event and count both as dropped.
    const FwSizeType cap = DoomEngine::KEY_QUEUE_CAPACITY;
    for (FwSizeType i = 0; i + 1 < cap; ++i) {
        this->invoke_to_rawKeyIn(0, true, static_cast<U8>(i & 0xFFu));
    }
    Doom::DoomKey use_key(Doom::DoomKey::USE);
    this->invoke_to_keyTapIn(0, use_key);
    ASSERT_EQ(this->component.m_keyQueueCount, cap - 1);
    ASSERT_EQ(this->component.m_keysDropped, 2U);
    ASSERT_EVENTS_KeyQueueOverflow_SIZE(1);

    // The KeyTap command reports the same overflow as EXECUTION_ERROR.
    this->sendCmd_KeyTap(TEST_INSTANCE_ID, 0, use_key);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, DoomEngine::OPCODE_KEYTAP, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EQ(this->component.m_keysDropped, 4U);
}

void DoomEngineTester::testStartRejectsMissingWad() {
    // A missing WAD must reject the Start (EXECUTION_ERROR + FAILED)
    // instead of letting the vendored I_Error exit the process.
    this->component.setWadPath("/nonexistent/doom1.wad");
    this->sendCmd_Start(TEST_INSTANCE_ID, 0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, DoomEngine::OPCODE_START, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_WadUnavailable_SIZE(1);
    ASSERT_EVENTS_WadUnavailable(0, "/nonexistent/doom1.wad");
    ASSERT_EVENTS_EngineStarted_SIZE(0);
    ASSERT_TLM_State_SIZE(1);
    ASSERT_TLM_State(0, Doom::EngineState::FAILED);
    ASSERT_FALSE(this->component.m_engineRunning.load());

    // FAILED persists across schedIn heartbeats (not clobbered by OFF).
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_State_SIZE(2);
    ASSERT_TLM_State(1, Doom::EngineState::FAILED);
}

void DoomEngineTester::testHeartbeatSelfHealsStaleRunning() {
    // A stale RUNNING latched by a tick that raced a Stop must be
    // self-healed to OFF by the not-running heartbeat.
    this->component.m_lastState.store(Doom::EngineState::RUNNING);
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_State_SIZE(1);
    ASSERT_TLM_State(0, Doom::EngineState::OFF);
    ASSERT_EQ(this->component.m_lastState.load(), Doom::EngineState::OFF);

    // Subsequent heartbeats keep publishing OFF.
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_State_SIZE(2);
    ASSERT_TLM_State(1, Doom::EngineState::OFF);
}

void DoomEngineTester::testStartCommandRejectsWhenRunning() {
    // A Start command on a running engine maps to EXECUTION_ERROR.
    this->component.m_engineRunning.store(true);
    this->sendCmd_Start(TEST_INSTANCE_ID, 0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, DoomEngine::OPCODE_START, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_AlreadyRunning_SIZE(1);
}

void DoomEngineTester::testStartRejectsUnconfiguredWad() {
    // No WAD path configured: Start must reject rather than let the
    // vendored auto-search reach I_Error/exit.
    this->sendCmd_Start(TEST_INSTANCE_ID, 0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, DoomEngine::OPCODE_START, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_WadUnavailable_SIZE(1);
    ASSERT_EVENTS_WadUnavailable(0, "(no WAD path configured)");
    ASSERT_TLM_State_SIZE(1);
    ASSERT_TLM_State(0, Doom::EngineState::FAILED);
}

void DoomEngineTester::testForceStartResumesAfterStop() {
    // A Start after a Stop must resume the existing engine: no second
    // doomgeneric_Create, elapsed-time accumulators preserved (the
    // vendored timer's cached basetime must never see the clock step
    // backwards), melt/draw pacing state discarded.
    this->component.m_engineCreated = true;
    this->component.m_realElapsedUsec = 5000000U;
    this->component.m_virtualSleepMs = 250U;
    this->component.m_meltCount = 3U;
    this->component.m_drawsThisTick = 2U;

    ASSERT_TRUE(this->component.forceStart());

    ASSERT_EVENTS_EngineStarted_SIZE(1);
    ASSERT_TLM_State_SIZE(2);
    ASSERT_TLM_State(0, Doom::EngineState::STARTING);
    ASSERT_TLM_State(1, Doom::EngineState::RUNNING);
    ASSERT_TRUE(this->component.m_engineRunning.load());
    ASSERT_EQ(this->component.m_realElapsedUsec, 5000000U);
    ASSERT_EQ(this->component.m_virtualSleepMs, 250U);
    ASSERT_EQ(this->component.m_meltCount, 0U);
    ASSERT_EQ(this->component.m_drawsThisTick, 0U);

    this->component.m_engineRunning.store(false);
}

void DoomEngineTester::testForceStartWhenAlreadyRunning() {
    // A Start while the engine runs must fail fast with AlreadyRunning
    // and leave the running state untouched.
    this->component.m_engineRunning.store(true);
    ASSERT_FALSE(this->component.forceStart());
    ASSERT_EVENTS_AlreadyRunning_SIZE(1);
    ASSERT_EVENTS_StartBusy_SIZE(0);
    ASSERT_TRUE(this->component.m_engineRunning.load());
    this->component.m_engineRunning.store(false);
}

}  // namespace Doom
