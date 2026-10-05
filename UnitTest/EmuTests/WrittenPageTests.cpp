#include "Pch.h"

#include "EhmTestHelper.h"
#include "HResultAssert.h"
#include "EmuTests/ReverseSessionRig.h"
#include "EmuTests/TestMachine.h"
#include "Core/RamPages.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/DebugMemoryView.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/RamDevice.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace UnitTestHelpers;


static constexpr Word  s_kPageMain      = 0x0300;
static constexpr Word  s_kPageText      = 0x0400;
static constexpr Word  s_kPageHires     = 0x2000;
static constexpr Word  s_kPageHigh      = 0x6000;
static constexpr Word  s_kLcBankAddress = 0xD000;
static constexpr Word  s_kLcHighAddress = 0xE000;
static constexpr Word  s_kRamWrtOff     = 0xC004;
static constexpr Word  s_kRamWrtOn      = 0xC005;
static constexpr Word  s_k80StoreOn     = 0xC001;
static constexpr Word  s_kAltZpOn       = 0xC009;
static constexpr Word  s_kPage2On       = 0xC055;
static constexpr Word  s_kHiresOn       = 0xC057;
static constexpr Word  s_kLcBank1RamRw  = 0xC08B;
static constexpr Word  s_kLcBank2RamRw  = 0xC083;
static constexpr Byte  s_kWritten       = 0xA5;
static constexpr Byte  s_kHostKey       = 'A';
static constexpr Byte  s_kHostPaddle    = 0x11;

// Each //e banking switch as its off and on address: RAMRD, RAMWRT, ALTZP,
// 80STORE, PAGE2 and HIRES.
static constexpr Word  s_kBankingSwitches[][2] =
{
    { 0xC002, 0xC003 },
    { 0xC004, 0xC005 },
    { 0xC008, 0xC009 },
    { 0xC000, 0xC001 },
    { 0xC054, 0xC055 },
    { 0xC056, 0xC057 },
};





////////////////////////////////////////////////////////////////////////////////
//
//  WrittenPageTests
//
//  A checkpoint keeps by reference each RAM chunk nobody wrote since the one
//  before it, so every way RAM changes must mark the page it changed. Each
//  test takes a sharing save, changes RAM one way, and takes another: the
//  second, flattened, must be the machine as a full save shows it, and the
//  change must be in it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WrittenPageTests)
{
public:

    TEST_METHOD (ABusWriteToMainRamIsCaptured)
    {
        ExpectCaptured (L"main RAM", [] (TestMachine & machine) { machine.GetMemoryBus().WriteByte (s_kPageMain, s_kWritten); });
    }


    TEST_METHOD (AnInstructionsStoreIsCaptured)
    {
        TestMachine  machine ("Apple2e");



        machine.PowerCycle();

        // STA $0310 at $0800, with A loaded.
        machine.GetMemoryBus().WriteByte (0x0800, 0x8D);
        machine.GetMemoryBus().WriteByte (0x0801, 0x10);
        machine.GetMemoryBus().WriteByte (0x0802, 0x03);
        machine.GetCpu()->SetPC (0x0800);

        ExpectCapturedOn (machine, L"an instruction's store", [] (TestMachine & m) { m.StepOne(); });
    }


    TEST_METHOD (RamWrtRedirectionToAuxIsCaptured)
    {
        ExpectCaptured (L"RAMWRT to aux", [] (TestMachine & machine)
        {
            machine.GetMemoryBus().WriteByte (s_kRamWrtOn, 0);
            machine.GetMemoryBus().WriteByte (s_kPageHigh, s_kWritten);
            machine.GetMemoryBus().WriteByte (s_kRamWrtOff, 0);
        });
    }


    TEST_METHOD (EightyStoreTextAndHiresToAuxAreCaptured)
    {
        ExpectCaptured (L"80STORE text and hires", [] (TestMachine & machine)
        {
            machine.GetMemoryBus().WriteByte (s_k80StoreOn, 0);
            machine.GetMemoryBus().WriteByte (s_kPage2On, 0);
            machine.GetMemoryBus().WriteByte (s_kHiresOn, 0);
            machine.GetMemoryBus().WriteByte (s_kPageText, s_kWritten);
            machine.GetMemoryBus().WriteByte (s_kPageHires, s_kWritten);
        });
    }


    TEST_METHOD (AltZpToAuxIsCaptured)
    {
        ExpectCaptured (L"ALTZP", [] (TestMachine & machine)
        {
            machine.GetMemoryBus().WriteByte (s_kAltZpOn, 0);
            machine.GetMemoryBus().WriteByte (0x0042, s_kWritten);
            machine.GetMemoryBus().WriteByte (0x01F0, s_kWritten);
        });
    }


    TEST_METHOD (LanguageCardBankWritesAreCaptured)
    {
        ExpectCaptured (L"language card, main and aux", [] (TestMachine & machine)
        {
            MemoryBus  & bus = machine.GetMemoryBus();



            bus.ReadByte  (s_kLcBank1RamRw);
            bus.ReadByte  (s_kLcBank1RamRw);
            bus.WriteByte (s_kLcBankAddress, s_kWritten);
            bus.WriteByte (s_kLcHighAddress, s_kWritten);

            bus.ReadByte  (s_kLcBank2RamRw);
            bus.ReadByte  (s_kLcBank2RamRw);
            bus.WriteByte (s_kLcBankAddress + 1, s_kWritten);

            bus.WriteByte (s_kAltZpOn, 0);
            bus.WriteByte (s_kLcBankAddress + 2, s_kWritten);
            bus.WriteByte (s_kLcHighAddress + 2, s_kWritten);
        });
    }


    TEST_METHOD (AWriteToAWatchedPageIsCaptured)
    {
        ExpectCaptured (L"a watched page", [] (TestMachine & machine)
        {
            machine.GetMemoryBus().SetWatchedPage (s_kPageMain >> 8, true);
            machine.GetMemoryBus().WriteByte (s_kPageMain, s_kWritten);
            machine.GetMemoryBus().SetWatchedPage (s_kPageMain >> 8, false);
        });
    }


    TEST_METHOD (ADeviceWriteToRamIsCaptured)
    {
        ExpectCaptured (L"RamDevice::Write", [] (TestMachine & machine) { machine.GetRefs().mainRamDev->Write (s_kPageMain, s_kWritten); });
    }


    TEST_METHOD (DebuggerEditsAreCaptured)
    {
        ExpectCaptured (L"debugger poke and patch", [] (TestMachine & machine)
        {
            DebugMemoryView  view (machine);
            MemoryBus      & bus = machine.GetMemoryBus();



            Assert::IsTrue (view.TryPoke  (s_kPageMain, s_kWritten), L"poke");
            Assert::IsTrue (view.TryPatch (s_kPageMain + 1, s_kWritten), L"patch");

            bus.ReadByte (s_kLcBank1RamRw);
            bus.ReadByte (s_kLcBank1RamRw);

            Assert::IsTrue (view.TryPoke (s_kLcBankAddress, s_kWritten), L"poke into the language card");
        });
    }


    TEST_METHOD (APokeIntoTheCpuMemoryIsCaptured)
    {
        ExpectCaptured (L"EmuCpu::PokeByte", [] (TestMachine & machine) { machine.GetCpu()->PokeByte (s_kPageMain, s_kWritten); });
    }


    TEST_METHOD (ALoadedStateIsCaptured)
    {
        TestMachine        machine ("Apple2e");
        std::vector<Byte>  earlier;



        ReverseSessionRig::Prepare (machine);
        machine.RunCycles (KeyframeSettings::kFrameCycles);

        earlier = ReverseSessionRig::Save (machine);

        machine.RunCycles (KeyframeSettings::kFrameCycles);

        ExpectCapturedOn (machine, L"LoadState", [&earlier] (TestMachine & m)
        {
            StateReader  reader (earlier.data(), earlier.size());
            HRESULT      hr     = m.LoadState (reader);



            AssertSucceeded (hr, L"LoadState");
        });
    }


    TEST_METHOD (OnlyTheWrittenChunkIsCopied)
    {
        TestMachine                machine ("Apple2e");
        std::vector<StateSegment>  before;
        std::vector<StateSegment>  after;
        size_t                     i       = 0;
        size_t                     changed = 0;



        machine.PowerCycle();

        before = SaveShared (machine);

        machine.GetMemoryBus().WriteByte (s_kPageMain, static_cast<Byte> (~machine.GetMemoryBus().ReadByte (s_kPageMain)));

        after = SaveShared (machine);

        Assert::AreEqual (before.size(), after.size());

        for (i = 0; i < after.size(); i++)
        {
            changed += (before[i].bytes == after[i].bytes) ? 0 : 1;
        }

        Assert::AreEqual<size_t> (1, changed, L"one written byte gives one new chunk; every other is shared");
    }


    TEST_METHOD (AnUnmarkedWriteFailsTheDebugCheck)
    {
        TestMachine  machine ("Apple2e");
        StateWriter  writer;
        HRESULT      hr      = S_OK;



        machine.PowerCycle();

        SaveShared (machine);

        // Straight into the buffer, past every mark.
        machine.GetRefs().mainRamDev->GetData()[s_kPageMain] ^= 0xFF;

        writer.SetSharing (true);

        hr = machine.SaveState (writer);
        AssertSucceeded (hr, L"SaveState");

        {
            ExpectedEhmAssert  expect;

            machine.CheckSharedSave (writer);

            expect.RequireCount (1);
        }
    }


    //  Every RAM page the bus maps, $0000-$BFFF and the language card's
    //  $D000-$FFFF, written under every combination of the //e's banking
    //  switches and the language card's two banks, on each path a bus write
    //  can take: the page table, the watched path and the trace. Each pass
    //  ends with a sharing save that must be the full save.
    TEST_METHOD (EveryRamPageIsCapturedUnderEveryBankingAndBusPath)
    {
        static constexpr int  kBusPaths    = 3;
        static constexpr int  kSwitchCount = static_cast<int> (sizeof (s_kBankingSwitches) / sizeof (s_kBankingSwitches[0]));
        static constexpr int  kBankings    = 1 << (kSwitchCount + 1);
        static constexpr int  kPageCount   = 0x100;
        static constexpr int  kFirstIoPage = 0xC0;
        static constexpr int  kLastIoPage  = 0xCF;
        static constexpr int  kPageShift   = 8;
        static constexpr int  kOffsetMask  = 0xFF;
        static constexpr int  kValueStep   = 31;

        TestMachine            machine ("Apple2e");
        MemoryBus            & bus     = machine.GetMemoryBus();
        int                    path    = 0;
        int                    banking = 0;
        int                    page    = 0;
        int                    pass    = 0;
        std::wostringstream    what;



        machine.PowerCycle();

        SaveShared (machine);

        for (path = 0; path < kBusPaths; path++)
        {
            SetBusPath (bus, path);

            for (banking = 0; banking < kBankings; banking++)
            {
                SetBanking (bus, banking, kSwitchCount);

                for (page = 0; page < kPageCount; page++)
                {
                    if (page >= kFirstIoPage && page <= kLastIoPage)
                    {
                        continue;
                    }

                    bus.WriteByte (static_cast<Word> ((page << kPageShift) | (pass & kOffsetMask)), static_cast<Byte> (pass * kValueStep + page + 1));
                }

                what.str (L"");
                what << L"bus path " << path << L", banking " << banking;

                ExpectSharedSaveIsFull (machine, what.str().c_str());

                pass++;
            }
        }

        SetBusPath (bus, 0);
    }


    TEST_METHOD (HostInputBetweenASaveAndItsCheckIsNotAMissedWrite)
    {
        TestMachine        machine ("Apple2e");
        StateWriter        writer;
        std::vector<Byte>  before;
        HRESULT            hr      = S_OK;



        machine.PowerCycle();

        SaveShared (machine);

        writer.SetSharing (true);

        hr = machine.SaveState (writer);
        AssertSucceeded (hr, L"SaveState");

        before = ReverseSessionRig::Save (machine);

        // What the UI thread does while the emulation thread is between the
        // keyframe save and its check: a key, a joystick move, a button.
        machine.GetRefs().keyboard->PressKey (s_kHostKey);
        machine.GetRefs().iieSoftSwitches->SetPaddle (0, s_kHostPaddle);
        machine.GetRefs().iieKeyboard->SetOpenApple (true);

        Assert::IsTrue (ReverseSessionRig::Save (machine) != before, L"the host input is in the save");

        machine.CheckSharedSave (writer);
    }


    TEST_METHOD (AnUnmarkedTrackChangeFailsTheDebugCheck)
    {
        TestMachine    machine ("Apple2e");
        StateWriter    writer;
        DiskImage    * disk    = nullptr;
        HRESULT        hr      = S_OK;



        ReverseSessionRig::Prepare (machine);

        disk = machine.GetDiskStore().GetImage (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive);
        Assert::IsNotNull (disk, L"the rig's disk");

        SaveShared (machine);

        // Straight into the track, past its generation.
        const_cast<std::vector<Byte> &> (disk->GetTrackBits (0))[0] ^= 0xFF;

        writer.SetSharing (true);

        hr = machine.SaveState (writer);
        AssertSucceeded (hr, L"SaveState");

        {
            ExpectedEhmAssert  expect;

            machine.CheckSharedSave (writer);

            expect.RequireCount (1);
        }
    }

private:

    using Change = std::function<void (TestMachine &)>;


    static void ExpectCaptured (const wchar_t * what, const Change & change)
    {
        TestMachine  machine ("Apple2e");



        machine.PowerCycle();

        ExpectCapturedOn (machine, what, change);
    }


    //  A sharing save before the change and one after it: the second,
    //  flattened, is the full save, and differs from the first.
    static void ExpectCapturedOn (TestMachine & machine, const wchar_t * what, const Change & change)
    {
        StateWriter        first;
        StateWriter        second;
        std::vector<Byte>  firstFlat;
        std::vector<Byte>  secondFlat;
        std::vector<Byte>  full;
        HRESULT            hr          = S_OK;



        first.SetSharing (true);

        hr = machine.SaveState (first);
        AssertSucceeded (hr, L"SaveState before");

        first.FlattenInto (firstFlat);

        change (machine);

        second.SetSharing (true);

        hr = machine.SaveState (second);
        AssertSucceeded (hr, L"SaveState after");

        second.FlattenInto (secondFlat);
        full = ReverseSessionRig::Save (machine);

        Assert::IsTrue (full != firstFlat, what);
        Assert::IsTrue (secondFlat == full, what);
    }


    //  A sharing save, flattened, is the full save, and every run it kept
    //  still holds its buffer's bytes.
    static void ExpectSharedSaveIsFull (TestMachine & machine, const wchar_t * what)
    {
        StateWriter        writer;
        std::vector<Byte>  flat;
        HRESULT            hr     = S_OK;



        writer.SetSharing (true);

        hr = machine.SaveState (writer);
        AssertSucceeded (hr, L"SaveState");

        writer.FlattenInto (flat);

        Assert::IsTrue (flat == ReverseSessionRig::Save (machine), what);

        machine.CheckSharedSave (writer);
    }


    //  0: the page table; 1: every page watched; 2: the trace.
    static void SetBusPath (MemoryBus & bus, int path)
    {
        static constexpr int  kWatched   = 1;
        static constexpr int  kTrace     = 2;
        static constexpr int  kPageCount = 0x100;
        int                   page       = 0;



        for (page = 0; page < kPageCount; page++)
        {
            bus.SetWatchedPage (page, path == kWatched);
        }

        bus.SetTraceAllPages (path == kTrace);
    }


    //  Bit n of banking turns banking switch n on; the bit past them picks
    //  language card bank 2 over bank 1, read and write enabled either way.
    static void SetBanking (MemoryBus & bus, int banking, int switchCount)
    {
        Word  lcSwitch = ((banking >> switchCount) & 1) != 0 ? s_kLcBank2RamRw : s_kLcBank1RamRw;
        int   i        = 0;



        for (i = 0; i < switchCount; i++)
        {
            bus.WriteByte (s_kBankingSwitches[i][(banking >> i) & 1], 0);
        }

        bus.ReadByte (lcSwitch);
        bus.ReadByte (lcSwitch);
    }


    static std::vector<StateSegment> SaveShared (MachineHost & machine)
    {
        StateWriter  writer;
        HRESULT      hr     = S_OK;



        writer.SetSharing (true);

        hr = machine.SaveState (writer);
        AssertSucceeded (hr, L"SaveState");

        return writer.TakeSegments();
    }
};
