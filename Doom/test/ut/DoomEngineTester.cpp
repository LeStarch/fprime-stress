// ======================================================================
// \title  DoomTester.cpp
// \brief  Unit-test harness for the DoomEngine component.
// ======================================================================

#include "Doom/test/ut/DoomEngineTester.hpp"
#include "Doom/FppConstantsAc.hpp"

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

    this->sendCmd_KeyTap(TEST_INSTANCE_ID, cmdSeq, Doom::DoomKey::FIRE);
    this->dispatchOne(this->component);

    this->sendCmd_KeyDown(TEST_INSTANCE_ID, cmdSeq + 1, Doom::DoomKey::UP);
    this->dispatchOne(this->component);

    this->sendCmd_KeyUp(TEST_INSTANCE_ID, cmdSeq + 2, Doom::DoomKey::UP);
    this->dispatchOne(this->component);

    this->sendCmd_RawKey(TEST_INSTANCE_ID, cmdSeq + 3, true, static_cast<U8>(0x42));
    this->dispatchOne(this->component);

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
    this->dispatchOne(this->component);
    ASSERT_CMD_RESPONSE_SIZE(1);
}

void DoomEngineTester::testSchedInWhenEngineOff() {
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_FrameOut_SIZE(0);
    ASSERT_EVENTS_EngineStarted_SIZE(0);
    ASSERT_EVENTS_EngineStopped_SIZE(0);
}

}  // namespace Doom
