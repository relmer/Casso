#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FileMapTestImages.h"
#include "CassoExplorer/Model/DiskThumbnailWorker.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailTests
//
//  The preview's small platter and chips (FR-072, SC-009): the rings and
//  chips of an image, no thumbnail for one that does not open, the note for
//  a 3.5" WOZ, only the newest selection's thumbnail taken, and a 140 KB
//  image's thumbnail built within 300 ms in Release.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskThumbnailTests)
{
public:

    static constexpr int  kWaitMs = 5000;



    //  A builder that blocks on the request for `heldPath` until LetGo, and
    //  counts the notifications.
    struct HeldBuilder
    {
        std::mutex               lock;
        std::condition_variable  changed;
        std::wstring             heldPath;
        bool                     isHeld    = false;
        bool                     isLetGo   = false;
        int                      notified  = 0;

        DiskThumbnailResult Build (const std::wstring & path)
        {
            DiskThumbnailResult           result;
            std::unique_lock<std::mutex>  guard (lock);

            if (path == heldPath)
            {
                isHeld = true;
                changed.notify_all();
                changed.wait_for (guard, std::chrono::milliseconds (kWaitMs), [this] () { return isLetGo; });
            }

            result.path     = path;
            result.isOpened = true;
            return result;
        }

        void Notify()
        {
            std::lock_guard<std::mutex>  guard (lock);

            notified++;
            changed.notify_all();
        }

        //  True once `test` is true, false after kWaitMs.
        bool WaitFor (const std::function<bool()> & test)
        {
            std::unique_lock<std::mutex>  guard (lock);

            return changed.wait_for (guard, std::chrono::milliseconds (kWaitMs), test);
        }

        void LetGo()
        {
            std::lock_guard<std::mutex>  guard (lock);

            isLetGo = true;
            changed.notify_all();
        }
    };



    TEST_METHOD (ADiskImageGivesItsRingsAndChips)
    {
        DiskThumbnailResult  result = DiskThumbnailWorker::BuildFromBytes (FileMapTestImages::MakeDos33(), L"C:\\Disks\\master.dsk");



        Assert::IsTrue  (result.isOpened);
        Assert::IsFalse (result.is35);
        Assert::IsNotNull (result.analysis.get());
        Assert::IsFalse (result.chips.empty());
        Assert::AreEqual (static_cast<int> (SummaryChipKind::Format), static_cast<int> (result.chips[0].kind), L"the format chip first, as in the status band");
        Assert::IsTrue  (std::any_of (result.levels.begin(), result.levels.end(), [] (const auto & levels) { return levels != nullptr; }), L"each record's ring levels");
    }



    TEST_METHOD (AnImageThatDoesNotOpenGetsNoThumbnail)
    {
        DiskThumbnailResult  result = DiskThumbnailWorker::BuildFromBytes (vector<Byte> (300, 0x42), L"C:\\Disks\\junk.woz");



        Assert::IsFalse (result.isOpened, L"only the error is in the preview");
        Assert::IsTrue  (result.chips.empty());
    }



    TEST_METHOD (A35InchWozGetsTheNote)
    {
        static constexpr size_t  kBitCount = 51200;

        vector<Byte>         image;
        WozSyntheticTrack    track;
        DiskThumbnailResult  result;
        size_t               info  = 0;



        track.data.assign ((kBitCount + 7) / 8, 0xFF);
        track.bitCount      = kBitCount;
        track.quarterTracks = { 0 };
        AssertSucceeded (WozLoader::BuildSyntheticV21 ({ track }, image));

        info = WozLoader::kHeaderSize;

        while (info + 4 <= image.size() && memcmp (image.data() + info, "INFO", 4) != 0)
        {
            info++;
        }

        image[info + 8 + WozLoader::kInfoOffsetDiskType] = WozLoader::kDiskType35;
        std::fill (image.begin() + 8, image.begin() + 12, Byte (0));
        result = DiskThumbnailWorker::BuildFromBytes (image, L"C:\\Disks\\mac.woz");

        Assert::IsTrue (result.isOpened);
        Assert::IsTrue (result.is35, L"the note in place of the platter and chips");
        Assert::IsTrue (result.chips.empty());
    }



    TEST_METHOD (OnlyTheNewestSelectionsThumbnailIsTaken)
    {
        HeldBuilder          builder;
        DiskThumbnailResult  result;



        builder.heldPath = L"A.dsk";

        {
            DiskThumbnailWorker  worker ([&builder] () { builder.Notify(); }, [&builder] (const std::wstring & path) { return builder.Build (path); });

            (void) worker.Request (L"A.dsk");
            Assert::IsTrue (builder.WaitFor ([&builder] () { return builder.isHeld; }), L"A is being built");

            (void) worker.Request (L"B.dsk");
            builder.LetGo();
            Assert::IsTrue (builder.WaitFor ([&builder] () { return builder.notified > 0; }));

            Assert::IsTrue  (worker.TakeResult (result));
            Assert::AreEqual (std::wstring (L"B.dsk"), result.path, L"A's thumbnail was dropped when it finished");
            Assert::AreEqual (1, builder.notified);
            Assert::IsFalse (worker.TakeResult (result), L"each result is taken once");
        }
    }



    TEST_METHOD (ASelectionThatLeavesTheImagesDropsItsThumbnail)
    {
        HeldBuilder          builder;
        DiskThumbnailResult  result;



        builder.heldPath = L"A.dsk";

        {
            DiskThumbnailWorker  worker ([&builder] () { builder.Notify(); }, [&builder] (const std::wstring & path) { return builder.Build (path); });

            (void) worker.Request (L"A.dsk");
            Assert::IsTrue (builder.WaitFor ([&builder] () { return builder.isHeld; }));

            worker.Cancel();
            builder.LetGo();
        }

        Assert::AreEqual (0, builder.notified);
    }



    //  SC-009: the thumbnail of a 140 KB image is built within 300 ms.
    TEST_METHOD (A140KbThumbnailIsBuiltWithin300Ms)
    {
#ifdef _DEBUG
        Logger::WriteMessage (L"SC-009 is measured in Release only");
#else
        vector<Byte>         image     = FileMapTestImages::MakeDos33();
        LARGE_INTEGER        frequency = {};
        LARGE_INTEGER        start     = {};
        LARGE_INTEGER        end       = {};
        double               ms        = 0;
        DiskThumbnailResult  result;



        QueryPerformanceFrequency (&frequency);
        QueryPerformanceCounter   (&start);
        result = DiskThumbnailWorker::BuildFromBytes (image, L"C:\\Disks\\master.dsk");
        QueryPerformanceCounter   (&end);
        ms = 1000.0 * static_cast<double> (end.QuadPart - start.QuadPart) / static_cast<double> (frequency.QuadPart);

        Logger::WriteMessage (std::format (L"140 KB thumbnail took {:.1f} ms", ms).c_str());
        Assert::IsTrue (result.isOpened);
        Assert::IsTrue (ms < 300.0);
#endif
    }
};
