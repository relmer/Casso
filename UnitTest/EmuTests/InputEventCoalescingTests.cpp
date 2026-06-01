#include "Pch.h"
#include "Core/MemoryBus.h"
#include "Devices/AppleKeyboard.h"
#include "Devices/Apple2eKeyboard.h"
#include "Devices/Apple2eSoftSwitchBank.h"
#include "Devices/IInputEventSink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RecordingInputSink
    //
    //  Test double that tallies every IInputEventSink callback and remembers
    //  the most recent arguments so the device-side coalescing can be
    //  asserted without standing up the real panel or any threading.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class RecordingInputSink : public IInputEventSink
    {
    public:
        int   kbdDataReadCount   = 0;
        int   kbdStrobeCount     = 0;
        int   buttonReadCount    = 0;
        int   paddleTriggerCount = 0;
        int   paddleReadCount    = 0;
        int   autoRepeatCount    = 0;
        int   hostKeyDownCount   = 0;
        int   hostKeyUpCount     = 0;

        Word  lastAddress        = 0;
        Byte  lastValue          = 0;
        bool  lastStrobeSet      = false;
        bool  lastClearedStrobe  = false;
        Byte  lastAscii          = 0;

        void OnKbdDataRead (Word address, Byte value, bool strobeSet) override
        {
            kbdDataReadCount++;
            lastAddress   = address;
            lastValue     = value;
            lastStrobeSet = strobeSet;
        }

        void OnKbdStrobe (Word address, Byte value, bool clearedStrobe) override
        {
            kbdStrobeCount++;
            lastAddress       = address;
            lastValue         = value;
            lastClearedStrobe = clearedStrobe;
        }

        void OnButtonRead (Word address, Byte value) override
        {
            buttonReadCount++;
            lastAddress = address;
            lastValue   = value;
        }

        void OnPaddleTrigger (Word address) override
        {
            paddleTriggerCount++;
            lastAddress = address;
        }

        void OnPaddleRead (Word address, Byte value) override
        {
            paddleReadCount++;
            lastAddress = address;
            lastValue   = value;
        }

        void OnHostAutoRepeat (Byte asciiChar) override
        {
            autoRepeatCount++;
            lastAscii = asciiChar;
        }

        void OnHostKeyDown (Byte asciiChar) override
        {
            hostKeyDownCount++;
            lastAscii = asciiChar;
        }

        void OnHostKeyUp (Byte asciiChar) override
        {
            hostKeyUpCount++;
            lastAscii = asciiChar;
        }
    };
}




////////////////////////////////////////////////////////////////////////////////
//
//  InputEventCoalescingTests
//
//  Verifies the producer-side coalescing the keyboard devices perform
//  before notifying an attached IInputEventSink: a tight poll loop must
//  collapse to one event per observed transition, and detaching the sink
//  must silence all emits (the no-sink fast path).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InputEventCoalescingTests)
{
public:

    TEST_METHOD (DataRead_CoalescesIdenticalPolls)
    {
        AppleKeyboard       kbd;
        RecordingInputSink  sink;

        kbd.SetInputEventSink (&sink);
        kbd.KeyPress ('A');

        for (int i = 0; i < 5; i++)
        {
            kbd.Read (0xC000);
        }

        Assert::AreEqual (1, sink.kbdDataReadCount,
            L"Five identical $C000 polls must emit exactly one data-read event");
        Assert::AreEqual (static_cast<uint8_t> (0xC1), sink.lastValue,
            L"The emitted value must be the latched key with the strobe bit set");
        Assert::IsTrue (sink.lastStrobeSet,
            L"The strobe bit must be reported set while the key is unread");
    }

    TEST_METHOD (DataRead_ReEmitsWhenLatchChanges)
    {
        AppleKeyboard       kbd;
        RecordingInputSink  sink;

        kbd.SetInputEventSink (&sink);

        kbd.KeyPress ('A');
        kbd.Read (0xC000);

        kbd.KeyPress ('B');
        kbd.Read (0xC000);

        Assert::AreEqual (2, sink.kbdDataReadCount,
            L"A latch change between polls must produce a second data-read event");
        Assert::AreEqual (static_cast<uint8_t> (0xC2), sink.lastValue,
            L"The second emit must carry the newly latched key");
    }

    TEST_METHOD (Strobe_EmitsOnClearThenCoalesces)
    {
        AppleKeyboard       kbd;
        RecordingInputSink  sink;

        kbd.SetInputEventSink (&sink);
        kbd.KeyPress ('A');

        kbd.Read (0xC010);
        kbd.Read (0xC010);

        Assert::AreEqual (1, sink.kbdStrobeCount,
            L"The first $C010 clears a set strobe (one emit); the second is a no-op edge");
        Assert::IsTrue (sink.lastClearedStrobe,
            L"The clearing access must report clearedStrobe == true");
    }

    TEST_METHOD (DetachedSink_SilencesAllEmits)
    {
        AppleKeyboard       kbd;
        RecordingInputSink  sink;

        kbd.SetInputEventSink (&sink);
        kbd.KeyPress ('A');
        kbd.Read (0xC000);

        int baseline = sink.kbdDataReadCount;

        kbd.SetInputEventSink (nullptr);
        kbd.KeyPress ('B');

        for (int i = 0; i < 5; i++)
        {
            kbd.Read (0xC000);
        }

        Assert::AreEqual (baseline, sink.kbdDataReadCount,
            L"No callbacks may fire once the sink is detached (no-sink fast path)");
    }

    TEST_METHOD (HostKeyDownUp_CoalescesHeldKey)
    {
        AppleKeyboard       kbd;
        RecordingInputSink  sink;

        kbd.SetInputEventSink (&sink);

        kbd.BeginKeyRepeat ('J');
        kbd.BeginKeyRepeat ('J');
        kbd.BeginKeyRepeat (0);

        Assert::AreEqual (1, sink.hostKeyDownCount,
            L"A repeated host down for the same held key must coalesce to one key-down");
        Assert::AreEqual (1, sink.hostKeyUpCount,
            L"Disarming the repeat must emit exactly one key-up");
    }

    TEST_METHOD (AutoRepeat_FiresFromTickWhileHeld)
    {
        AppleKeyboard       kbd;
        RecordingInputSink  sink;

        kbd.SetInputEventSink (&sink);
        kbd.SetKeyDown (true);
        kbd.BeginKeyRepeat ('K');

        // First Tick arms the cadence for the newly armed key; the second,
        // with a large cycle budget, crosses the pre-repeat delay and
        // re-latches the held key once.
        kbd.Tick (1);
        kbd.Tick (10000000);

        Assert::AreEqual (1, sink.autoRepeatCount,
            L"A held key crossing the repeat delay must emit one auto-repeat event");
        Assert::AreEqual (static_cast<uint8_t> ('K'), sink.lastAscii,
            L"The auto-repeat must carry the held key's character");
    }

    TEST_METHOD (ButtonRead_CoalescesPerAddress)
    {
        MemoryBus           bus;
        Apple2eKeyboard     kbd (&bus);
        RecordingInputSink  sink;

        kbd.SetInputEventSink (&sink);
        kbd.SetClosedApple (true);

        for (int i = 0; i < 3; i++)
        {
            kbd.Read (0xC062);
        }

        Assert::AreEqual (1, sink.buttonReadCount,
            L"Three identical Closed-Apple polls must emit one button-read event");
        Assert::AreEqual (static_cast<uint8_t> (0x80), sink.lastValue,
            L"A pressed button must report bit 7 set");

        kbd.SetClosedApple (false);
        kbd.Read (0xC062);

        Assert::AreEqual (2, sink.buttonReadCount,
            L"Releasing the button is a new edge and must emit a second event");

        kbd.Read (0xC061);

        Assert::AreEqual (3, sink.buttonReadCount,
            L"A different button address coalesces independently and must emit");
    }

    TEST_METHOD (PaddleTrigger_FiresOnEveryStrobe)
    {
        Apple2eSoftSwitchBank  bank;
        RecordingInputSink     sink;
        uint64_t               cycles = 0;

        bank.SetCpuCycleSource (&cycles);
        bank.SetInputEventSink (&sink);

        bank.Read (0xC070);
        bank.Read (0xC070);

        Assert::AreEqual (2, sink.paddleTriggerCount,
            L"Each $C070 PTRIG strobe must emit its own trigger event (no coalescing)");
        Assert::AreEqual (static_cast<uint16_t> (0xC070), sink.lastAddress,
            L"The trigger event must carry the PTRIG strobe address");
    }

    TEST_METHOD (PaddleRead_CoalescesUntilTimerExpires)
    {
        Apple2eSoftSwitchBank  bank;
        RecordingInputSink     sink;
        uint64_t               cycles = 1000;

        bank.SetCpuCycleSource (&cycles);
        bank.SetInputEventSink (&sink);
        bank.SetPaddle (0, 100);

        // Arm the game-port timers at cycle 1000; axis 0 holds bit 7 for
        // position * 11 = 1100 cycles.
        bank.Read (0xC070);

        bank.Read (0xC064);
        bank.Read (0xC064);

        Assert::AreEqual (1, sink.paddleReadCount,
            L"Identical $C064 polls while the timer is counting must coalesce to one event");
        Assert::AreEqual (static_cast<uint8_t> (0x80), sink.lastValue,
            L"A still-counting axis must report bit 7 set");

        cycles = 1000 + 100 * 11;
        bank.Read (0xC064);

        Assert::AreEqual (2, sink.paddleReadCount,
            L"The timer expiring (bit 7 -> 0) is a new edge and must emit a second event");
        Assert::AreEqual (static_cast<uint8_t> (0x00), sink.lastValue,
            L"An expired axis must report bit 7 clear");
    }

    TEST_METHOD (PaddleRead_DetachedSink_Silenced)
    {
        Apple2eSoftSwitchBank  bank;
        RecordingInputSink     sink;
        uint64_t               cycles = 0;

        bank.SetCpuCycleSource (&cycles);
        bank.SetInputEventSink (&sink);
        bank.Read (0xC070);

        int  baseline = sink.paddleTriggerCount;

        bank.SetInputEventSink (nullptr);
        bank.Read (0xC070);
        bank.Read (0xC064);

        Assert::AreEqual (baseline, sink.paddleTriggerCount,
            L"No paddle callbacks may fire once the sink is detached");
        Assert::AreEqual (0, sink.paddleReadCount,
            L"No paddle-read callbacks may fire once the sink is detached");
    }
};
