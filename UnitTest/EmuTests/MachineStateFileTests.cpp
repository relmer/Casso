#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Common/Disk2AudioSource.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Shell/MachineStateFile.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kStateWarmupCycles = 200000;
static constexpr uint64_t  s_kStateRunCycles    = 150000;
static constexpr size_t    s_kStatePattern      = 29;
static constexpr size_t    s_kChangedPattern    = 31;
static constexpr Byte      s_kStateHighBit      = 0x80;
static constexpr Word      s_kRamEnd            = 0xC000;
static constexpr const char *  s_kImagePath     = "C:\\Disks\\state.nib";





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFileTests
//
//  The whole machine saved to a state file's bytes and loaded back: a
//  running //e with a disk resumes exactly where it was saved, in the same
//  session or a new one, with writes not yet saved to the image file; a
//  state another ROM set saved, and a damaged or cut-short file, are refused
//  without the machine changing; and neither the save nor the load reads or
//  writes the image file.
//
//  The guest is the reverse execution rig's loop, which writes to the disk
//  every sixteenth pass. The disk is a nibble image, which serializes every
//  bit the guest wrote; image file reads and writes go to counting stand-ins.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineStateFileTests)
{
public:

    struct FileLog
    {
        size_t  reads   = 0;
        size_t  flushes = 0;
        size_t  pattern = s_kStatePattern;
    };


    //  What a test compares between two machines: registers, cycles,
    //  the 48K of main RAM and the disk in slot 6 drive 1.
    struct Snapshot
    {
        Cpu6502Registers   registers = {};
        uint64_t           cycles    = 0;
        std::vector<Byte>  ram;
        std::vector<Byte>  disk;
    };


    TEST_METHOD (ARunningMachineWithADiskResumesWhereItWasSaved)
    {
        TestMachine           machine ("Apple2e");
        FileLog               log;
        std::vector<Byte>     bytes;
        Snapshot              expected;
        Snapshot              actual;



        Prepare (machine, log, true);

        machine.RunCycles (s_kStateWarmupCycles);

        bytes = Build (machine);

        machine.RunCycles (s_kStateRunCycles);
        expected = Take (machine);

        Load (machine, bytes);

        machine.RunCycles (s_kStateRunCycles);
        actual = Take (machine);

        AssertSame (expected, actual);
    }


    //  A new session: another machine, never run, with the image file
    //  mounted as it is on the host. The state's disk replaces it with the
    //  guest's writes, and the machine then runs as the saved one did.
    TEST_METHOD (UnsavedDiskWritesSurviveIntoANewSession)
    {
        TestMachine           source ("Apple2e");
        TestMachine           target ("Apple2e");
        FileLog               sourceLog;
        FileLog               targetLog;
        std::vector<Byte>     bytes;
        std::vector<Byte>     fileImage;
        Snapshot              expected;
        Snapshot              actual;



        Prepare (source, sourceLog, true);

        source.RunCycles (s_kStateWarmupCycles);

        Assert::IsTrue (source.GetDiskStore().GetImage (6, 0)->IsDirty(), L"the guest wrote to the disk");
        Assert::AreEqual<size_t> (0, sourceLog.flushes, L"and none of it reached the image file");

        bytes = Build (source);

        source.RunCycles (s_kStateRunCycles);
        expected = Take (source);

        Prepare (target, targetLog, false);

        Load (target, bytes);

        target.RunCycles (s_kStateRunCycles);
        actual = Take (target);

        AssertSucceeded (ReadPatternImage (s_kStatePattern, fileImage), L"ReadPatternImage");
        Assert::IsFalse (actual.disk == fileImage, L"the disk holds the guest's writes, not the file's contents");

        AssertSame (expected, actual);
    }


    //  Saving reads and writes no image file, however dirty the disk is, and
    //  loading over a clean disk does neither, even when the file on the host
    //  has changed since the save.
    TEST_METHOD (SaveAndLoadLeaveTheImageFileAlone)
    {
        TestMachine           source ("Apple2e");
        TestMachine           target ("Apple2e");
        FileLog               sourceLog;
        FileLog               targetLog;
        std::vector<Byte>     bytes;
        std::vector<Byte>     savedDisk;
        size_t                readsBefore = 0;



        Prepare (source, sourceLog, true);

        source.RunCycles (s_kStateWarmupCycles);

        readsBefore = sourceLog.reads;
        bytes       = Build (source);

        Assert::AreEqual<size_t> (readsBefore, sourceLog.reads,   L"the save read no image file");
        Assert::AreEqual<size_t> (0,           sourceLog.flushes, L"the save wrote no image file");
        Assert::IsTrue (source.GetDiskStore().GetImage (6, 0)->IsDirty(), L"and the disk's writes are still unsaved");

        AssertSucceeded (source.GetDiskStore().GetImage (6, 0)->Serialize (savedDisk), L"Serialize the saved disk");

        Prepare (target, targetLog, false);

        targetLog.pattern = s_kChangedPattern;
        readsBefore       = targetLog.reads;

        Load (target, bytes);

        Assert::AreEqual<size_t> (readsBefore, targetLog.reads,   L"the load read no image file");
        Assert::AreEqual<size_t> (0,           targetLog.flushes, L"the load wrote no image file");
        Assert::IsTrue (Take (target).disk == savedDisk, L"the disk is the one the state holds");

        Assert::IsFalse          (target.GetDiskStore().IsRetainingMedia(),         L"the store keeps no disks once the load is over");
        Assert::AreEqual<size_t> (0, target.GetDiskStore().GetRetainedMediaCount(), L"so the disk the state replaced is let go");
    }


    TEST_METHOD (AStateFromADifferentRomSetIsRefused)
    {
        TestMachine           source ("Apple2e");
        TestMachine           target ("Apple2e");
        FileLog               sourceLog;
        FileLog               targetLog;
        std::vector<Byte>     bytes;
        std::vector<Byte>     before;
        MachineStateContents  contents;
        MachineStateError     error;
        HRESULT               hr      = S_OK;



        Prepare (source, sourceLog, true);
        source.RunCycles (s_kStateWarmupCycles);

        bytes = Build (source);

        Prepare (target, targetLog, true);

        hr = MachineStateFile::Parse (bytes, contents, error);
        AssertSucceeded (hr, L"Parse");

        hr = MachineStateFile::Check (target, contents, error);
        AssertSucceeded (hr, L"before the patch, the same ROM images take the state");

        PatchRom (target);

        before = ReverseSessionRig::Save (target);

        hr = MachineStateFile::Apply (target, contents, error);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"a state from other ROM images must not load");
        Assert::AreEqual (std::string ("different ROM images"), error.label);
        Assert::IsTrue (before == ReverseSessionRig::Save (target), L"and the machine is unchanged");
        Assert::AreEqual<size_t> (0, targetLog.flushes, L"with no disk written");
    }


    TEST_METHOD (ADamagedOrCutShortFileIsRefusedWithoutChangingTheMachine)
    {
        constexpr size_t      kVersionOffset = 8;
        constexpr size_t      kTooShort      = 10;
        TestMachine           machine ("Apple2e");
        FileLog               log;
        std::vector<Byte>     bytes;
        std::vector<Byte>     damaged;
        std::vector<Byte>     before;
        MachineStateContents  contents;
        MachineStateError     error;
        HRESULT               hr      = S_OK;



        Prepare (machine, log, true);
        machine.RunCycles (s_kStateWarmupCycles);

        bytes = Build (machine);

        machine.RunCycles (s_kStateRunCycles);
        before = ReverseSessionRig::Save (machine);

        damaged = bytes;
        damaged.resize (bytes.size() / 2);
        hr = LoadIfValid (machine, damaged, error);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"a cut-short file must not load");
        Assert::AreEqual (std::string ("damaged state file"), error.label);

        damaged.resize (kTooShort);
        hr = LoadIfValid (machine, damaged, error);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"a file shorter than the header must not load");

        damaged = bytes;
        damaged[bytes.size() - bytes.size() / 3] ^= 1;
        hr = LoadIfValid (machine, damaged, error);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"a file with one bit changed must not load");
        Assert::AreEqual (std::string ("damaged state file"), error.label);

        damaged = bytes;
        damaged[0] = 'X';
        hr = LoadIfValid (machine, damaged, error);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"a file that is not a state must not load");
        Assert::AreEqual (std::string ("not a machine state"), error.label);

        damaged = bytes;
        damaged[kVersionOffset]++;
        hr = LoadIfValid (machine, damaged, error);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_REVISION_MISMATCH), hr, L"a newer format must not load");

        Assert::IsTrue (before == ReverseSessionRig::Save (machine), L"no refused file changed the machine");
        Assert::AreEqual<size_t> (0, log.flushes, L"or wrote a disk");

        hr = MachineStateFile::Parse (bytes, contents, error);
        AssertSucceeded (hr, L"the undamaged file still parses");
    }


    //  A state that fails to load after its disks went into the bays: the
    //  machine state is cut short inside its own section, so its header reads
    //  and the load fails in the parts after it. The bays are left as they
    //  were -- drive 1 keeps its disk rather than the state's, drive 2 keeps
    //  the disk the state had no place for -- the drives read them, and the
    //  machine is unchanged.
    TEST_METHOD (AStateThatFailsToLoadLeavesTheBaysAsTheyWere)
    {
        constexpr int           kOtherDrive = 1;
        TestMachine             source ("Apple2e");
        TestMachine             target ("Apple2e");
        FileLog                 sourceLog;
        FileLog                 targetLog;
        std::vector<Byte>       bytes;
        std::vector<Byte>       otherDisk;
        std::vector<Byte>       before;
        MachineStateContents    contents;
        MachineStateError       error;
        DiskImageStore        & store       = target.GetDiskStore();
        Disk2Controller       * controller  = target.GetRefs().diskController;
        uint64_t                diskId      = 0;
        uint64_t                otherId     = 0;
        HRESULT                 hr          = S_OK;



        Prepare (source, sourceLog, true);
        source.RunCycles (s_kStateWarmupCycles);

        bytes = Build (source);

        hr = MachineStateFile::Parse (bytes, contents, error);
        AssertSucceeded (hr, L"Parse");

        CutShort (contents.machineState);

        Prepare (target, targetLog, true);

        AssertSucceeded (ReadPatternImage (s_kChangedPattern, otherDisk), L"ReadPatternImage");

        hr = store.MountFromBytes (ReverseSessionRig::kDiskSlot, kOtherDrive, "C:\\Disks\\other.nib", DiskFormat::Nib, otherDisk);
        AssertSucceeded (hr, L"MountFromBytes");

        diskId  = store.GetMediaId (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive);
        otherId = store.GetMediaId (ReverseSessionRig::kDiskSlot, kOtherDrive);
        before  = ReverseSessionRig::Save (target);

        Assert::IsTrue (diskId != 0 && otherId != 0, L"both drives hold a disk before the load");

        hr = MachineStateFile::Apply (target, contents, error);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"a state cut short inside its section must not load");
        Assert::AreEqual (std::string ("state not loaded"), error.label);

        Assert::AreEqual<uint64_t> (diskId,  store.GetMediaId (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive), L"drive 1 holds the disk it held, not the state's");
        Assert::AreEqual<uint64_t> (otherId, store.GetMediaId (ReverseSessionRig::kDiskSlot, kOtherDrive),                   L"drive 2 still holds the disk the state had no place for");
        Assert::AreEqual<uint64_t> (diskId,  controller->GetDisk (ReverseSessionRig::kDiskDrive)->GetImageId(),              L"drive 1 reads its own disk");
        Assert::AreEqual<uint64_t> (otherId, controller->GetDisk (kOtherDrive)->GetImageId(),                                L"drive 2 reads its own disk");
        Assert::IsTrue (before == ReverseSessionRig::Save (target), L"and the machine is unchanged");

        Assert::IsFalse          (store.IsRetainingMedia(),         L"the store keeps no disks once the load is over");
        Assert::AreEqual<size_t> (0, store.GetRetainedMediaCount(), L"so the state's disks are let go");
    }


    //  The drive sounds follow the bays through a failed load, fed from the
    //  bay-change sink as the shell feeds them. Drive 1 starts empty and drive
    //  2 holds a disk. The load puts the state's disk in drive 1 and empties
    //  drive 2, then fails, and the bays go back as they were. Unless the sink
    //  receives that change too, drive 2's motor is silent with a disk in it
    //  and drive 1's hums with none, until the next real insert or eject.
    TEST_METHOD (AStateThatFailsToLoadLeavesEachDriveSoundWithItsBay)
    {
        constexpr int                                               kOtherDrive = 1;
        TestMachine                                                 source ("Apple2e");
        TestMachine                                                 target ("Apple2e");
        FileLog                                                     sourceLog;
        FileLog                                                     targetLog;
        std::vector<Byte>                                           bytes;
        std::vector<Byte>                                           otherDisk;
        MachineStateContents                                        contents;
        MachineStateError                                           error;
        std::array<Disk2AudioSource, DiskImageStore::kDriveCount>   sounds;
        DiskImageStore                                            & store       = target.GetDiskStore();
        bool                                                        isMounted   = false;
        int                                                         drive       = 0;
        HRESULT                                                     hr          = S_OK;



        Prepare (source, sourceLog, true);
        source.RunCycles (s_kStateWarmupCycles);

        bytes = Build (source);

        hr = MachineStateFile::Parse (bytes, contents, error);
        AssertSucceeded (hr, L"Parse");

        CutShort (contents.machineState);

        Prepare (target, targetLog, true);

        AssertSucceeded (ReadPatternImage (s_kChangedPattern, otherDisk), L"ReadPatternImage");

        hr = store.MountFromBytes (ReverseSessionRig::kDiskSlot, kOtherDrive, "C:\\Disks\\other.nib", DiskFormat::Nib, otherDisk);
        AssertSucceeded (hr, L"MountFromBytes");

        store.Eject (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive);

        FollowBaysWithSounds (target, sounds);

        Assert::IsFalse (sounds[ReverseSessionRig::kDiskDrive].IsDiskPresent(), L"drive 1's sound source has no disk present before the load");
        Assert::IsTrue  (sounds[kOtherDrive].IsDiskPresent(),                   L"and drive 2's has one");

        hr = MachineStateFile::Apply (target, contents, error);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"a state cut short inside its section must not load");

        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            isMounted = store.IsMounted (ReverseSessionRig::kDiskSlot, drive);

            Assert::AreEqual (isMounted, sounds[drive].IsDiskPresent(),
                              std::format (L"drive {}'s sound source has a disk present exactly when its bay holds one after the failed load", drive + 1).c_str());
        }

        Assert::IsFalse (store.IsMounted (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive), L"drive 1 is empty again");
        Assert::IsTrue  (store.IsMounted (ReverseSessionRig::kDiskSlot, kOtherDrive),                   L"and drive 2 holds its disk again");
    }


private:

    //  The rig's //e and loop, with a nibble image read from s_kImagePath in
    //  slot 6 drive 1 and every read and write of an image file counted.
    //  Without the loop, the machine is left as built, with the disk mounted.
    static void Prepare (TestMachine & machine, FileLog & log, bool withLoop)
    {
        DiskImageStore  & store = machine.GetDiskStore();
        HRESULT           hr    = S_OK;



        if (withLoop)
        {
            ReverseSessionRig::Prepare (machine);
        }

        store.SetImageReader ([&log] (const std::string &, std::vector<Byte> & bytes)
        {
            log.reads++;
            return ReadPatternImage (log.pattern, bytes);
        });

        store.SetIdentityReader ([] (const std::string &) { return ImageIdentity(); });

        store.SetBayChangeSink ([&machine] (int slot, int drive, BayChange)
        {
            if (slot == ReverseSessionRig::kDiskSlot)
            {
                machine.GetRefs().diskController->SetExternalDisk (drive, machine.GetDiskStore().GetImage (slot, drive));
            }
        });

        store.SetFlushSink ([&log] (const std::string &, const std::vector<Byte> &)
        {
            log.flushes++;
            return S_OK;
        });

        hr = store.Mount (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive, s_kImagePath);
        AssertSucceeded (hr, L"Mount");
    }


    static HRESULT ReadPatternImage (size_t pattern, std::vector<Byte> & bytes)
    {
        size_t  i = 0;



        bytes.assign (NibbleImageCodec::kNibImageSize, 0);

        for (i = 0; i < bytes.size(); i++)
        {
            bytes[i] = static_cast<Byte> ((i * pattern) | s_kStateHighBit);
        }

        return S_OK;
    }


    //  Replaces Prepare's bay-change sink with one that also feeds each slot 6
    //  bay change to that drive's sound, as DiskManager::OnBayChange does.
    //  Each sound starts with a disk exactly when its bay holds one.
    static void FollowBaysWithSounds (TestMachine & machine, std::array<Disk2AudioSource, DiskImageStore::kDriveCount> & sounds)
    {
        DiskImageStore  & store = machine.GetDiskStore();
        int               drive = 0;



        store.SetBayChangeSink ([&machine, &sounds] (int slot, int changedDrive, BayChange change)
        {
            if (slot != ReverseSessionRig::kDiskSlot)
            {
                return;
            }

            machine.GetRefs().diskController->SetExternalDisk (changedDrive, machine.GetDiskStore().GetImage (slot, changedDrive));

            switch (change)
            {
                case BayChange::Inserted:
                    sounds[changedDrive].OnDiskInserted();
                    break;

                case BayChange::Ejected:
                    sounds[changedDrive].OnDiskEjected();
                    break;

                case BayChange::Swapped:
                    sounds[changedDrive].OnDiskSwapped();
                    break;
            }
        });

        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            if (store.IsMounted (ReverseSessionRig::kDiskSlot, drive))
            {
                sounds[drive].OnDiskInserted();
            }
        }
    }


    //  Cuts the last bytes off a machine state inside its own section: the
    //  section's size is lowered to match, so the section and its header still
    //  read and the load fails in the last of the parts.
    static void CutShort (std::vector<Byte> & state)
    {
        constexpr size_t  kSizeOffset = sizeof (uint32_t) + sizeof (Word);
        constexpr size_t  kCut        = 16;
        uint32_t          size        = 0;
        bool              isLong      = state.size() > kSizeOffset + sizeof (size) + kCut;



        Assert::IsTrue (isLong, L"a machine state long enough to cut");

        memcpy (&size, state.data() + kSizeOffset, sizeof (size));
        size -= kCut;
        memcpy (state.data() + kSizeOffset, &size, sizeof (size));

        state.resize (state.size() - kCut);
    }


    static std::vector<Byte> Build (TestMachine & machine)
    {
        std::vector<Byte>  bytes;
        MachineStateError  error;
        HRESULT            hr = S_OK;



        hr = MachineStateFile::Build (machine, bytes, error);
        AssertSucceeded (hr, L"Build");

        return bytes;
    }


    static void Load (TestMachine & machine, const std::vector<Byte> & bytes)
    {
        MachineStateError  error;
        HRESULT            hr = S_OK;



        hr = LoadIfValid (machine, bytes, error);
        AssertSucceeded (hr, L"Parse and Apply");
    }


    //  As the shell loads a file: parse it, and apply it only if it parsed.
    static HRESULT LoadIfValid (TestMachine & machine, const std::vector<Byte> & bytes, MachineStateError & error)
    {
        MachineStateContents  contents;
        HRESULT               hr = S_OK;



        hr = MachineStateFile::Parse (bytes, contents, error);

        if (SUCCEEDED (hr))
        {
            hr = MachineStateFile::Apply (machine, contents, error);
        }

        return hr;
    }


    static Snapshot Take (TestMachine & machine)
    {
        Snapshot  snapshot;
        HRESULT   hr      = S_OK;
        Word      address = 0;



        snapshot.registers = machine.GetCpu()->GetCpu6502()->GetRegisters();
        snapshot.cycles    = machine.GetCpu()->GetTotalCycles();

        for (address = 0; address < s_kRamEnd; address++)
        {
            snapshot.ram.push_back (machine.GetMemoryBus().ReadByte (address));
        }

        hr = machine.GetDiskStore().GetImage (6, 0)->Serialize (snapshot.disk);
        AssertSucceeded (hr, L"Serialize");

        return snapshot;
    }


    static void AssertSame (const Snapshot & expected, const Snapshot & actual)
    {
        Assert::AreEqual<uint64_t> (expected.cycles,   actual.cycles,   L"cycle count");
        Assert::AreEqual<int> (expected.registers.pc, actual.registers.pc, L"PC");
        Assert::AreEqual<int> (expected.registers.a,  actual.registers.a,  L"A");
        Assert::AreEqual<int> (expected.registers.x,  actual.registers.x,  L"X");
        Assert::AreEqual<int> (expected.registers.y,  actual.registers.y,  L"Y");
        Assert::AreEqual<int> (expected.registers.sp, actual.registers.sp, L"SP");
        Assert::IsTrue (expected.ram  == actual.ram,  L"main RAM");
        Assert::IsTrue (expected.disk == actual.disk, L"the disk");
    }


    static void PatchRom (TestMachine & machine)
    {
        RomDevice  * rom     = nullptr;
        Word         last    = 0;
        bool         patched = false;



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
    }
};
