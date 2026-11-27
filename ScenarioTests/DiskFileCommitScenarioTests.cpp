#include "Pch.h"

#include "EhmTestHelper.h"
#include "Devices/Disk/CommitPlan.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/Store/DiskFlusher.h"
#include "Devices/Disk/Store/DiskFileCommit.h"
#include "Machines/Apple2/Common/NibbleFormat.h"

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


    TEST_METHOD (AWriteThatFailsLeavesTheImageByteForByte)
    {
        fs::path           folder = fs::temp_directory_path() / L"CassoTwoWriterTests";
        fs::path           target = folder / L"work.dsk";
        std::vector<Byte>  before (512, 0x5A);
        std::vector<Byte>  after;
        std::error_code    ec;



        fs::remove_all      (folder, ec);
        fs::create_directories (folder, ec);

        AssertSucceeded (DiskFileCommit::WriteFileAtomically (target.string(), before));

        //  EVERY name the commit could take is occupied by a directory, which
        //  no stream can open -- so the write fails after the target already
        //  holds something worth keeping. All of them, because stepping past an
        //  occupied name is exactly what the commit does.
        for (unsigned attempt = 0; attempt < CommitPlan::kMaxAttempts; attempt++)
        {
            fs::create_directories (
                fs::path (DiskFileCommit::GetCommitTemporaryPath (target.string(), attempt)), ec);
        }

        {
            std::vector<Byte>  replacement (512, 0xA5);
            HRESULT            hrWrite     = DiskFileCommit::WriteFileAtomically (
                                                 target.string(), replacement);

            Assert::IsTrue (FAILED (hrWrite), L"the write could not have succeeded");
        }

        AssertSucceeded (DiskFileCommit::ReadFileBytes (target.string(), after));

        //  The guarantee that makes a failed flush survivable: the file on disk
        //  is what it was, not a truncated or half-written version of what was
        //  attempted.
        Assert::IsTrue (after == before,
                        L"a failed write leaves the image exactly as it was");

        fs::remove_all (folder, ec);
    }


    TEST_METHOD (ACommitIsVisibleOnlyAsAWholeVersion)
    {
        fs::path           folder = fs::temp_directory_path() / L"CassoAtomicCommitTests";
        fs::path           target = folder / L"work.dsk";
        std::vector<Byte>  bytes (4096, 0x33);
        std::error_code    ec;



        fs::remove_all      (folder, ec);
        fs::create_directories (folder, ec);

        AssertSucceeded (DiskFileCommit::WriteFileAtomically (target.string(), bytes));

        //  The target was never opened directly: had it been, a reader could
        //  have seen it truncated. The evidence a test can hold is that the
        //  bytes arrived whole and the temporary the commit went through is
        //  gone -- so a later refactor that writes in place fails here rather
        //  than silently reintroducing a partly written image.
        Assert::AreEqual ((size_t) 4096, fs::file_size (target, ec));
        Assert::IsFalse (fs::exists (
                             DiskFileCommit::GetCommitTemporaryPath (target.string(), 0), ec),
                         L"the commit went through a temporary and consumed it");

        fs::remove_all (folder, ec);
    }


    TEST_METHOD (ATemporaryLeftByAKilledWriterIsSteppedPastRatherThanAdopted)
    {
        fs::path           folder    = fs::temp_directory_path() / L"CassoAbandonedTempTests";
        fs::path           target    = folder / L"work.dsk";
        std::string        abandoned = DiskFileCommit::GetCommitTemporaryPath (target.string(), 0);
        std::vector<Byte>  bytes (256, 0x77);
        std::vector<Byte>  readBack;
        std::error_code    ec;



        fs::remove_all      (folder, ec);
        fs::create_directories (folder, ec);

        //  What a writer killed mid-commit leaves behind.
        AssertSucceeded (DiskFileCommit::WriteFileAtomically (abandoned, std::vector<Byte> (99, 0x11)));

        AssertSucceeded (DiskFileCommit::WriteFileAtomically (target.string(), bytes));

        AssertSucceeded (DiskFileCommit::ReadFileBytes (target.string(), readBack));

        //  Adopting it would have appended this image to the remains of
        //  somebody else's, or renamed those remains over the target.
        Assert::IsTrue (readBack == bytes);
        Assert::IsTrue (fs::exists (abandoned, ec),
                        L"and the leftover is left alone rather than consumed");

        fs::remove_all (folder, ec);
    }


    static constexpr int   kSlot  = 6;
    static constexpr int   kDrive = 0;

    vector<Byte> MakeDsk (Byte fill)
    {
        return vector<Byte> (NibbleFormat::kImageByteSize, fill);
    }


    TEST_METHOD (Flush_ToRealFile_WritesThroughAtomicPath)
    {
        // The production branch -- no flush sink -- end to end on a real
        // file. Every test in DiskImageStoreTests installs a sink, so this path
        // has no other coverage.
        DiskImageStore   store;
        fs::path         target = ScratchPath ("flush.dsk");
        vector<Byte>     seedBytes = MakeDsk (0x24);
        std::error_code  ec;

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, target.string(),
                                               DiskFormat::Dsk, seedBytes));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0,
            static_cast<uint8_t> (store.GetImage (kSlot, kDrive)->ReadBit (0, 0) ^ 1));

        AssertSucceeded (store.GetFlusher().Flush (kSlot, kDrive));

        Assert::AreEqual (static_cast<size_t> (NibbleFormat::kImageByteSize), ReadBackAll (target).size(),
            L"a real-file flush must write a full-size image");
        Assert::IsFalse (fs::exists (DiskFileCommit::GetCommitTemporaryPath (target.string(), 0)),
            L"the flush must not leave its temp file beside the image");

        fs::remove (target, ec);
    }
};
