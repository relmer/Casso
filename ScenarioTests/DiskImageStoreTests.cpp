#include "Pch.h"
#include "EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStoreTests
//
//  The store's commit against real files in the system temp folder: the
//  cases of the unit suite's DiskImageStoreTests that cannot run on a flush
//  sink.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskImageStoreTests)
{
public:

    static constexpr int   kSlot  = 6;
    static constexpr int   kDrive = 0;

    vector<Byte> MakeDsk (Byte fill)
    {
        return vector<Byte> (NibblizationLayer::kImageByteSize, fill);
    }

    ////////////////////////////////////////////////////////////////////////
    //
    //  Atomic persist. FlushEntry used to open the mounted image's own path
    //  with ofstream, which truncates it before the first byte is written --
    //  so a write that then failed (full volume, disconnected share) left no
    //  file at all, and nothing checked the stream afterwards, so the image
    //  was marked clean either way. These cover the replacement: the target
    //  is only ever swapped for a fully written temp file.
    //
    //  Not covered here: a write that fails PART WAY through. Forcing that
    //  needs a filesystem seam this class does not have, so the guarantee it
    //  rests on -- the target is untouched until the temp file is complete --
    //  is checked structurally below rather than by simulating a short write.
    //
    ////////////////////////////////////////////////////////////////////////

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

        AssertSucceeded (DiskImageStore::WriteFileAtomically (target.string(), bytes));

        Assert::AreEqual (size_t (2048), ReadBackAll (target).size(),
            L"the target must hold the new bytes, not the stale ones");
        Assert::IsFalse (fs::exists (DiskImageStore::GetCommitTemporaryPath (target.string(), 0)),
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

        hr = DiskImageStore::WriteFileAtomically (target.string(), bytes);

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
        Assert::IsFalse (fs::exists (DiskImageStore::GetCommitTemporaryPath (target.string(), 0)),
            L"no temp file may be left behind on failure");
    }


    TEST_METHOD (Flush_ToRealFile_WritesThroughAtomicPath)
    {
        // The production branch -- no flush sink -- end to end on a real
        // file. Every other test in this class installs a sink, so this path
        // had no coverage at all.
        DiskImageStore   store;
        fs::path         target = ScratchPath ("flush.dsk");
        vector<Byte>     seedBytes = MakeDsk (0x24);
        std::error_code  ec;

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, target.string(),
                                               DiskFormat::Dsk, seedBytes));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0,
            static_cast<uint8_t> (store.GetImage (kSlot, kDrive)->ReadBit (0, 0) ^ 1));

        AssertSucceeded (store.Flush (kSlot, kDrive));

        Assert::AreEqual (static_cast<size_t> (NibblizationLayer::kImageByteSize), ReadBackAll (target).size(),
            L"a real-file flush must write a full-size image");
        Assert::IsFalse (fs::exists (DiskImageStore::GetCommitTemporaryPath (target.string(), 0)),
            L"the flush must not leave its temp file beside the image");

        fs::remove (target, ec);
    }
};
