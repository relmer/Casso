#include "Pch.h"

#include "TestMachine.h"
#include "HResultAssert.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kFrameCycles  = 17030;
static constexpr uint64_t  s_kWarmupFrames = 30;
static constexpr uint64_t  s_kRunFrames    = 20;
static constexpr int       s_kDiskSlot     = 6;
static constexpr int       s_kDiskDrive    = 0;
static constexpr size_t    s_kPatternStep  = 7;
static constexpr Word      s_kSlotRomByte  = 0xC600;

static constexpr const char * s_kMachineIds[] =
{
    "Apple2",
    "Apple2Plus",
    "Apple2e",
    "Apple2eEnhanced",
    "Apple2c",
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostStateTests
//
//  The whole-machine state: a state taken from a running machine, loaded back
//  and run forward, must reproduce the run it was taken from to the byte, and
//  a state from another machine or ROM set must not load.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineHostStateTests)
{
public:

    TEST_METHOD (RunningFromALoadedStateRepeatsTheRunOnEveryMachine)
    {
        for (const char * id : s_kMachineIds)
        {
            TestMachine  machine (id);

            // Powered on as Casso powers on a machine it has just built.
            machine.PowerCycle();
            machine.RunCycles (s_kWarmupFrames * s_kFrameCycles);

            CheckRunRepeats (machine, ToWide (id));
        }
    }


    TEST_METHOD (RunningFromALoadedStateRepeatsTheRunWithADiskSpinning)
    {
        TestMachine  machine ("Apple2e");
        DiskImage  * image = nullptr;



        MountPatternDisk (machine);

        image = machine.GetDiskStore().GetImage (s_kDiskSlot, s_kDiskDrive);
        Assert::IsNotNull (image);

        // Partway into the boot ROM's read of track 0, with the motor on.
        machine.RunCycles (s_kWarmupFrames * s_kFrameCycles);

        CheckRunRepeats (machine, L"Apple2e with a disk");
    }


    TEST_METHOD (LoadingAStateRestoresItExactly)
    {
        TestMachine        machine ("Apple2e");
        std::vector<Byte>  saved;
        std::vector<Byte>  resaved;
        HRESULT            hr = S_OK;



        MountPatternDisk (machine);
        machine.RunCycles (s_kWarmupFrames * s_kFrameCycles);

        saved = Save (machine);

        machine.RunCycles (s_kRunFrames * s_kFrameCycles);

        hr = Load (machine, saved);
        AssertSucceeded (hr, L"loading a state into the machine that saved it");

        resaved = Save (machine);

        Assert::IsTrue (saved == resaved, L"a state saved right after a load must equal the state loaded");
    }


    TEST_METHOD (LoadingAStateRestoresTheDiskBits)
    {
        constexpr int      kTrack = 3;
        constexpr size_t   kBit   = 1000;
        TestMachine        machine ("Apple2e");
        DiskImage        * image  = nullptr;
        std::vector<Byte>  saved;
        uint8_t            before = 0;
        HRESULT            hr     = S_OK;



        MountPatternDisk (machine);

        image = machine.GetDiskStore().GetImage (s_kDiskSlot, s_kDiskDrive);
        Assert::IsNotNull (image);

        saved  = Save (machine);
        before = image->ReadBit (kTrack, kBit);

        // What a guest write does to the media after the state was taken.
        image->WriteBit (kTrack, kBit, static_cast<uint8_t> (before ^ 1));

        hr = Load (machine, saved);
        AssertSucceeded (hr, L"load");

        Assert::AreEqual<uint8_t> (before, image->ReadBit (kTrack, kBit), L"the load must put back the bit the write changed");
        Assert::IsTrue  (image->IsTrackDirty (kTrack), L"the file may hold the written bit, so the next flush writes the track");
    }


    TEST_METHOD (AStateFromAnotherMachineKindDoesNotLoad)
    {
        TestMachine        source ("Apple2e");
        TestMachine        other  ("Apple2c");
        TestMachine        same   ("Apple2eEnhanced");
        std::vector<Byte>  saved;
        Word               pc     = 0;
        uint64_t           cycles = 0;
        HRESULT            hr     = S_OK;



        source.RunCycles (s_kWarmupFrames * s_kFrameCycles);
        other.RunCycles  (s_kWarmupFrames * s_kFrameCycles);

        saved  = Save (source);
        pc     = other.GetCpu()->GetPC();
        cycles = other.GetCpu()->GetTotalCycles();

        hr = Load (other, saved);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"a //e state must not load into a //c");

        Assert::AreEqual<Word>     (pc,     other.GetCpu()->GetPC(),          L"the rejected load must leave the CPU alone");
        Assert::AreEqual<uint64_t> (cycles, other.GetCpu()->GetTotalCycles(), L"and its cycle count");

        hr = Load (same, saved);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"a //e state must not load into an enhanced //e");
    }


    TEST_METHOD (AStateFromAnotherRomSetDoesNotLoad)
    {
        TestMachine        source  ("Apple2e");
        TestMachine        target  ("Apple2e");
        TestMachine        slotRom ("Apple2e");
        std::vector<Byte>  saved;
        RomDevice        * rom     = nullptr;
        CxxxRomRouter    * router  = nullptr;
        Word               last    = 0;
        bool               patched = false;
        HRESULT            hr      = S_OK;



        saved = Save (source);

        hr = Load (target, saved);
        AssertSucceeded (hr, L"a state loads into another machine of the same kind and ROM set");

        for (std::unique_ptr<MemoryDevice> & owned : target.GetOwnedDevices())
        {
            rom = dynamic_cast<RomDevice *> (owned.get());

            if (rom != nullptr && !patched)
            {
                last    = rom->GetEnd();
                patched = rom->TryPatch (last, static_cast<Byte> (~rom->GetData()[last - rom->GetStart()]));
            }
        }

        Assert::IsTrue (patched, L"the //e must have a ROM device to patch");

        hr = Load (target, saved);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"a state must not load against a different ROM image");

        // The Disk II boot ROM, which only the $Cxxx router holds.
        router = slotRom.GetMmu()->GetCxxxRouter();
        Assert::IsNotNull (router);

        patched = router->TryPatch (s_kSlotRomByte, static_cast<Byte> (~router->GetSlotRom (s_kDiskSlot)[0]));
        Assert::IsTrue (patched, L"the slot 6 ROM must take a patch");

        hr = Load (slotRom, saved);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"a state must not load against a different slot ROM");
    }


    TEST_METHOD (TheRomIdentityIsHashedOnceUntilARomIsPatched)
    {
        TestMachine      machine ("Apple2e");
        RomDevice      * rom     = nullptr;
        CxxxRomRouter  * router  = nullptr;
        Word             last    = 0;
        bool             patched = false;
        uint64_t         first   = 0;
        uint64_t         second  = 0;
        uint64_t         hashes  = 0;



        first  = machine.GetRomIdentity();
        hashes = machine.GetRomIdentityHashCount();

        Assert::AreEqual<uint64_t> (first,  machine.GetRomIdentity(),          L"an unchanged ROM set keeps its identity");
        Assert::AreEqual<uint64_t> (hashes, machine.GetRomIdentityHashCount(), L"and a second call does not hash the ROMs again");

        for (std::unique_ptr<MemoryDevice> & owned : machine.GetOwnedDevices())
        {
            rom = dynamic_cast<RomDevice *> (owned.get());

            if (rom != nullptr && !patched)
            {
                last    = rom->GetEnd();
                patched = rom->TryPatch (last, static_cast<Byte> (~rom->GetData()[last - rom->GetStart()]));
            }
        }

        Assert::IsTrue (patched, L"the //e must have a ROM device to patch");

        second = machine.GetRomIdentity();
        Assert::AreNotEqual<uint64_t> (first, second, L"a patched ROM device changes the identity");

        router = machine.GetMmu()->GetCxxxRouter();
        Assert::IsNotNull (router);

        patched = router->TryPatch (s_kSlotRomByte, static_cast<Byte> (~router->GetSlotRom (s_kDiskSlot)[0]));
        Assert::IsTrue (patched, L"the slot 6 ROM must take a patch");

        Assert::AreNotEqual<uint64_t> (second, machine.GetRomIdentity(), L"and so does a patched slot ROM");
    }


    TEST_METHOD (AStateWithADiskDoesNotLoadWithoutIt)
    {
        TestMachine        source ("Apple2e");
        TestMachine        target ("Apple2e");
        std::vector<Byte>  saved;
        HRESULT            hr = S_OK;



        MountPatternDisk (source);

        saved = Save (source);

        hr = Load (target, saved);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"the mounted drive bays must match");
    }


private:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  CheckRunRepeats
    //
    //  Save, run, save; load the first state, run the same distance, save.
    //  The two later states must match byte for byte: CPU, every RAM bank,
    //  every device and every disk track.
    //
    ////////////////////////////////////////////////////////////////////////////

    static void CheckRunRepeats (TestMachine & machine, const std::wstring & label)
    {
        std::vector<Byte>  start;
        std::vector<Byte>  firstRun;
        std::vector<Byte>  secondRun;
        Word               firstPc     = 0;
        uint64_t           firstCycles = 0;
        HRESULT            hr          = S_OK;



        start = Save (machine);

        machine.RunCycles (s_kRunFrames * s_kFrameCycles);

        firstRun    = Save (machine);
        firstPc     = machine.GetCpu()->GetPC();
        firstCycles = machine.GetCpu()->GetTotalCycles();

        Assert::IsFalse (start == firstRun, (label + L": the run must change the machine").c_str());

        hr = Load (machine, start);
        AssertSucceeded (hr, (label + L": load").c_str());

        machine.RunCycles (s_kRunFrames * s_kFrameCycles);

        secondRun = Save (machine);

        Assert::AreEqual<Word>     (firstPc,     machine.GetCpu()->GetPC(),          (label + L": PC").c_str());
        Assert::AreEqual<uint64_t> (firstCycles, machine.GetCpu()->GetTotalCycles(), (label + L": cycle count").c_str());
        Assert::IsTrue (firstRun == secondRun, (label + L": the replayed run must leave the same state").c_str());
    }


    static std::vector<Byte> Save (const MachineHost & machine)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = machine.SaveState (writer);
        AssertSucceeded (hr, L"SaveState");

        return writer.GetBytes();
    }


    static HRESULT Load (MachineHost & machine, const std::vector<Byte> & bytes)
    {
        StateReader  reader (bytes);



        return machine.LoadState (reader);
    }


    //  A DOS-order image of a byte pattern in slot 6, drive 1, wired to the
    //  controller the way the drive widget wires it.
    static void MountPatternDisk (TestMachine & machine)
    {
        std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);
        HRESULT            hr = S_OK;
        size_t             i  = 0;



        // Power on first, as Casso does, then mount.
        machine.PowerCycle();

        for (i = 0; i < raw.size(); i++)
        {
            raw[i] = static_cast<Byte> (i * s_kPatternStep);
        }

        hr = machine.GetDiskStore().MountFromBytes (s_kDiskSlot, s_kDiskDrive, "pattern.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes");

        machine.GetRefs().diskController->SetExternalDisk (s_kDiskDrive, machine.GetDiskStore().GetImage (s_kDiskSlot, s_kDiskDrive));
    }


    static std::wstring ToWide (const char * text)
    {
        std::string  narrow (text);



        return std::wstring (narrow.begin(), narrow.end());
    }
};
