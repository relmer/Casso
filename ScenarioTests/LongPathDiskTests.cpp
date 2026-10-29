#include "Pch.h"
#include "EhmTestHelper.h"
#include "Core/LongPath.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Seams/Win32DiskFileIo.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LongPathDiskTests
//
//  A disk image at a path longer than MAX_PATH, against real files: it
//  mounts, saves and reads back through the paths the user gave, which stay
//  free of the \\?\ prefix everywhere but at the calls into Windows.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LongPathDiskTests)
{
public:

    static constexpr int  kSlot  = 6;
    static constexpr int  kDrive = 0;



    TEST_METHOD (ADiskBeyondMaxPathMountsSavesAndReadsBack)
    {
        std::error_code          ec;
        std::filesystem::path    root    = std::filesystem::temp_directory_path (ec)
                                           / std::format (L"casso-longpath-{}-{}", GetCurrentProcessId(), GetTickCount64());
        std::filesystem::path    folder  = root;
        std::string              path;
        Win32DiskFileIo          io;
        auto                     store   = std::make_unique<DiskImageStore>();
        vector<Byte>             before;
        vector<Byte>             after;
        HRESULT                  hr      = S_OK;
        int                      i       = 0;



        for (i = 0; i < 6; i++)
        {
            folder /= std::format (L"a folder with a long name to pass MAX_PATH {}", i);
        }

        path = (folder / "a disk image beyond MAX_PATH.dsk").string();

        std::filesystem::create_directories (std::filesystem::path (LongPath::ToWin32 (folder.wstring())), ec);
        hr = io.WriteAllBytes (path, vector<Byte> (NibblizationLayer::kImageByteSize, 0x00));

        store->SetFileIo (&io);

        if (SUCCEEDED (hr))
        {
            hr = store->Mount (kSlot, kDrive, path);
        }

        if (SUCCEEDED (hr))
        {
            before = vector<Byte> (NibblizationLayer::kImageByteSize, 0x00);
            store->GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
            hr = store->Flush (kSlot, kDrive);
        }

        if (SUCCEEDED (hr))
        {
            hr = io.ReadAllBytes (path, after);
        }

        store.reset();
        std::filesystem::remove_all (std::filesystem::path (LongPath::ToWin32 (root.wstring())), ec);

        Assert::IsTrue   (path.size() > MAX_PATH, L"the path is beyond MAX_PATH");
        AssertSucceeded  (hr, L"the disk mounts, saves and reads back");
        Assert::IsTrue   (before == after, L"the saved file has the image");
        Assert::IsFalse  (std::filesystem::exists (std::filesystem::path (LongPath::ToWin32 (root.wstring())), ec), L"and the scratch folder is gone");
    }
};
