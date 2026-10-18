#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/Reverse/HeldInputWatch.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInputReadTests
//
//  Input the user gives while the machine replays history is held back, and
//  the question whether to discard the history ahead waits until the guest
//  actually reads a line the held input would change. Each test records a
//  real machine running a small guest loop, rewinds into the recording, and
//  replays it with the watch on: the replay stops after the first read that
//  would differ, and runs on to live without stopping when no read would.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HeldInputReadTests)
{
public:

    static constexpr Word      kStart          = 0x0800;
    static constexpr uint64_t  kRecordCycles   = 120000;
    static constexpr uint64_t  kSliceCycles    = 1023;
    static constexpr uint64_t  kJoyportSettle  = SiriusJoyport::kReleaseCycles + 20000;
    static constexpr Byte      kHeldKey        = 0xD1;     // 'Q', strobe set
    static constexpr Byte      kRecordedPaddle = 50;
    static constexpr Byte      kHeldPaddle     = 100;


    //  The rig polls $C000 at the top of its loop. A key held behind live is
    //  in question at the first read of the latch, made by the LDA $C000 at
    //  the loop top; the machine put back at that read's position is the
    //  recorded machine there, about to make it.
    TEST_METHOD (AHeldKeyStopsAtTheFirstKeyboardRead)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        HeldInputWatch     watch;
        HeldInputLines     lines;
        uint64_t           start      = 0;
        uint64_t           expected   = 0;
        uint64_t           checksum   = 0;



        RecordRig (machine, controller);

        start    = GetPointIn (machine, controller, 2);
        expected = FindFirstAt (machine, controller, start, ReverseSessionRig::kLoopTop, checksum);

        Rewind (machine, controller, start);

        lines.keyLatch = kHeldKey;
        RunWatched (machine, controller, watch, lines);

        Assert::IsTrue (watch.HasHit(), L"the guest read the keyboard");
        Assert::AreEqual<uint64_t> (expected, watch.GetHitPosition(), L"at the first LDA $C000");
        Assert::AreEqual<uint64_t> (expected + 1, machine.GetPosition(), L"the run ended after that instruction");
        Assert::IsTrue (controller.IsInHistory(), L"still replaying");

        Rewind (machine, controller, watch.GetHitPosition());

        Assert::AreEqual<Word>     (ReverseSessionRig::kLoopTop, machine.GetCpu()->GetPC(), L"put back before the read");
        Assert::AreEqual<uint64_t> (checksum, ReverseSessionRig::Checksum (machine), L"as the recording has it there");
    }


    //  A key let go of before the guest reads it still left its key waiting.
    //  The latch is held, the key-down line is not, and the read still stops.
    TEST_METHOD (AKeyLetGoOfBeforeTheReadStillStopsThere)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        HeldInputWatch     watch;
        HeldInputLines     lines;



        RecordRig (machine, controller);
        Rewind    (machine, controller, GetPointIn (machine, controller, 2));

        lines.keyLatch  = kHeldKey;
        lines.isKeyDown = false;
        RunWatched (machine, controller, watch, lines);

        Assert::IsTrue (watch.HasHit(), L"the key is still waiting at the read");
    }


    //  A guest that never reads the keyboard, the game port or the mouse is
    //  never in question: the replay runs on to live, where the held input
    //  simply goes in.
    TEST_METHOD (NoReadMeansNoStopAllTheWayToLive)
    {
        static constexpr Byte  kCounting[] =
        {
            0xEE, 0x04, 0x03,       // 0800  INC $0304
            0x4C, 0x00, 0x08,       // 0803  JMP $0800
        };
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        HeldInputWatch     watch;
        HeldInputLines     lines;



        RecordProgram (machine, controller, kCounting, sizeof (kCounting), 0);
        Rewind        (machine, controller, GetPointIn (machine, controller, 4));

        lines.keyLatch    = kHeldKey;
        lines.isKeyDown   = true;
        lines.buttons[0]  = true;
        lines.paddles[0]  = kHeldPaddle;
        lines.mouseButton = true;
        RunWatched (machine, controller, watch, lines);

        Assert::IsFalse (watch.HasHit(), L"nothing read the held lines");
        Assert::IsFalse (controller.IsInHistory(), L"the replay reached live");
    }


    //  $C010's bit 7 is any-key-down. A key the host still holds down is in
    //  question there even with no key latched.
    TEST_METHOD (AKeyStillDownStopsAtTheStrobeRead)
    {
        static constexpr Byte  kStrobe[] =
        {
            0xAD, 0x10, 0xC0,       // 0800  LDA $C010
            0x8D, 0x00, 0x03,       // 0803  STA $0300
            0x4C, 0x00, 0x08,       // 0806  JMP $0800
        };
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        HeldInputWatch     watch;
        HeldInputLines     lines;
        uint64_t           start      = 0;
        uint64_t           expected   = 0;
        uint64_t           checksum   = 0;



        RecordProgram (machine, controller, kStrobe, sizeof (kStrobe), 0);

        start    = GetPointIn (machine, controller, 2);
        expected = FindFirstAt (machine, controller, start, kStart, checksum);

        Rewind (machine, controller, start);

        lines.isKeyDown = true;
        RunWatched (machine, controller, watch, lines);

        Assert::IsTrue (watch.HasHit());
        Assert::AreEqual<uint64_t> (expected, watch.GetHitPosition(), L"at the first LDA $C010");
    }


    //  The rig times paddle 0 against a recorded position of 50. Held at 100,
    //  every read of the timer while both one-shots still run agrees with the
    //  recording; the first that differs is the one where the recorded timer
    //  has run out and the held one has not, which ends the guest's count.
    TEST_METHOD (AHeldPaddleStopsAtTheFirstTimerReadThatDiffers)
    {
        static constexpr Word  kPaddleRead = 0x0816;
        static constexpr Word  kCountDone  = 0x081E;
        TestMachine            machine    ("Apple2e");
        ReverseController      controller (machine);
        HeldInputWatch         watch;
        HeldInputLines         lines;
        uint64_t               start      = 0;
        uint64_t               expected   = 0;
        Byte                   x          = 0;



        RecordRig (machine, controller);

        start = GetPointIn (machine, controller, 2);
        Rewind (machine, controller, start);

        // The recording's own answer: the last timer read before the guest's
        // count is done, which is the first whose recorded timer has run out.
        while (machine.GetCpu()->GetPC() != kCountDone)
        {
            if (machine.GetCpu()->GetPC() == kPaddleRead)
            {
                expected = machine.GetPosition();
            }

            machine.StepOne();
        }

        Rewind (machine, controller, expected);
        x = machine.GetCpu()->GetCpu6502()->GetRegisters().x;

        Assert::IsTrue (x > 0, L"not the first read after the trigger");

        Rewind (machine, controller, start);

        lines.paddles[0] = kHeldPaddle;
        RunWatched (machine, controller, watch, lines);

        Assert::IsTrue (watch.HasHit());
        Assert::AreEqual<uint64_t> (expected, watch.GetHitPosition(), L"the read that ends the recorded count");
    }


    //  A paddle held where the recording already has it changes no read.
    TEST_METHOD (APaddleHeldWhereTheRecordingHasItDoesNotStop)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        HeldInputWatch     watch;
        HeldInputLines     lines;



        RecordRig (machine, controller);
        Rewind    (machine, controller, GetPointIn (machine, controller, 2));

        lines.paddles[0] = kRecordedPaddle;
        RunWatched (machine, controller, watch, lines);

        Assert::IsFalse (watch.HasHit(), L"every read agrees with the recording");
        Assert::IsFalse (controller.IsInHistory(), L"so the replay reached live");
    }


    //  Open Apple is the //e's PB0. Held down, the first $C061 read stops;
    //  a paddle held behind live is not in question at that read.
    TEST_METHOD (AHeldButtonStopsAtTheFirstButtonRead)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        HeldInputWatch     watch;
        HeldInputLines     lines;
        uint64_t           start      = 0;
        uint64_t           expected   = 0;
        uint64_t           checksum   = 0;



        RecordProgram (machine, controller, s_kButtonPoll, sizeof (s_kButtonPoll), 0);

        start    = GetPointIn (machine, controller, 2);
        expected = FindFirstAt (machine, controller, start, kStart, checksum);

        Rewind (machine, controller, start);

        lines.paddles[1] = kHeldPaddle;
        RunWatched (machine, controller, watch, lines);

        Assert::IsFalse (watch.HasHit(), L"the guest never reads paddle 1");

        Rewind (machine, controller, start);

        lines.buttons[0] = true;
        RunWatched (machine, controller, watch, lines);

        Assert::IsTrue (watch.HasHit());
        Assert::AreEqual<uint64_t> (expected, watch.GetHitPosition(), L"at the first LDA $C061");
    }


    //  With the Joyport attached the $C061 read is its left jack's fire
    //  switch. A closed fire switch held there stops at that read; a closed
    //  switch on the other jack does not, since AN0 selects the left one.
    TEST_METHOD (AHeldJoyportSwitchStopsAtTheReadOfItsJack)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        HeldInputWatch     watch;
        HeldInputLines     lines;
        JoystickSwitches   fire;
        uint64_t           start      = 0;
        uint64_t           expected   = 0;
        uint64_t           checksum   = 0;



        Load (machine, s_kButtonPoll, sizeof (s_kButtonPoll));

        machine.GetJoyport()->SetAttached (true);
        machine.RunCycles (kJoyportSettle);

        Record (machine, controller);

        start    = GetPointIn (machine, controller, 2);
        expected = FindFirstAt (machine, controller, start, kStart, checksum);

        fire.set (static_cast<size_t> (JoystickSwitch::Fire));

        Rewind (machine, controller, start);

        lines.jacks[JoyportJacks::kRightJack] = fire;
        RunWatched (machine, controller, watch, lines);

        Assert::IsFalse (watch.HasHit(), L"the right jack is not the one read");

        Rewind (machine, controller, start);

        lines.jacks[JoyportJacks::kLeftJack] = fire;
        RunWatched (machine, controller, watch, lines);

        Assert::IsTrue (watch.HasHit());
        Assert::AreEqual<uint64_t> (expected, watch.GetHitPosition(), L"at the first read of the left jack's fire switch");
    }


    //  The //c's mouse button is $C063, active low. Held down, the first read
    //  stops there.
    TEST_METHOD (AHeldMouseButtonStopsAtTheFirstButtonRead)
    {
        static constexpr Byte  kMousePoll[] =
        {
            0xAD, 0x63, 0xC0,       // 0800  LDA $C063
            0x8D, 0x00, 0x03,       // 0803  STA $0300
            0x4C, 0x00, 0x08,       // 0806  JMP $0800
        };
        TestMachine        machine    ("Apple2c", TestMachine::Slots::Empty);
        ReverseController  controller (machine);
        HeldInputWatch     watch;
        HeldInputLines     lines;
        uint64_t           start      = 0;
        uint64_t           expected   = 0;
        uint64_t           checksum   = 0;



        RecordProgram (machine, controller, kMousePoll, sizeof (kMousePoll), 0);

        start    = GetPointIn (machine, controller, 2);
        expected = FindFirstAt (machine, controller, start, kStart, checksum);

        Rewind (machine, controller, start);

        lines.mouseButton = true;
        RunWatched (machine, controller, watch, lines);

        Assert::IsTrue (watch.HasHit());
        Assert::AreEqual<uint64_t> (expected, watch.GetHitPosition(), L"at the first LDA $C063");
    }


    //  The host pointer enters the //c only through the mouse's retarget pass,
    //  and only once the guest has set the mouse up: with the firmware's
    //  screen holes still garbage nothing moves, so a held pointer is never
    //  in question. With the holes live, the first pass stops.
    TEST_METHOD (AHeldPointerStopsOnlyOnceTheGuestHasTheMouseUp)
    {
        static constexpr Byte  kCounting[] =
        {
            0xEE, 0x04, 0x03,       // 0800  INC $0304
            0x4C, 0x00, 0x08,       // 0803  JMP $0800
        };
        static constexpr uint32_t  kFarCorner = 0xFFFFFFFF;

        for (bool isMouseUp : { false, true })
        {
            TestMachine        machine    ("Apple2c", TestMachine::Slots::Empty);
            ReverseController  controller (machine);
            HeldInputWatch     watch;
            HeldInputLines     lines;



            Load (machine, kCounting, sizeof (kCounting));

            if (isMouseUp)
            {
                SetUpMouseHoles (machine);
            }
            else
            {
                ClearMouseHoles (machine);
            }

            Record (machine, controller);
            Rewind (machine, controller, GetPointIn (machine, controller, 4));

            lines.mouseTarget = kFarCorner;
            RunWatched (machine, controller, watch, lines);

            Assert::AreEqual (isMouseUp, watch.HasHit(), isMouseUp ? L"the pass would move the pointer" : L"nothing set the mouse up");
        }
    }


    //  The whole exchange as the emulator makes it, on the rig. The replay
    //  stops after the first keyboard read, and is put back where that read
    //  began. A yes cuts history there and the key goes in live, so the same
    //  instruction reads it; a no leaves history alone and the instruction
    //  reads what the recording has.
    TEST_METHOD (AYesAtTheReadGoesLiveThereAndTheReadSeesTheKey)
    {
        for (bool isYes : { true, false })
        {
            TestMachine     machine  ("Apple2e");
            ReverseHost     host     (machine);
            HeldInputWatch  watch;
            HeldInputLines  lines;
            ReverseResult   result;
            HRESULT         hr       = S_OK;
            uint64_t        liveEnd  = 0;
            Byte            read     = 0;



            ReverseSessionRig::Prepare (machine);

            hr = host.StartRecording (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"StartRecording");

            machine.RunCycles (kRecordCycles);
            liveEnd = machine.GetPosition();

            hr = host.Execute (ReverseCommand::Seek, GetPointIn (machine, host.GetController(), 2), nullptr, result);
            AssertSucceeded (hr, L"Seek");

            lines.keyLatch = kHeldKey;
            RunWatched (machine, host.GetController(), watch, lines);

            Assert::IsTrue (watch.HasHit());

            hr = host.Execute (ReverseCommand::Seek, watch.GetHitPosition(), nullptr, result);
            AssertSucceeded (hr, L"Seek back to the read");

            Assert::AreEqual<Word> (ReverseSessionRig::kLoopTop, machine.GetCpu()->GetPC(), L"about to read the keyboard");

            if (isYes)
            {
                hr = host.Diverge();
                AssertSucceeded (hr, L"Diverge");

                machine.GetRefs().keyboard->PressKey ('Q');
            }

            machine.StepOne();
            read = machine.GetCpu()->GetCpu6502()->GetRegisters().a;

            if (isYes)
            {
                Assert::IsFalse (host.IsBehindLive(), L"live from the read on");
                Assert::AreEqual<Byte> (kHeldKey, read, L"the read saw the key");
            }
            else
            {
                Assert::IsTrue  (host.IsBehindLive(), L"still replaying");
                Assert::AreEqual<uint64_t> (liveEnd, host.GetController().GetLiveEndPosition(), L"history kept");
                Assert::IsTrue  (read != kHeldKey, L"the read saw the recording");
            }
        }
    }

    //  The watch keeps the first read only, and a refresh forgets it.
    TEST_METHOD (TheFirstDifferenceIsKeptUntilTheNextRefresh)
    {
        static constexpr uint64_t  kFirst   = 100;
        static constexpr uint64_t  kSecond  = 200;
        HeldInputWatch             watch;
        HeldInputLines             lines;
        uint64_t                   position = kFirst;



        lines.keyLatch = kHeldKey;
        watch.Publish (lines);

        Assert::IsTrue (watch.Refresh(), L"a key is held");

        watch.SetPositionSource (&position);
        watch.CheckLatch (0);

        position = kSecond;
        watch.CheckLatch (0);

        Assert::IsTrue (watch.HasHit());
        Assert::AreEqual<uint64_t> (kFirst, watch.GetHitPosition());

        watch.Publish (HeldInputLines());

        Assert::IsFalse (watch.Refresh(), L"nothing held");
        Assert::IsFalse (watch.HasHit(),  L"and the earlier read forgotten");
    }


    //  A read whose value the held line would leave unchanged is no
    //  difference: the same key already waiting, a button as recorded, two
    //  paddle timers on the same side of the read.
    TEST_METHOD (AReadTheHeldLineLeavesAloneIsNoDifference)
    {
        constexpr uint64_t  kCyclesPerUnit = 11;
        HeldInputWatch      watch;
        HeldInputLines      lines;



        lines.keyLatch   = kHeldKey;
        lines.buttons[1] = true;
        lines.paddles[2] = kHeldPaddle;
        watch.Publish (lines);
        watch.Refresh();

        watch.CheckLatch   (kHeldKey);
        watch.CheckButton  (1, true);
        watch.CheckButton  (0, false);
        watch.CheckPaddle  (2, kRecordedPaddle, 0, kCyclesPerUnit);
        watch.CheckPaddle  (2, kRecordedPaddle, kHeldPaddle * kCyclesPerUnit, kCyclesPerUnit);
        watch.CheckKeyDown (false);

        Assert::IsFalse (watch.HasHit());

        watch.CheckPaddle (2, kRecordedPaddle, kRecordedPaddle * kCyclesPerUnit, kCyclesPerUnit);

        Assert::IsTrue (watch.HasHit(), L"the recorded timer has run out and the held one has not");
    }


private:

    static constexpr Byte  s_kButtonPoll[] =
    {
        0xAD, 0x61, 0xC0,       // 0800  LDA $C061
        0x8D, 0x00, 0x03,       // 0803  STA $0300
        0x4C, 0x00, 0x08,       // 0806  JMP $0800
    };


    static void Load (TestMachine & machine, const Byte * program, size_t size)
    {
        size_t  i = 0;



        machine.PowerCycle();

        for (i = 0; i < size; i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (kStart + i), program[i]);
        }

        machine.GetCpu()->SetPC (kStart);
    }


    static void Record (TestMachine & machine, ReverseController & controller)
    {
        HRESULT  hr = S_OK;



        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (kRecordCycles);
    }


    static void RecordProgram (TestMachine & machine, ReverseController & controller, const Byte * program, size_t size, uint64_t settle)
    {
        Load (machine, program, size);

        if (settle > 0)
        {
            machine.RunCycles (settle);
        }

        Record (machine, controller);
    }


    //  The rig's loop with paddle 0 recorded at kRecordedPaddle and no key.
    static void RecordRig (TestMachine & machine, ReverseController & controller)
    {
        ReverseSessionRig::Prepare (machine);

        machine.GetRefs().iieSoftSwitches->SetPaddle (0, kRecordedPaddle);

        Record (machine, controller);
    }


    //  A point the given fraction of the way into the recorded history.
    static uint64_t GetPointIn (TestMachine & machine, ReverseController & controller, uint64_t divisor)
    {
        uint64_t  oldest = controller.GetOldestPosition();



        return oldest + (machine.GetPosition() - oldest) / divisor;
    }


    static void Rewind (TestMachine & machine, ReverseController & controller, uint64_t position)
    {
        ReverseResult  result;
        HRESULT        hr     = S_OK;



        hr = controller.SeekToPosition (position, result);
        AssertSucceeded (hr, L"SeekToPosition");

        Assert::AreEqual<uint64_t> (position, machine.GetPosition(), L"landed where asked");
        Assert::IsTrue (controller.IsInHistory(), L"behind live");
    }


    //  The first position at or after start whose instruction begins at pc,
    //  and the recorded machine's checksum there, found by replaying with no
    //  watch on.
    static uint64_t FindFirstAt (TestMachine & machine, ReverseController & controller, uint64_t start, Word pc, uint64_t & outChecksum)
    {
        Rewind (machine, controller, start);

        while (machine.GetCpu()->GetPC() != pc)
        {
            machine.StepOne();
        }

        outChecksum = ReverseSessionRig::Checksum (machine);

        return machine.GetPosition();
    }


    //  Replays with the watch on, as the emulator runs slices, until a read
    //  differs or the machine is live.
    static void RunWatched (TestMachine & machine, ReverseController & controller, HeldInputWatch & watch, const HeldInputLines & lines)
    {
        watch.Publish (lines);
        watch.Refresh();

        machine.SetHeldInputWatch (&watch);

        while (!watch.HasHit() && controller.IsInHistory())
        {
            machine.RunCycles (kSliceCycles);
        }

        machine.SetHeldInputWatch (nullptr);
    }


    //  The mouse firmware's screen holes as INITMOUSE leaves them: a clamp
    //  window of 0..1023 on each axis with the pointer at its origin.
    static void SetUpMouseHoles (TestMachine & machine)
    {
        static constexpr Word  kHoles[]  = { 0x047D, 0x057D, 0x04FD, 0x05FD, 0x047F, 0x057F, 0x04FF, 0x05FF };
        static constexpr Word  kMaxLo[]  = { 0x067D, 0x06FD };
        static constexpr Word  kMaxHi[]  = { 0x077D, 0x07FD };
        static constexpr Byte  kMaxLow   = 0xFF;
        static constexpr Byte  kMaxHigh  = 0x03;

        for (Word hole : kHoles)
        {
            machine.GetMemoryBus().WriteByte (hole, 0);
        }

        for (Word hole : kMaxLo)
        {
            machine.GetMemoryBus().WriteByte (hole, kMaxLow);
        }

        for (Word hole : kMaxHi)
        {
            machine.GetMemoryBus().WriteByte (hole, kMaxHigh);
        }
    }


    //  Holes no INITMOUSE has set: an empty clamp window.
    static void ClearMouseHoles (TestMachine & machine)
    {
        static constexpr Word  kHoles[] = { 0x047D, 0x057D, 0x04FD, 0x05FD, 0x047F, 0x057F, 0x04FF, 0x05FF,
                                            0x067D, 0x077D, 0x06FD, 0x07FD };

        for (Word hole : kHoles)
        {
            machine.GetMemoryBus().WriteByte (hole, 0);
        }
    }
};
