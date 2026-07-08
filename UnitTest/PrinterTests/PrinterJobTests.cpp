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
        for (i = 0; i < bytes.size (); i++)
        {
            Assert::IsTrue (ring.TryPush (bytes[i]));
        }
    }




    TEST_CLASS (PrinterJobTests)
    {
    public:

        TEST_METHOD (DrainFeedsRasterFromRing)
        {
            PrinterByteRing        ring;
            PrinterJob             job (ring);
            vector<PrinterEvent>   events;

            // ESC G 0001 then one top-pin column.
            PushAll (ring, { 0x1B, 'G', '0', '0', '0', '1', 0x80 });

            size_t   drained = job.Drain (events);

            Assert::AreEqual ((size_t) 7, drained);
            Assert::AreEqual ((Byte) InkPrimary::Black, job.Raster ().CellAt (0, 0));
            Assert::IsTrue (job.HasContent ());
        }


        TEST_METHOD (EmptyRingDrainsNothing)
        {
            PrinterByteRing        ring;
            PrinterJob             job (ring);
            vector<PrinterEvent>   events;

            Assert::AreEqual ((size_t) 0, job.Drain (events));
            Assert::IsFalse (job.HasContent ());
        }


        TEST_METHOD (ObserverSeesEveryDrainedByte)
        {
            PrinterByteRing        ring;
            PrinterJob             job (ring);
            vector<PrinterEvent>   events;
            vector<Byte>           seen;

            job.SetByteObserver ([&] (const Byte * p, size_t n)
            {
                seen.insert (seen.end (), p, p + n);
            });

            vector<Byte>   stream = { 0x1B, 'T', '1', '2', 0x0A, 0x0C };
            PushAll (ring, stream);
            job.Drain (events);

            Assert::AreEqual (stream.size (), seen.size ());
            for (size_t i = 0; i < stream.size (); i++)
            {
                Assert::AreEqual (stream[i], seen[i]);
            }
        }


        TEST_METHOD (CommandSplitAcrossDrainsCompletes)
        {
            PrinterByteRing        ring;
            PrinterJob             job (ring);
            vector<PrinterEvent>   events;

            // First drain gets the graphics header but no data byte yet.
            PushAll (ring, { 0x1B, 'G', '0', '0', '0', '1' });
            job.Drain (events);
            Assert::IsFalse (job.HasContent ());          // nothing struck yet

            // Second drain delivers the data byte; the run completes.
            PushAll (ring, { 0x80 });
            job.Drain (events);
            Assert::AreEqual ((Byte) InkPrimary::Black, job.Raster ().CellAt (0, 0));
        }


        TEST_METHOD (ResetClearsStrip)
        {
            PrinterByteRing        ring;
            PrinterJob             job (ring);
            vector<PrinterEvent>   events;

            PushAll (ring, { 0x1B, 'G', '0', '0', '0', '1', 0x80, 0x0A });
            job.Drain (events);
            Assert::IsTrue (job.HasContent ());

            job.Reset ();

            Assert::IsFalse (job.HasContent ());
            Assert::AreEqual (0, job.Raster ().RowsUsed ());
            Assert::AreEqual (0, job.Raster ().PaperRow ());
        }
    };
}
