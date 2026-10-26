#include "Pch.h"
#include "EhmTestHelper.h"
#include "Devices/Disk/CommitPlan.h"
#include "Devices/Disk/DiskImageStore.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageTests
//
//  The commit of an image other processes may share, against real files in
//  the system temp folder: the cases of the unit suite's SharedImageTests
//  that write real files.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SharedImageTests)
{
public:
    TEST_METHOD (AWriteThatFailsLeavesTheImageByteForByte)
    {
        fs::path           folder = fs::temp_directory_path() / L"CassoTwoWriterTests";
        fs::path           target = folder / L"work.dsk";
        std::vector<Byte>  before (512, 0x5A);
        std::vector<Byte>  after;
        std::error_code    ec;



        fs::remove_all      (folder, ec);
        fs::create_directories (folder, ec);

        AssertSucceeded (DiskImageStore::WriteFileAtomically (target.string(), before));

        //  EVERY name the commit could take is occupied by a directory, which
        //  no stream can open -- so the write fails after the target already
        //  holds something worth keeping. All of them, because stepping past an
        //  occupied name is exactly what the commit does.
        for (unsigned attempt = 0; attempt < CommitPlan::kMaxAttempts; attempt++)
        {
            fs::create_directories (
                fs::path (DiskImageStore::GetCommitTemporaryPath (target.string(), attempt)), ec);
        }

        {
            std::vector<Byte>  replacement (512, 0xA5);
            HRESULT            hrWrite     = DiskImageStore::WriteFileAtomically (
                                                 target.string(), replacement);

            Assert::IsTrue (FAILED (hrWrite), L"the write could not have succeeded");
        }

        AssertSucceeded (DiskImageStore::ReadFileBytes (target.string(), after));

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

        AssertSucceeded (DiskImageStore::WriteFileAtomically (target.string(), bytes));

        //  The target was never opened directly: had it been, a reader could
        //  have seen it truncated. The evidence a test can hold is that the
        //  bytes arrived whole and the temporary the commit went through is
        //  gone -- so a later refactor that writes in place fails here rather
        //  than silently reintroducing a partly written image.
        Assert::AreEqual ((size_t) 4096, fs::file_size (target, ec));
        Assert::IsFalse (fs::exists (
                             DiskImageStore::GetCommitTemporaryPath (target.string(), 0), ec),
                         L"the commit went through a temporary and consumed it");

        fs::remove_all (folder, ec);
    }



    TEST_METHOD (ATemporaryLeftByAKilledWriterIsSteppedPastRatherThanAdopted)
    {
        fs::path           folder    = fs::temp_directory_path() / L"CassoAbandonedTempTests";
        fs::path           target    = folder / L"work.dsk";
        std::string        abandoned = DiskImageStore::GetCommitTemporaryPath (target.string(), 0);
        std::vector<Byte>  bytes (256, 0x77);
        std::vector<Byte>  readBack;
        std::error_code    ec;



        fs::remove_all      (folder, ec);
        fs::create_directories (folder, ec);

        //  What a writer killed mid-commit leaves behind.
        AssertSucceeded (DiskImageStore::WriteFileAtomically (abandoned, std::vector<Byte> (99, 0x11)));

        AssertSucceeded (DiskImageStore::WriteFileAtomically (target.string(), bytes));

        AssertSucceeded (DiskImageStore::ReadFileBytes (target.string(), readBack));

        //  Adopting it would have appended this image to the remains of
        //  somebody else's, or renamed those remains over the target.
        Assert::IsTrue (readBack == bytes);
        Assert::IsTrue (fs::exists (abandoned, ec),
                        L"and the leftover is left alone rather than consumed");

        fs::remove_all (folder, ec);
    }
};
