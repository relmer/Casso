#include "Pch.h"

#include "EhmTestHelper.h"
#include "HResultAssert.h"
#include "EmuTests/ReverseSessionRig.h"
#include "EmuTests/TestMachine.h"
#include "Core/RamPages.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/DebugMemoryView.h"
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
