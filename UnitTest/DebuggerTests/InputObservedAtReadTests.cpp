#include "Pch.h"

#include "Core/StateWriter.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"

#include "EmuTests/TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


//
//  Cycles the reads below are made at. The first sits past the Joyport's
//  reset window, so an attached Joyport answers every button read; a paddle
//  is read s_kObservedPaddleSpan cycles after its trigger, when a centered
//  paddle has timed out and one pushed to s_kObservedPaddleHigh is still
//  timing.
//
static constexpr uint64_t  s_kObservedBaselineCycle = 1'000'000;
static constexpr uint64_t  s_kObservedFirstCycle    = 1'010'000;
static constexpr uint64_t  s_kObservedSecondCycle   = 1'020'000;
static constexpr uint64_t  s_kObservedPaddleSpan    = 1'500;
static constexpr Byte      s_kObservedPaddleHigh    = 200;
static constexpr int       s_kObservedButtonCount   = 3;
static constexpr Word      s_kObservedPaddleTrigger = 0xC070;
static constexpr int       s_kObservedMoveX         = 5;
static constexpr int       s_kObservedMoveY         = -3;
static constexpr uint16_t  s_kObservedTargetX       = 0x4000;
static constexpr uint16_t  s_kObservedTargetY       = 0xC000;





////////////////////////////////////////////////////////////////////////////////
//
//  InputObservedAtReadTests
//
//  Host input reaches the devices from the UI and controller threads; with
//  the journal on, each device records a new value at the first CPU-thread
//  read that sees it, stamped with that read's cycle. Each case writes a
//  value as another thread would, reads twice, and checks for exactly one
//  record at the first read. It then applies the records to a second machine
//  that never saw the write and checks the same reads return the same bytes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InputObservedAtReadTests)
{
public:

    TEST_METHOD (KeyboardDataReadRecordsTheLatch)
    {
        CheckObserved ("Apple2e", 0xC000,
                       [] (MachineHost & m) { m.GetRefs().keyboard->PressKey ('A'); },
                       InputKind::KeyLatch, 'A' | 0x80, 0, 0);
    }


    TEST_METHOD (KeyboardStrobeReadRecordsTheLatch)
    {
        CheckObserved ("Apple2e", 0xC010,
                       [] (MachineHost & m) { m.GetRefs().keyboard->PressKey ('B'); },
                       InputKind::KeyLatch, 'B' | 0x80, 0, 0);
    }


    TEST_METHOD (KeyboardStrobeReadRecordsAnyKeyDown)
    {
        CheckObserved ("Apple2e", 0xC010,
                       [] (MachineHost & m) { m.GetRefs().keyboard->SetKeyDown (true); },
                       InputKind::KeyDown, 1, 0, 0);
    }


    TEST_METHOD (IIPlusKeyboardDataReadRecordsTheLatch)
    {
        CheckObserved ("Apple2Plus", 0xC000,
                       [] (MachineHost & m) { m.GetRefs().keyboard->PressKey ('Q'); },
                       InputKind::KeyLatch, 'Q' | 0x80, 0, 0);
    }


    TEST_METHOD (StatusReadRecordsTheLatchItsLowBitsShow)
    {
        CheckObserved ("Apple2e", 0xC01F,
                       [] (MachineHost & m) { m.GetRefs().keyboard->PressKey ('C'); },
                       InputKind::KeyLatch, 'C' | 0x80, 0, 0);
    }


    TEST_METHOD (OpenAppleReadRecordsTheKey)
    {
        CheckObserved ("Apple2e", 0xC061,
                       [] (MachineHost & m) { m.GetRefs().iieKeyboard->SetOpenApple (true); },
                       InputKind::Button, 1, static_cast<uint16_t> (InputLine::OpenApple), 0);
    }


    TEST_METHOD (ClosedAppleReadRecordsTheKey)
    {
        CheckObserved ("Apple2e", 0xC062,
                       [] (MachineHost & m) { m.GetRefs().iieKeyboard->SetClosedApple (true); },
                       InputKind::Button, 1, static_cast<uint16_t> (InputLine::ClosedApple), 0);
    }


    TEST_METHOD (ShiftReadRecordsTheKey)
    {
        CheckObserved ("Apple2e", 0xC063,
                       [] (MachineHost & m) { m.GetRefs().iieKeyboard->SetShift (true); },
                       InputKind::Button, 1, static_cast<uint16_t> (InputLine::Shift), 0);
    }


    TEST_METHOD (IIPlusButtonReadsRecordEachButton)
    {
        for (int button = 0; button < s_kObservedButtonCount; button++)
        {
            CheckObserved ("Apple2Plus", static_cast<Word> (0xC061 + button),
                           [button] (MachineHost & m) { m.GetRefs().gamePort->SetButton (button, true); },
                           InputKind::Button, 1, static_cast<uint16_t> (static_cast<int> (InputLine::GamePortButton0) + button), 0);
        }
    }


    TEST_METHOD (IIePaddleReadRecordsThePosition)
    {
        CheckObserved ("Apple2e", 0xC064,
                       [] (MachineHost & m) { m.GetRefs().iieSoftSwitches->SetPaddle (0, s_kObservedPaddleHigh); },
                       InputKind::Paddle, s_kObservedPaddleHigh, 0, 0);
    }


    TEST_METHOD (IIPlusPaddleReadRecordsThePosition)
    {
        CheckObserved ("Apple2Plus", 0xC065,
                       [] (MachineHost & m) { m.GetRefs().gamePort->SetPaddle (1, s_kObservedPaddleHigh); },
                       InputKind::Paddle, s_kObservedPaddleHigh, 1, 0);
    }


    TEST_METHOD (MouseButtonReadRecordsTheButton)
    {
        CheckObserved ("Apple2c", 0xC063,
                       [] (MachineHost & m) { m.GetMouse()->SetButton (true); },
                       InputKind::MouseButton, 1, 0, 0);
    }


    TEST_METHOD (EightyColumnSwitchReadRecordsTheSwitch)
    {
        CheckObserved ("Apple2c", 0xC060,
                       [] (MachineHost & m) { m.GetRefs().iieKeyboard->SetEightyColumnSwitchIn (true); },
                       InputKind::Button, 1, static_cast<uint16_t> (InputLine::EightyColumnSwitch), 0);
    }


    TEST_METHOD (JoyportButtonReadRecordsAttaching)
    {
        CheckObserved ("Apple2Plus", 0xC061,
                       [] (MachineHost & m) { m.GetJoyport()->SetAttached (true); },
                       InputKind::Button, 1, static_cast<uint16_t> (InputLine::JoyportAttached), 0);
    }


    TEST_METHOD (JoyportButtonReadRecordsTheJack)
    {
        JoystickSwitches  fire;



        fire.set (static_cast<size_t> (JoystickSwitch::Fire));

        CheckObserved ("Apple2Plus", 0xC061,
                       [fire] (MachineHost & m) { m.GetJoyport()->SetJackSwitches (JoyportJacks::kLeftJack, fire); },
                       InputKind::JoyportJack, 0, static_cast<uint16_t> (JoyportJacks::kLeftJack), fire.to_ulong(),
                       [] (MachineHost & m) { m.GetJoyport()->SetAttached (true); });
    }


    TEST_METHOD (JoyportPaddleReadRecordsTheRearSockets)
    {
        CheckObserved ("Apple2e", 0xC064,
                       [] (MachineHost & m) { m.GetJoyport()->SetPaddlesConnected (true); },
                       InputKind::Button, 1, static_cast<uint16_t> (InputLine::JoyportPaddlesConnected), 0,
                       [] (MachineHost & m) { m.GetJoyport()->SetAttached (true); });
    }


    //  The mouse drains host motion in its cycle tick, not at a bus read, so
    //  the record is stamped with the cycle the ticking instruction began at.
    TEST_METHOD (MouseTickRecordsTheMotionItDrains)
    {
        TestMachine          live    ("Apple2c", TestMachine::Slots::Empty);
        TestMachine          replay  ("Apple2c", TestMachine::Slots::Empty);
        const InputRecord  * record  = nullptr;
        uint64_t             data    = 0;



        live.SetInputJournalOn (true);

        SetCycle (live, s_kObservedFirstCycle);
        live.GetMouse()->MoveBy (s_kObservedMoveX, s_kObservedMoveY);
        TickMouse (live);

        Assert::AreEqual<size_t> (1, live.GetInputJournal().GetEndIndex(), L"one record for the motion the tick drained");

        record = &live.GetInputJournal().GetRecord (0);
        data   = (static_cast<uint64_t> (static_cast<uint32_t> (s_kObservedMoveX)) << 32) | static_cast<uint32_t> (s_kObservedMoveY);

        Assert::IsTrue (record->kind == InputKind::MouseMove);
        Assert::AreEqual<uint64_t> (s_kObservedFirstCycle,    record->cycle, L"stamped at the start of the ticking instruction");
        Assert::AreEqual<uint64_t> (data,                     record->data);

        SetCycle (replay, s_kObservedFirstCycle);
        Assert::IsTrue (replay.ApplyDeviceInput (*record), L"the mouse applies a motion record");
        TickMouse (replay);

        Assert::AreEqual<Byte> (live.GetMouse()->ReadMouX1(),            replay.GetMouse()->ReadMouX1(),            L"X direction");
        Assert::AreEqual<Byte> (live.GetMouse()->ReadMouY1(),            replay.GetMouse()->ReadMouY1(),            L"Y direction");
        Assert::AreEqual<Byte> (live.GetMouse()->ReadXInterruptStatus(), replay.GetMouse()->ReadXInterruptStatus(), L"X latch");
        Assert::AreEqual<Byte> (0x80,                                    replay.GetMouse()->ReadXInterruptStatus(), L"the replayed motion latched");
    }


    TEST_METHOD (MouseRetargetRecordsTheTarget)
    {
        TestMachine          live    ("Apple2c", TestMachine::Slots::Empty);
        TestMachine          replay  ("Apple2c", TestMachine::Slots::Empty);
        const InputRecord  * record  = nullptr;
        uint32_t             packed  = (static_cast<uint32_t> (s_kObservedTargetX) << 16) | s_kObservedTargetY;



        live.SetInputJournalOn (true);

        SetCycle (live, s_kObservedFirstCycle);
        live.GetMouse()->SetHostTargetFraction (s_kObservedTargetX, s_kObservedTargetY);
        TickMouse (live);

        Assert::AreEqual<size_t> (1, live.GetInputJournal().GetEndIndex(), L"one record for the target the retarget pass read");

        record = &live.GetInputJournal().GetRecord (0);

        Assert::IsTrue (record->kind == InputKind::MouseTarget);
        Assert::AreEqual<uint64_t> (s_kObservedFirstCycle, record->cycle);
        Assert::AreEqual<Byte>     (1,                     record->value);
        Assert::AreEqual<uint64_t> (packed,                record->data);

        SetCycle (replay, s_kObservedFirstCycle);
        Assert::IsTrue (replay.ApplyDeviceInput (*record), L"the mouse applies a target record");
        TickMouse (replay);

        Assert::IsTrue (SaveMouse (live) == SaveMouse (replay), L"the replayed mouse holds the same target and state");
    }


    //  A key the CPU thread latches itself is journaled by its caller, so the
    //  read that sees it adds nothing.
    TEST_METHOD (AKeyLatchedOnTheCpuThreadIsNotRecordedAgain)
    {
        TestMachine  live ("Apple2e", TestMachine::Slots::Empty);



        live.SetInputJournalOn (true);

        SetCycle (live, s_kObservedFirstCycle);
        live.GetRefs().keyboard->PressKeyOnCpuThread ('P');
        live.GetMemoryBus().ReadByte (0xC000);

        Assert::AreEqual<size_t> (0, live.GetInputJournal().GetEndIndex());
    }


    //  Turning the journal off detaches it from the devices: with the journal
    //  switched back on but not reattached, a read records nothing.
    TEST_METHOD (TurningTheJournalOffDetachesTheDevices)
    {
        TestMachine  live ("Apple2e", TestMachine::Slots::Empty);



        live.SetInputJournalOn (true);
        live.SetInputJournalOn (false);
        live.GetInputJournal().SetOn (true);

        SetCycle (live, s_kObservedFirstCycle);
        live.GetRefs().keyboard->PressKey ('Z');
        live.GetMemoryBus().ReadByte (0xC000);

        Assert::AreEqual<size_t> (0, live.GetInputJournal().GetEndIndex());
    }


private:

    using MachineAction = std::function<void (MachineHost &)>;


    //  Both machines are set up alike, read once at the baseline, and only
    //  the live one then sees the write. Its records are applied to the other
    //  just before the first read, and every read must match.
    static void CheckObserved (
        const char           * machineId,
        Word                   address,
        const MachineAction  & write,
        InputKind              kind,
        Byte                   value,
        uint16_t               detail,
        uint64_t               data,
        const MachineAction  & setUp = nullptr)
    {
        TestMachine          live      (machineId, TestMachine::Slots::Empty);
        TestMachine          replay    (machineId, TestMachine::Slots::Empty);
        InputJournal       & journal    = live.GetInputJournal();
        const InputRecord  * record     = nullptr;
        Byte                 liveFirst  = 0;
        Byte                 liveSecond = 0;
        size_t               i          = 0;
        std::wstring         where      = std::format (L"{} ${:04X}: ", std::wstring (machineId, machineId + strlen (machineId)), address);



        if (setUp)
        {
            setUp (live);
            setUp (replay);
        }

        live.SetInputJournalOn (true);

        Assert::AreEqual<Byte> (ReadAt (live, address, s_kObservedBaselineCycle), ReadAt (replay, address, s_kObservedBaselineCycle),
                                (where + L"the two machines must agree before the write").c_str());
        Assert::AreEqual<size_t> (0, journal.GetEndIndex(), (where + L"nothing has changed yet").c_str());

        write (live);

        liveFirst  = ReadAt (live, address, s_kObservedFirstCycle);
        liveSecond = ReadAt (live, address, s_kObservedSecondCycle);

        Assert::AreEqual<size_t> (1, journal.GetEndIndex(), (where + L"exactly one record, at the first read that sees the write").c_str());

        record = &journal.GetRecord (0);

        Assert::IsTrue             (record->kind == kind,                   (where + L"kind").c_str());
        Assert::AreEqual<uint64_t> (s_kObservedFirstCycle, record->cycle,   (where + L"stamped with the first read's cycle").c_str());
        Assert::AreEqual<Byte>     (value,                 record->value,   (where + L"value").c_str());
        Assert::AreEqual<uint16_t> (detail,                record->detail,  (where + L"detail").c_str());
        Assert::AreEqual<uint64_t> (data,                  record->data,    (where + L"data").c_str());

        for (i = journal.GetBeginIndex(); i < journal.GetEndIndex(); i++)
        {
            Assert::IsTrue (replay.ApplyDeviceInput (journal.GetRecord (i)), (where + L"a device applies the record").c_str());
        }

        Assert::AreEqual<Byte> (liveFirst,  ReadAt (replay, address, s_kObservedFirstCycle),  (where + L"the replayed first read").c_str());
        Assert::AreEqual<Byte> (liveSecond, ReadAt (replay, address, s_kObservedSecondCycle), (where + L"the replayed second read").c_str());
    }


    //  A paddle address is read a fixed span after a trigger, so the value
    //  depends on the staged position: a centered paddle has timed out by
    //  then and one at s_kObservedPaddleHigh has not.
    static Byte ReadAt (MachineHost & machine, Word address, uint64_t cycle)
    {
        bool  isPaddle = address >= 0xC064 && address <= 0xC067;



        if (isPaddle)
        {
            SetCycle (machine, cycle - s_kObservedPaddleSpan);
            machine.GetMemoryBus().ReadByte (s_kObservedPaddleTrigger);
        }

        SetCycle (machine, cycle);

        return machine.GetMemoryBus().ReadByte (address);
    }


    static void SetCycle (MachineHost & machine, uint64_t cycle)
    {
        *machine.GetCpu()->GetCycleCounterPtr() = cycle;
        *machine.GetCpu()->GetBusCyclePtr()     = cycle;
    }


    //  One instruction's worth of cycles, as the CPU's rollup hands them to
    //  the mouse after it has counted them, with the retarget pass due.
    static void TickMouse (MachineHost & machine)
    {
        uint64_t  start = *machine.GetCpu()->GetCycleCounterPtr();



        SetCycle (machine, start + AppleMouse::kSampleQuantum);
        machine.GetMouse()->Tick (AppleMouse::kSampleQuantum);
    }


    static std::vector<Byte> SaveMouse (MachineHost & machine)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = machine.GetMouse()->SaveState (writer);
        Assert::IsTrue (hr == S_OK, L"the mouse saves its state");

        return writer.GetBytes();
    }
};
