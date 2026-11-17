#include "Pch.h"

#include "EhmTestHelper.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Config/Win32FileSystem.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Seams/Win32DiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PickerTimingTests
//
//  A folder of 500 images on the local drive, listed the way the picker
//  lists it: the rows, then the File system and Volume name columns, then
//  both again on a return visit. The times are written to the test's output
//  and to %TEMP%\CassoPickerTiming.txt. Only the results are checked, since
//  the times depend on the build and the machine.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PickerTimingTests)
{
public:

    static constexpr int  kImages = 500;

    using Clock = std::chrono::steady_clock;


    //  Every 5th image a WOZ, and the others DOS 3.3 and ProDOS by turns.
    static void WriteImages (const std::filesystem::path & folder)
    {
        Win32DiskFileIo  io;
        vector<Byte>     dsk;
        vector<Byte>     po;
        vector<Byte>     woz;
        BlankDiskSpec    spec;
        BootPayload      payload;



        spec.format   = DiskFormat::Dsk;
        spec.contents = BlankDiskContents::Dos33;
        AssertSucceeded (BlankDiskBuilder::Build (spec, payload, dsk));

        spec.format   = DiskFormat::Po;
        spec.contents = BlankDiskContents::ProDos;
        AssertSucceeded (BlankDiskBuilder::Build (spec, payload, po));

        spec.format   = DiskFormat::Woz;
        spec.contents = BlankDiskContents::Dos33;
        AssertSucceeded (BlankDiskBuilder::Build (spec, payload, woz));

        for (int i = 0; i < kImages; i++)
        {
            bool                   isWoz = i % 5 == 4;
            bool                   isPo  = !isWoz && i % 2 == 1;
            std::filesystem::path  path  = folder / std::format (L"Disk {:03}.{}", i, isWoz ? L"woz" : isPo ? L"po" : L"dsk");

            AssertSucceeded (io.WriteAllBytes (TextEncoding::WideToUtf8 (path.wstring()), isWoz ? woz : isPo ? po : dsk));
        }
    }


    static size_t CountImages (const CassoExplorerBrowser & browser)
    {
        return (size_t) std::count_if (browser.GetRows().begin(), browser.GetRows().end(), [] (const CatalogRow & row) { return row.isDiskImage; });
    }


    static bool HasAllFacts (const CassoExplorerBrowser & browser)
    {
        return std::all_of (browser.GetRows().begin(), browser.GetRows().end(), [] (const CatalogRow & row) { return !row.isDiskImage || !row.collection.fileSystem.empty(); });
    }


    //  Lists the folder, then waits for both columns, applying the facts as
    //  the window does when the worker reports them.
    static void List (CassoExplorerBrowser & browser, const std::filesystem::path & folder, double & outListMs, double & outColumnsMs)
    {
        Clock::time_point  start = Clock::now();
        Clock::time_point  limit = start + std::chrono::seconds (60);



        browser.NavigateToLocation (Location::MakeHostFolder (folder.wstring()));
        outListMs = std::chrono::duration<double, std::milli> (Clock::now() - start).count();

        Assert::AreEqual ((size_t) kImages, CountImages (browser));

        while (!HasAllFacts (browser) && Clock::now() < limit)
        {
            browser.ApplyVolumeFacts();
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }

        outColumnsMs = std::chrono::duration<double, std::milli> (Clock::now() - start).count();

        Assert::IsTrue (HasAllFacts (browser), L"every image has a file system");
    }


    TEST_METHOD (FiveHundredImages_AreListedWithBothColumns)
    {
        std::filesystem::path  root            = std::filesystem::temp_directory_path() / std::format (L"CassoPickerTiming.{}", GetCurrentProcessId());
        std::filesystem::path  images          = root / L"Images";
        std::filesystem::path  other           = root / L"Other";
        Win32FileSystem        fs;
        Win32DiskFileIo        io;
        double                 firstListMs     = 0;
        double                 firstColumnsMs  = 0;
        double                 returnListMs    = 0;
        double                 returnColumnsMs = 0;
        std::wstring           report;
        std::error_code        ec;



        std::filesystem::create_directories (images);
        std::filesystem::create_directories (other);
        WriteImages (images);

        {
            CassoExplorerBrowser  browser { fs, io };



            List (browser, images, firstListMs, firstColumnsMs);

            browser.NavigateToLocation (Location::MakeHostFolder (other.wstring()));

            List (browser, images, returnListMs, returnColumnsMs);
        }

        report = std::format (L"{} images. First visit: list {:.0f} ms, both columns {:.0f} ms. Return visit: list {:.0f} ms, both columns {:.0f} ms.\n",
                              kImages, firstListMs, firstColumnsMs, returnListMs, returnColumnsMs);

        Logger::WriteMessage (report.c_str());
        std::ofstream (std::filesystem::temp_directory_path() / L"CassoPickerTiming.txt") << TextEncoding::WideToUtf8 (report);

        std::filesystem::remove_all (root, ec);
    }
};
