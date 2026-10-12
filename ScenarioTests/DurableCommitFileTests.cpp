#include "Pch.h"
#include "EhmTestHelper.h"
#include "Devices/Disk/DurableCommit.h"
#include "Seams/Win32DiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DurableCommitFileTests
//
//  The platform half of a durable commit, against real files: a flush, the
//  metadata a replace must keep, a rename that will not overwrite, and a
//  whole commit that keeps a hidden image hidden.
//
//  Each test works in a scratch folder of its own and removes it before it
//  asserts anything, so a failure leaves nothing behind.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DurableCommitFileTests)
{
public:

    static std::filesystem::path MakeScratch (const wchar_t * name)
    {
        std::error_code        ec;
        std::filesystem::path  dir = std::filesystem::temp_directory_path (ec)
                                     / std::format (L"casso-durable-{}-{}-{}", GetCurrentProcessId(), GetTickCount64(), name);



        std::filesystem::create_directories (dir, ec);

        return dir;
    }



    static void RemoveScratch (const std::filesystem::path & dir)
    {
        std::error_code  ec;



        for (const auto & entry : std::filesystem::directory_iterator (dir, ec))
        {
            SetFileAttributesW (entry.path().c_str(), FILE_ATTRIBUTE_NORMAL);
        }

        std::filesystem::remove_all (dir, ec);
    }



    static std::string Narrow (const std::filesystem::path & path)
    {
        return path.string();
    }



    TEST_METHOD (FlushingAWrittenFileSucceeds)
    {
        Win32DiskFileIo        io;
        std::filesystem::path  dir     = MakeScratch (L"flush");
        std::string            file    = Narrow (dir / "a.dsk");
        HRESULT                written = io.WriteAllBytes (file, vector<Byte> (4096, 0x5A));
        HRESULT                flushed = io.FlushToStorage (file);
        HRESULT                missing = io.FlushToStorage (Narrow (dir / "none.dsk"));



        RemoveScratch (dir);

        AssertSucceeded (written);
        AssertSucceeded (flushed);
        Assert::IsTrue (FAILED (missing), L"a file that is not there cannot be flushed");
    }



    TEST_METHOD (MetadataGoesToTheNewFile)
    {
        Win32DiskFileIo            io;
        std::filesystem::path      dir      = MakeScratch (L"meta");
        std::string                from     = Narrow (dir / "old.dsk");
        std::string                to       = Narrow (dir / "new.dsk");
        WIN32_FILE_ATTRIBUTE_DATA  oldData  = {};
        WIN32_FILE_ATTRIBUTE_DATA  newData  = {};
        FILETIME                   created  = { 0x12345678, 0x01D00000 };
        HANDLE                     handle   = INVALID_HANDLE_VALUE;
        HRESULT                    copied   = E_FAIL;



        (void) io.WriteAllBytes (from, vector<Byte> (16, 1));
        (void) io.WriteAllBytes (to,   vector<Byte> (16, 2));

        handle = CreateFileW (std::filesystem::path (from).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        SetFileTime (handle, &created, nullptr, nullptr);
        CloseHandle (handle);
        SetFileAttributesW (std::filesystem::path (from).c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);

        copied = io.CopyFileMetadata (from, to);
        GetFileAttributesExW (std::filesystem::path (from).c_str(), GetFileExInfoStandard, &oldData);
        GetFileAttributesExW (std::filesystem::path (to).c_str(),   GetFileExInfoStandard, &newData);

        RemoveScratch (dir);

        AssertSucceeded (copied);
        Assert::IsTrue ((newData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0);
        Assert::IsTrue ((newData.dwFileAttributes & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED) != 0);
        Assert::AreEqual (oldData.ftCreationTime.dwLowDateTime,  newData.ftCreationTime.dwLowDateTime);
        Assert::AreEqual (oldData.ftCreationTime.dwHighDateTime, newData.ftCreationTime.dwHighDateTime);
    }



    TEST_METHOD (RenameWithoutReplacingNeverOverwrites)
    {
        Win32DiskFileIo        io;
        std::filesystem::path  dir      = MakeScratch (L"rename");
        std::string            temp     = Narrow (dir / "t.tmp");
        std::string            target   = Narrow (dir / "a.dsk");
        HRESULT                first    = E_FAIL;
        HRESULT                second   = E_FAIL;
        vector<Byte>           kept;



        (void) io.WriteAllBytes (temp, vector<Byte> (8, 1));
        first = io.RenameWithoutReplacing (temp, target);

        (void) io.WriteAllBytes (temp, vector<Byte> (8, 2));
        second = io.RenameWithoutReplacing (temp, target);
        (void) io.ReadAllBytes (target, kept);

        RemoveScratch (dir);

        AssertSucceeded (first);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS), second);
        Assert::IsTrue (kept == vector<Byte> (8, 1), L"the first file is still there");
    }



    TEST_METHOD (ACommitKeepsAHiddenImageHiddenAndLeavesNoTemporary)
    {
        Win32DiskFileIo        io;
        CommitPlan::Progress   progress;
        std::filesystem::path  dir      = MakeScratch (L"commit");
        std::string            target   = Narrow (dir / "a.dsk");
        HRESULT                hr       = E_FAIL;
        DWORD                  attrs    = 0;
        vector<Byte>           result;
        int                    files    = 0;
        std::error_code        ec;



        (void) io.WriteAllBytes (target, vector<Byte> (143360, 0x11));
        SetFileAttributesW (std::filesystem::path (target).c_str(), FILE_ATTRIBUTE_HIDDEN);

        hr    = DurableCommit::Commit (io, target, vector<Byte> (143360, 0x22), CommitPlan::NextInvocationTag(), CommitMode::Replace, progress);
        attrs = GetFileAttributesW (std::filesystem::path (target).c_str());
        (void) io.ReadAllBytes (target, result);

        for (const auto & entry : std::filesystem::directory_iterator (dir, ec))
        {
            (void) entry;
            files++;
        }

        RemoveScratch (dir);

        AssertSucceeded (hr);
        Assert::IsTrue (result == vector<Byte> (143360, 0x22));
        Assert::IsTrue ((attrs & FILE_ATTRIBUTE_HIDDEN) != 0, L"the replaced image is still hidden");
        Assert::AreEqual (1, files, L"no temporary beside the image");
    }
};
