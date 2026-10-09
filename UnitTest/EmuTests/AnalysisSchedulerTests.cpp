#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Ui/DiskInspector/AnalysisScheduler.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AnalysisSchedulerTests
//
//  Background analysis per record (FR-021): every record of a disk arrives
//  once, the newest request for a record wins, and results for a disk no
//  longer shown are dropped.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AnalysisSchedulerTests)
{
public:

    static constexpr auto  kDeadline = std::chrono::seconds (10);



    static std::shared_ptr<const DiskCopy> MakeCopy (uint64_t mediaId, Byte fill)
    {
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, fill);
        DiskImage     image;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));

        return DiskCopy::MakeFromImage (image, mediaId, "test", 0, false);
    }



    //  Takes results until nothing is pending, or fails at the deadline.
    static void Drain (AnalysisScheduler & scheduler, vector<RecordResult> & outAll)
    {
        auto                  stop    = std::chrono::steady_clock::now() + kDeadline;
        vector<RecordResult>  batch;



        outAll.clear();

        while (scheduler.HasPending() && std::chrono::steady_clock::now() < stop)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (2));
            scheduler.TakeResults (batch);
            outAll.insert (outAll.end(), batch.begin(), batch.end());
        }

        scheduler.TakeResults (batch);
        outAll.insert (outAll.end(), batch.begin(), batch.end());

        Assert::IsFalse (scheduler.HasPending(), L"analysis finished before the deadline");
    }



    TEST_METHOD (EveryRecordArrivesOnceAndNotifies)
    {
        std::atomic<int>      notified = 0;
        AnalysisScheduler     scheduler ([&notified] () { notified++; });
        vector<RecordResult>  results;
        std::set<int>         slots;



        scheduler.Restart (MakeCopy (1, 0), DecodeSettings::MakeStandard());
        Drain (scheduler, results);

        for (const RecordResult & r : results)
        {
            slots.insert (r.slot);
        }

        Assert::AreEqual (static_cast<int> (results.size()), static_cast<int> (slots.size()), L"each record once");
        Assert::AreEqual (static_cast<int> (results.size()), notified.load());
        Assert::IsTrue (results[0].analysis != nullptr);
        Assert::AreEqual (16, results[0].analysis->sectorsGood);
    }



    TEST_METHOD (TheNewestRequestForARecordWins)
    {
        AnalysisScheduler                scheduler (nullptr);
        vector<RecordResult>             results;
        std::shared_ptr<const DiskCopy>  first  = MakeCopy (1, 0);
        std::shared_ptr<const DiskCopy>  second = MakeCopy (1, 0x55);
        const int                        slot   = 30;



        scheduler.Restart (first, DecodeSettings::MakeStandard());
        scheduler.Update  (second, std::span<const int> (&slot, 1));
        Drain (scheduler, results);

        for (const RecordResult & r : results)
        {
            Assert::IsFalse (r.slot == slot && r.copy == first, L"a replaced request's result is dropped");
        }

        Assert::AreEqual (1, static_cast<int> (std::count_if (results.begin(), results.end(), [slot] (const RecordResult & r) { return r.slot == slot; })));
    }



    TEST_METHOD (ResultsForAnotherDiskAreDropped)
    {
        AnalysisScheduler     scheduler (nullptr);
        vector<RecordResult>  results;



        scheduler.Restart (MakeCopy (1, 0), DecodeSettings::MakeStandard());
        scheduler.Restart (MakeCopy (2, 0), DecodeSettings::MakeStandard());
        scheduler.Update  (MakeCopy (1, 0), std::vector<int> { 3, 4 });
        Drain (scheduler, results);

        Assert::IsFalse (results.empty());

        for (const RecordResult & r : results)
        {
            Assert::AreEqual (2ull, r.mediaId);
        }
    }



    TEST_METHOD (ClosingMidwayDoesNotWaitForTheDisk)
    {
        auto  start = std::chrono::steady_clock::now();



        {
            AnalysisScheduler  scheduler (nullptr);

            scheduler.Restart (MakeCopy (1, 0), DecodeSettings::MakeStandard());
        }

        Assert::IsTrue (std::chrono::steady_clock::now() - start < kDeadline);
    }
};
