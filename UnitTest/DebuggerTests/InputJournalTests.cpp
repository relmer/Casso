#include "Pch.h"

#include "Core/Prng.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/MachineHost.h"
#include "resource.h"
#include "Shell/CpuManager.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


//
//  A machine of RAM and one ROM page of NOPs, so RunCycles has something to
//  run and the cycle counter something to count.
//
static constexpr Word      s_kJournalRamEnd      = 0xBFFF;
static constexpr Word      s_kJournalRomStart    = 0xFF00;
static constexpr Word      s_kJournalRomEnd      = 0xFFFF;
static constexpr Word      s_kJournalResetVector = 0xFFFC;
static constexpr Byte      s_kJournalNop         = 0xEA;
static constexpr uint64_t  s_kJournalSeed        = 0x1A9E7450ULL;
static constexpr uint64_t  s_kJournalRunCycles   = 100;





////////////////////////////////////////////////////////////////////////////////
//
//  InputJournalTests
//
//  The journal of host inputs a replay applies in place of the host: what it
//  keeps, how its indices behave as it is trimmed at either end, which queued
//  commands are inputs, and that the keyboard's host-time auto-repeat reports
//  each repeat it fires so the shell can journal it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InputJournalTests)
{
public:

    TEST_METHOD (AJournalThatIsOffRecordsNothing)
    {
        InputJournal  journal;

        journal.Record (10, InputKind::KeyPress, 'A', 0, {});

        Assert::AreEqual<size_t> (0, journal.GetEndIndex(),
            L"a journal that is off must ignore every record");
    }


    TEST_METHOD (ARecordKeepsItsCyclePositionKindAndArguments)
    {
        InputJournal  journal;
        uint64_t      position = 42;

        journal.SetOn (true);
        journal.SetPositionSource (&position);
        journal.Record (1234, InputKind::DiskMount, 1, 7, "C:\\disks\\dos33.dsk");

        const InputRecord  & record = journal.GetRecord (0);

        Assert::AreEqual<uint64_t> (42,   record.position);
        Assert::AreEqual<uint64_t> (1234, record.cycle);
        Assert::IsTrue (record.kind == InputKind::DiskMount);
        Assert::AreEqual<Byte>     (1,    record.value);
        Assert::AreEqual<uint16_t> (7,    record.detail);
        Assert::AreEqual<std::string> ("C:\\disks\\dos33.dsk", record.payload);
    }


    TEST_METHOD (DiscardingOldRecordsKeepsTheIndicesOfTheRest)
    {
        InputJournal  journal;

        journal.SetOn (true);

        for (Byte key = 'A'; key <= 'E'; key++)
        {
            journal.Record (key, InputKind::KeyPress, key, 0, {});
        }

        journal.DiscardBefore (2);

        Assert::AreEqual<size_t> (2, journal.GetBeginIndex());
        Assert::AreEqual<size_t> (5, journal.GetEndIndex());
        Assert::AreEqual<Byte> ('C', journal.GetRecord (2).value,
            L"index 2 must still be the third record made");
    }


    TEST_METHOD (TruncatingDropsTheRecordedFuture)
    {
        InputJournal  journal;

        journal.SetOn (true);

        for (Byte key = 'A'; key <= 'E'; key++)
        {
            journal.Record (key, InputKind::KeyPress, key, 0, {});
        }

        journal.Truncate (3);

        Assert::AreEqual<size_t> (3, journal.GetEndIndex());
        Assert::AreEqual<Byte> ('C', journal.GetRecord (2).value);
    }


    TEST_METHOD (IndicesKeepCountingAfterAClear)
    {
        InputJournal  journal;

        journal.SetOn (true);
        journal.Record (1, InputKind::KeyPress, 'A', 0, {});
        journal.Record (2, InputKind::KeyPress, 'B', 0, {});
        journal.Clear();
        journal.Record (3, InputKind::KeyPress, 'C', 0, {});

        Assert::AreEqual<size_t> (2, journal.GetBeginIndex(),
            L"a cursor held from before the clear must not reach the new record");
        Assert::AreEqual<Byte> ('C', journal.GetRecord (2).value);
    }


    TEST_METHOD (TheHostStampsAnInputWithTheCpuCycle)
    {
        MachineHost  host;

        Build (host);
        host.GetInputJournal().SetOn (true);

        host.RunCycles (s_kJournalRunCycles);
        host.RecordInput (InputKind::PowerCycle, 0, 0, {});

        Assert::AreEqual<size_t> (1, host.GetInputJournal().GetEndIndex());
        Assert::AreEqual<uint64_t> (host.GetCpu()->GetTotalCycles(),
                                    host.GetInputJournal().GetRecord (0).cycle,
            L"the stamp must be the cycle the CPU had reached");
    }


    TEST_METHOD (AResetIsAnInputCarryingTheAppleKeysItHolds)
    {
        EmulatorCommand  cmd   = { IDM_MACHINE_RESET, "open,closed" };
        InputRecord      input;

        Assert::IsTrue (CpuCommandDispatcher::TryGetJournalInput (cmd, input));
        Assert::IsTrue (input.kind == InputKind::Reset);
        Assert::AreEqual<uint16_t> (CpuCommandDispatcher::kResetHoldsOpenApple | CpuCommandDispatcher::kResetHoldsClosedApple,
                                    input.detail);
    }


    TEST_METHOD (DiskChangesAreInputsWithTheirDriveAndPath)
    {
        EmulatorCommand  mount   = { IDM_DISK_INSERT2, "C:\\disks\\game.woz" };
        EmulatorCommand  eject   = { IDM_DISK_EJECT1, "" };
        EmulatorCommand  protect = { IDM_DISK_WRITEPROTECT2, "1" };
        InputRecord      input;

        Assert::IsTrue (CpuCommandDispatcher::TryGetJournalInput (mount, input));
        Assert::IsTrue (input.kind == InputKind::DiskMount);
        Assert::AreEqual<Byte> (1, input.value);
        Assert::AreEqual<std::string> ("C:\\disks\\game.woz", input.payload);

        Assert::IsTrue (CpuCommandDispatcher::TryGetJournalInput (eject, input));
        Assert::IsTrue (input.kind == InputKind::DiskEject);
        Assert::AreEqual<Byte> (0, input.value);

        Assert::IsTrue (CpuCommandDispatcher::TryGetJournalInput (protect, input));
        Assert::IsTrue (input.kind == InputKind::DriveWriteProtect);
        Assert::AreEqual<Byte>     (1, input.value);
        Assert::AreEqual<uint16_t> (1, input.detail);
    }


    TEST_METHOD (APowerCycleIsAnInputAndAStepIsNot)
    {
        EmulatorCommand  power = { IDM_MACHINE_POWERCYCLE, "" };
        EmulatorCommand  step  = { IDM_MACHINE_STEP, "" };
        InputRecord      input;

        Assert::IsTrue  (CpuCommandDispatcher::TryGetJournalInput (power, input));
        Assert::IsTrue  (input.kind == InputKind::PowerCycle);
        Assert::IsFalse (CpuCommandDispatcher::TryGetJournalInput (step, input),
            L"a debugger step changes nothing a replay must repeat");
    }


    TEST_METHOD (AutoRepeatReportsEachRepeatItFires)
    {
        AppleKeyboard  kbd;
        Byte           fired = 0;

        kbd.PressKey ('Q');
        kbd.SetKeyDown (true);
        kbd.BeginKeyRepeat ('Q');

        fired = kbd.TickAutoRepeat (1);
        Assert::AreEqual<Byte> (0, fired, L"arming the key fires nothing");

        fired = kbd.TickAutoRepeat (AppleKeyboard::kKeyRepeatDelayUs - 1);
        Assert::AreEqual<Byte> (0, fired, L"nothing fires before the delay");

        fired = kbd.TickAutoRepeat (1);
        Assert::AreEqual<Byte> ('Q', fired, L"the repeat at the delay must be reported");

        fired = kbd.TickAutoRepeat (1);
        Assert::AreEqual<Byte> (0, fired, L"and nothing until the next interval");
    }

private:

    static void Build (MachineHost & host)
    {
        std::vector<Byte>  rom (s_kJournalRomEnd - s_kJournalRomStart + 1, s_kJournalNop);

        rom[s_kJournalResetVector - s_kJournalRomStart]     = static_cast<Byte> (s_kJournalRomStart & 0xFF);
        rom[s_kJournalResetVector + 1 - s_kJournalRomStart] = static_cast<Byte> (s_kJournalRomStart >> 8);

        host.SetPrng (std::make_unique<Prng> (s_kJournalSeed));

        auto  ram    = std::make_unique<RamDevice> (0x0000, s_kJournalRamEnd);
        auto  romDev = std::make_unique<RomDevice> (s_kJournalRomStart, s_kJournalRomEnd, std::move (rom));

        host.GetMemoryBus().AddDevice (ram.get());
        host.GetMemoryBus().AddDevice (romDev.get());

        host.GetOwnedDevices().push_back (std::move (ram));
        host.GetOwnedDevices().push_back (std::move (romDev));

        host.SetCpu (std::make_unique<EmuCpu> (host.GetMemoryBus()));
        host.GetCpu()->InitForEmulation (*host.GetPrng());
    }
};
