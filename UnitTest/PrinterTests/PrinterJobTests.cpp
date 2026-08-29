#include "Pch.h"

#include "Devices/Printer/PrinterJob.h"
#include "Devices/Printer/PrinterByteRing.h"
#include "Devices/Printer/PrintRaster.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterJobTests
//
//  The consumer-side drain: ring bytes flow into the raster, the observer tap
//  sees them, drains accumulate interpreter state across calls, and reset
//  clears the strip.
//
////////////////////////////////////////////////////////////////////////////////

namespace PrinterJobTests
{
    static void PushAll (PrinterByteRing & ring, const vector<Byte> & bytes)
    {
        size_t   i = 0;
        for (auto & byte : bytes)
        {
            Assert::IsTrue (ring.TryPush (byte));
        }
    }




    TEST_CLASS (PrinterJobTests)
    {
    public:

        TEST_METHOD (DrainFeedsRasterFromRing)
        {
            auto                  ring    = std::make_unique<PrinterByteRing> ();   // heap: 64KB (C6262)
            vector<PrinterEvent>  events;
            size_t                drained = 0;
            PrinterJob             job (*ring);

            // ESC G 0001 then one top-pin column (0x01 = LSB = top pin).
            PushAll (*ring, { 0x1B, 'G', '0', '0', '0', '1', 0x01 });

            drained = job.Drain (events);

            Assert::AreEqual ((size_t) 7, drained);
            Assert::AreEqual ((Byte) InkPrimary::Black, job.GetRaster().GetCell (0, 0));
            Assert::IsTrue (job.HasContent());
        }


        TEST_METHOD (EmptyRingDrainsNothing)
        {
            auto                   ring = std::make_unique<PrinterByteRing> ();   // heap: 64KB (C6262)
            vector<PrinterEvent>   events;
            PrinterJob             job (*ring);

            Assert::AreEqual ((size_t) 0, job.Drain (events));
            Assert::IsFalse (job.HasContent());
        }


        TEST_METHOD (HostFormFeedAdvancesToNextPageTop)
        {
            vector<PrinterEvent>  events;
            int                   before = 0;



            // The preview's Form Feed button: identical to the guest sending
            // $0C, through the same interpreter path.
            auto                   ring = std::make_unique<PrinterByteRing> ();   // heap: 64KB (C6262)
            PrinterJob             job (*ring);

            PushAll (*ring, { 0x1B, 'G', '0', '0', '0', '1', 0x01, 0x0A });
            job.Drain (events);

            before = job.GetHeadRow();

            job.FormFeed (events);

            Assert::AreEqual (PrinterGrid::kPageRows, job.GetHeadRow());
            Assert::IsTrue (job.GetHeadRow() > before);
            Assert::AreEqual (1, (int) std::count_if (events.begin(), events.end(),
                [] (const PrinterEvent & e) { return e.type == PrinterEventType::FormFeed; }));

            // A second feed lands on the following page top.
            job.FormFeed (events);
            Assert::AreEqual (2 * PrinterGrid::kPageRows, job.GetHeadRow());
        }


        TEST_METHOD (ObserverSeesEveryDrainedByte)
        {
            auto                  ring   = std::make_unique<PrinterByteRing> ();   // heap: 64KB (C6262)
            vector<PrinterEvent>  events;
            vector<Byte>          seen;
            vector<Byte>          stream;
            PrinterJob             job (*ring);

            job.SetByteObserver ([&] (const Byte * p, size_t n)
            {
                seen.insert (seen.end(), p, p + n);
            });

            stream = { 0x1B, 'T', '1', '2', 0x0A, 0x0C };
            PushAll (*ring, stream);
            job.Drain (events);

            Assert::AreEqual (stream.size(), seen.size());
            for (size_t i = 0; i < stream.size(); i++)
            {
                Assert::AreEqual (stream[i], seen[i]);
            }
        }


        TEST_METHOD (CommandSplitAcrossDrainsCompletes)
        {
            auto                   ring = std::make_unique<PrinterByteRing> ();   // heap: 64KB (C6262)
            vector<PrinterEvent>   events;
            PrinterJob             job (*ring);

            // First drain gets the graphics header but no data byte yet.
            PushAll (*ring, { 0x1B, 'G', '0', '0', '0', '1' });
            job.Drain (events);
            Assert::IsFalse (job.HasContent());          // nothing struck yet

            // Second drain delivers the data byte; the run completes.
            PushAll (*ring, { 0x01 });
            job.Drain (events);
            Assert::AreEqual ((Byte) InkPrimary::Black, job.GetRaster().GetCell (0, 0));
        }


        TEST_METHOD (ResetClearsStrip)
        {
            auto                   ring = std::make_unique<PrinterByteRing> ();   // heap: 64KB (C6262)
            vector<PrinterEvent>   events;
            PrinterJob             job (*ring);

            PushAll (*ring, { 0x1B, 'G', '0', '0', '0', '1', 0x80, 0x0A });
            job.Drain (events);
            Assert::IsTrue (job.HasContent());

            job.Reset();

            Assert::IsFalse (job.HasContent());
            Assert::AreEqual (0, job.GetRaster().GetRowsUsed());
            Assert::AreEqual (0, job.GetRaster().GetPaperRow());
        }
    };
}
