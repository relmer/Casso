#include "Pch.h"
#include "TestMachine.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/RomDevice.h"
#include "Shell/AutoStatePolicy.h"
#include "Shell/MachineStateFile.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AutoStateSafetyTests
//
//  An automatic state is the user's only copy of their session, so nothing
//  that fails may change it: a resume that fails only reads the file, and
//  after a replacement that fails partway the earlier file is as it was. Each
//  test writes a real automatic state into a folder of its own under the
//  temporary folder and compares the file byte for byte afterward.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AutoStateSafetyTests)
{
public:

    static constexpr const wchar_t *  kMachineName = L"Apple2e";
    static constexpr uint64_t         kRunCycles   = 200000;

    TEST_METHOD_INITIALIZE (MakeFolder)
    {
        std::error_code  ec;



        m_folder = std::filesystem::temp_directory_path (ec) / std::format (L"CassoAutoState-{}", GetCurrentProcessId());
        std::filesystem::create_directories (m_folder, ec);
    }


    TEST_METHOD_CLEANUP (RemoveFolder)
    {
        std::error_code  ec;



        std::filesystem::remove_all (m_folder, ec);
    }


    TEST_METHOD (AFailedResumeChangesNoByteOfTheStateFile)
    {
        std::filesystem::path  path     = AutoStatePolicy::GetAutoStatePath (m_folder, kMachineName);
        std::vector<Byte>      before;
        std::vector<Byte>      after;
        MachineStateContents   contents;
        MachineStateError      error;
        TestMachine            patched ("Apple2e");
        HRESULT                hr       = S_OK;



        before = SaveState (path);

        PatchRom (patched);

        hr = MachineStateFile::Parse (ReadFile (path), contents, error);
        AssertSucceeded (hr, L"Parse");

        hr = MachineStateFile::Apply (patched, contents, error);
        Assert::IsTrue (FAILED (hr), L"a state saved with other ROM images does not resume");
        Assert::AreEqual (std::string ("different ROM images"), error.label);

        after = ReadFile (path);
        Assert::IsTrue (before == after, L"the automatic state file is byte for byte as it was");
    }


    TEST_METHOD (AFailedReplacementChangesNoByteOfTheEarlierState)
    {
        constexpr size_t       kNewerBytes = 4096;
        constexpr Byte         kNewerFill  = 0x5A;
        std::filesystem::path  path        = AutoStatePolicy::GetAutoStatePath (m_folder, kMachineName);
        std::vector<Byte>      before;
        std::vector<Byte>      newer       (kNewerBytes, kNewerFill);
        HANDLE                 open        = INVALID_HANDLE_VALUE;
        HRESULT                hr          = S_OK;



        before = SaveState (path);

        // Another program has the state open without delete sharing, as a
        // backup tool or a second Casso might, so the final rename fails after
        // the new state has been written in full to its temporary file.
        open = CreateFileW (path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Assert::IsTrue (open != INVALID_HANDLE_VALUE, L"the state file is held open");

        hr = DiskImageStore::WriteFileAtomically (path.string(), newer);

        CloseHandle (open);

        Assert::IsTrue (FAILED (hr), L"replacing a state another program holds open fails");
        Assert::IsTrue (before == ReadFile (path), L"the earlier automatic state is byte for byte as it was");
    }


private:

    std::filesystem::path  m_folder;

    static std::vector<Byte> SaveState (const std::filesystem::path & path)
    {
        TestMachine        machine ("Apple2e");
        std::vector<Byte>  bytes;
        MachineStateError  error;
        HRESULT            hr      = S_OK;



        machine.RunCycles (kRunCycles);

        hr = MachineStateFile::Build (machine, bytes, error);
        AssertSucceeded (hr, L"Build");

        hr = DiskImageStore::WriteFileAtomically (path.string(), bytes);
        AssertSucceeded (hr, L"the automatic state is written");

        return bytes;
    }


    static std::vector<Byte> ReadFile (const std::filesystem::path & path)
    {
        std::ifstream      in (path, std::ios::binary);
        std::vector<Byte>  bytes ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());



        return bytes;
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





