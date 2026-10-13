#include "Pch.h"
#include "FixtureProvider.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "TestMachine.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Shell/MachineStateFile.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateCompatibilityTests
//
//  A machine state saved by a release of Casso loads in every later build:
//  each state under UnitTest/Fixtures/States/ is loaded into a machine of
//  its kind and run for a second. A change to a part's saved state that
//  stops an older form from loading fails here rather than for the users
//  whose automatic states were saved by that release.
//
//  The fixtures are saved with the fixture ROMs, which are the ROMs every
//  test machine uses, so a state's ROM identity matches the machine it loads
//  into. A release saves its set by running WriteStateFixturesForARelease
//  with CASSO_WRITE_STATE_FIXTURES set to the folder to write.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineStateCompatibilityTests)
{
public:

    static constexpr const char *     kStatesFolder     = "States";
    static constexpr const char *     kDiskFixture      = "synthetic.dsk";
    static constexpr const wchar_t *  kWriteVariable    = L"CASSO_WRITE_STATE_FIXTURES";
    static constexpr uint64_t         kOneSecondCycles  = 1020484;
    static constexpr uint64_t         kBootCycles       = 3000000;
    static constexpr int              kDiskSlot         = 6;
    static constexpr int              kDiskDrive        = 0;

    TEST_METHOD (EveryReleasedStateLoadsAndRuns)
    {
        FixtureProvider        fixtures;
        std::filesystem::path  root   = std::filesystem::path (fixtures.GetRoot()) / kStatesFolder;
        std::error_code        ec;
        size_t                 loaded = 0;



        for (const std::filesystem::directory_entry & entry : std::filesystem::recursive_directory_iterator (root, ec))
        {
            if (entry.path().extension() != std::wstring (L".") + MachineStateFile::kExtension)
            {
                continue;
            }

            LoadAndRun (entry.path());
            loaded++;
        }

        Assert::IsTrue (loaded > 0, L"UnitTest/Fixtures/States/ holds at least one saved state");
    }


    //  The release step's tool: with CASSO_WRITE_STATE_FIXTURES set to a
    //  folder, saves one state per machine kind there, each machine booted
    //  from a synthetic disk where it has a disk drive. Without the
    //  variable it writes nothing.
    TEST_METHOD (WriteStateFixturesForARelease)
    {
        constexpr DWORD   kPathChars         = 1024;
        wchar_t           folder[kPathChars] = {};
        DWORD             length             = GetEnvironmentVariableW (kWriteVariable, folder, kPathChars);
        std::error_code   ec;



        if (length == 0 || length >= kPathChars)
        {
            return;
        }

        std::filesystem::create_directories (folder, ec);

        for (const char * machineId : { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced", "Apple2c" })
        {
            WriteFixture (machineId, std::filesystem::path (folder));
        }
    }


private:

    static void LoadAndRun (const std::filesystem::path & path)
    {
        std::vector<Byte>             bytes;
        MachineStateContents          contents;
        MachineStateError             error;
        std::string                   machineId;
        std::unique_ptr<TestMachine>  machine;
        uint64_t                      before    = 0;
        HRESULT                       hr        = S_OK;



        bytes = ReadFile (path);

        hr = MachineStateFile::Parse (bytes, contents, error);
        AssertSucceeded (hr, std::format (L"{} parses", path.filename().wstring()).c_str());

        for (wchar_t ch : contents.machineName)
        {
            machineId.push_back (static_cast<char> (ch));
        }

        machine = std::make_unique<TestMachine> (machineId);

        hr = MachineStateFile::Apply (*machine, contents, error);
        AssertSucceeded (hr, std::format (L"{} loads into a {}", path.filename().wstring(), contents.machineName).c_str());

        before = machine->GetCpu()->GetTotalCycles();
        machine->RunCycles (kOneSecondCycles);

        Assert::IsTrue (machine->GetCpu()->GetTotalCycles() >= before + kOneSecondCycles,
                        std::format (L"{} runs a second after it loads", path.filename().wstring()).c_str());
    }


    static void WriteFixture (const char * machineId, const std::filesystem::path & folder)
    {
        TestMachine            machine (machineId);
        std::vector<Byte>      disk;
        std::vector<Byte>      bytes;
        MachineStateError      error;
        std::filesystem::path  path    = folder / (std::string (machineId) + ".cassostate");
        HRESULT                hr      = S_OK;



        disk = MakeSyntheticDisk();

        machine.PowerCycle();

        // A machine with a Disk II controller boots the disk, so its state has
        // a disk in it; one without saves a state with none. The disk is made
        // here rather than taken from a real one, so the fixtures contain no
        // third-party software.
        if (machine.GetRefs().diskController != nullptr)
        {
            hr = machine.GetDiskStore().MountFromBytes (kDiskSlot, kDiskDrive, kDiskFixture, DiskFormat::Dsk, disk);
            AssertSucceeded (hr, std::format (L"the disk mounts in the {}", std::wstring (machineId, machineId + strlen (machineId))).c_str());

            machine.GetRefs().diskController->SetExternalDisk (kDiskDrive, machine.GetDiskStore().GetImage (kDiskSlot, kDiskDrive));
        }

        machine.RunCycles (kBootCycles);

        hr = MachineStateFile::Build (machine, bytes, error);
        AssertSucceeded (hr, L"Build");

        hr = DiskImageStore::WriteFileAtomically (path.string(), bytes);
        AssertSucceeded (hr, L"write the fixture");
    }


    //  A 140 KB DOS-order image original to this test: the boot sector jumps
    //  to itself at $0801, so a machine that boots it runs in legal code, and
    //  every other byte follows a fixed pattern so the saved disk has content.
    static std::vector<Byte> MakeSyntheticDisk()
    {
        constexpr Byte     kJmp        = 0x4C;
        constexpr Word     kBootEntry  = 0x0801;
        constexpr size_t   kSectorSize = 256;
        constexpr size_t   kPatternMod = 251;
        std::vector<Byte>  disk (NibblizationLayer::kImageByteSize, 0);
        size_t             i           = 0;



        for (i = kSectorSize; i < disk.size(); i++)
        {
            disk[i] = static_cast<Byte> (i % kPatternMod);
        }

        disk[1] = kJmp;
        disk[2] = static_cast<Byte> (kBootEntry);
        disk[3] = static_cast<Byte> (kBootEntry >> CHAR_BIT);

        return disk;
    }


    static std::vector<Byte> ReadFile (const std::filesystem::path & path)
    {
        std::ifstream      in (path, std::ios::binary);
        std::vector<Byte>  bytes ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());



        Assert::IsTrue (in.good() || in.eof(), std::format (L"{} reads", path.filename().wstring()).c_str());

        return bytes;
    }
};





