#include "Pch.h"

#include "EhmTestHelper.h"
#include "Devices/Disk/Store/DiskFileCommit.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommitScenarioTests
//
//  Atomic replace against real files in the temp directory.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskFileCommitScenarioTests)
{
public:

    // A scratch path in the system temp directory, removed by the caller.
    static fs::path ScratchPath (const char * name)
    {
        return fs::temp_directory_path() / (string ("casso_atomic_") + name);
    }


    static string ReadBackAll (const fs::path & path)
    {
        ifstream           file (path, ios::binary);
        std::stringstream  buffer;

        buffer << file.rdbuf();

        return buffer.str();
    }


    TEST_METHOD (WriteFileAtomically_ReplacesTargetAndLeavesNoTempBehind)
    {
        fs::path         target = ScratchPath ("replace.bin");
        vector<Byte>     bytes  (2048, 0x5A);
        std::error_code  ec;

        {
            ofstream  seed (target, ios::binary);
            seed << "stale content";
        }

        AssertSucceeded (DiskFileCommit::WriteFileAtomically (target.string(), bytes));

        Assert::AreEqual (size_t (2048), ReadBackAll (target).size(),
            L"the target must hold the new bytes, not the stale ones");
        Assert::IsFalse (fs::exists (DiskFileCommit::GetCommitTemporaryPath (target.string(), 0)),
            L"a successful write must not leave its temp file behind");

        fs::remove (target, ec);
    }

    TEST_METHOD (WriteFileAtomically_UnwritableTarget_LeavesOriginalIntact)
    {
        // The target directory does not exist, so the temp file cannot be
        // created. The point is what does NOT happen: no partial file, no
        // truncated target, and a reported failure rather than a silent one.
        fs::path      missingDir = ScratchPath ("no_such_dir");
        fs::path      target     = missingDir / "image.dsk";
        vector<Byte>  bytes (512, 0x11);
        HRESULT       hr         = S_OK;

        Assert::IsFalse (fs::exists (missingDir), L"precondition: the directory must not exist");

        hr = DiskFileCommit::WriteFileAtomically (target.string(), bytes);

        Assert::IsTrue (FAILED (hr),
            L"an impossible write must report failure, not succeed silently");

        //  AND IT REPORTS WHICH FAILURE. This came back as E_FAIL, so the
        //  save-failure notice -- built to print the code and the system's own
        //  words for it -- said "Unspecified error" for a folder that refused
        //  the file. The second assertion is the one that would have caught it.
        Assert::AreEqual ((int) HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND), (int) hr,
            L"the system's own code for a folder that is not there");
        Assert::IsTrue (hr != E_FAIL, L"never the code that says nothing");
        Assert::IsFalse (fs::exists (target), L"no partial target may be left behind");
        Assert::IsFalse (fs::exists (DiskFileCommit::GetCommitTemporaryPath (target.string(), 0)),
            L"no temp file may be left behind on failure");
    }
};
