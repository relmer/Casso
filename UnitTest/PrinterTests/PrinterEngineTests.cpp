#include "Pch.h"

#include "Devices/Printer/PrinterEngine.h"
#include "Devices/Printer/PrinterByteRing.h"
#include "Devices/Printer/PrintRaster.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterEngineTests
//
//  The whole drain -> pace -> publish pipeline, driven with a SYNTHETIC clock and
//  a pre-loaded ring -- no thread, no real time. This is what used to be trapped
//  in the exe-side worker's Run loop: the backpressure gate, the wall-clock-vs-
//  guest-cycle pacing cap, host form feed replay, and the published preview state.
//
////////////////////////////////////////////////////////////////////////////////

namespace PrinterEngineTests
{
    static void Push (PrinterByteRing & ring, const vector<Byte> & bytes)
    {
        size_t   i = 0;

        for (auto & byte : bytes)
        {
            Assert::IsTrue (ring.TryPush (byte), L"ring push failed (test overran the ring)");
        }
    }


    // ESC G <4-digit count> + `cols` all-pin columns + CR + LF: one full graphics
    // band. A full-width line (cols = kDotsPerRow) is ~0.32 s of carriage.
    static void PushGraphicsLine (PrinterByteRing & ring, int cols)
    {
        vector<Byte>   data;
        int            i = 0;

        Push (ring, { 0x1B, 'G',
                      (Byte) ('0' + (cols / 1000) % 10),
                      (Byte) ('0' + (cols / 100) % 10),
                      (Byte) ('0' + (cols / 10) % 10),
                      (Byte) ('0' + cols % 10) });

        for (i = 0; i < cols; i++)
        {
            data.push_back (0xFF);
        }

        Push (ring, data);
        Push (ring, { 0x0D, 0x0A });
    }


    static void TickFor (PrinterEngine & engine, int64_t & nowMs, int ticks, int64_t stepMs)
    {
        int   i = 0;

        for (i = 0; i < ticks; i++)
        {
            nowMs += stepMs;
            engine.Tick (nowMs);
        }
    }




    TEST_CLASS (PrinterEngineTests)
    {
    public:

        TEST_METHOD (StartPublishesIdleState)
        {
            auto            ring   = std::make_unique<PrinterByteRing> ();   // heap: 64KB (C6262)
            PrinterEngine   engine;

            engine.Start (*ring);

            Assert::IsFalse  (engine.HasContent(),   L"fresh sheet has no content");
            Assert::AreEqual (0, engine.RowsUsed(),  L"fresh sheet has no rows");
            Assert::AreEqual ((uint64_t) 0, engine.ActivityCount(), L"no bytes drained yet");
            Assert::IsFalse  (engine.HeadMoving(),   L"head is parked");
            Assert::AreEqual (0, engine.CarriageCol(), L"carriage at the left margin");
        }


        TEST_METHOD (TickDrainsRingAndBuildsStrip)
        {
            auto            ring   = std::make_unique<PrinterByteRing> ();   // heap: 64KB (C6262)
            PrinterEngine   engine;
            int64_t         nowMs  = 1000;

            engine.Start (*ring);
            PushGraphicsLine (*ring, 20);

            TickFor (engine, nowMs, 4, 100);   // ~0.3 s of print time

            Assert::IsTrue   (engine.ActivityCount() > 0, L"Tick drained guest bytes");
            Assert::IsTrue   (engine.HasContent(),        L"the strip now holds content");
            Assert::IsTrue   (engine.RowsUsed() > 0,      L"the interpreter built rows");
            Assert::AreEqual ((Byte) InkPrimary::Black, engine.Job()->Raster().CellAt (0, 0),
                              L"the drained graphics landed on the strip");
        }


        TEST_METHOD (RunningGuestSweepsThenParks)
        {
            auto            ring       = std::make_unique<PrinterByteRing> ();   // heap: 64KB (C6262)
            PrinterEngine   engine;
            int64_t         nowMs      = 1000;
            int             peakCarr   = 0;
            int             i          = 0;

            engine.Start (*ring);                    // no cycle clock -> pace off the wall alone
            PushGraphicsLine (*ring, PrinterGrid::kDotsPerRow);   // full-width line

            for (i = 0; i < 60; i++)
            {
                nowMs += 100;
                engine.Tick (nowMs);
                peakCarr = (std::max) (peakCarr, engine.CarriageCol());
            }

            Assert::IsTrue  (peakCarr > 1000, L"the carriage swept most of a full-width line");
            Assert::IsFalse (engine.HeadMoving(), L"the head parks once the line is printed");
        }


        TEST_METHOD (PausedGuestFreezesHeadButStillBuilds)
        {
            auto            ring     = std::make_unique<PrinterByteRing> ();   // heap: 64KB (C6262)
            PrinterEngine   engine;
            int64_t         nowMs    = 1000;
            uint64_t        cycles   = 1000;   // never advances: the guest is paused

            engine.Start (*ring);
            engine.SetCycleClock (&cycles);
            PushGraphicsLine (*ring, PrinterGrid::kDotsPerRow);

            TickFor (engine, nowMs, 30, 100);   // 3 s of WALL time, but 0 guest cycles

            // The cycle cap pins the advance to 0, so the head is frozen even though
            // wall time marched on -- pausing the emulator freezes the printer.
            Assert::AreEqual (0, engine.CarriageCol(), L"a paused guest freezes the carriage");

            // But the interpreter still drained the pre-buffered bytes into the strip.
            Assert::IsTrue (engine.RowsUsed() > 0, L"the strip still built from the buffered bytes");
            Assert::IsTrue (engine.HeadMoving(),   L"motion is queued, just not advancing");
        }


        TEST_METHOD (BackpressureHoldsRingWhenBufferFull)
        {
            auto            ring   = std::make_unique<PrinterByteRing> ();   // heap: 64KB (C6262)
            PrinterEngine   engine;
            int             line   = 0;

            engine.Start (*ring);

            // Far more than one line buffer's worth of print (each full-width line
            // is ~0.32 s; the buffer holds ~0.75 s).
            for (line = 0; line < 10; line++)
            {
                PushGraphicsLine (*ring, PrinterGrid::kDotsPerRow);
            }

            engine.Tick (1000);   // a single step: drains only up to the buffer, then stops

            Assert::IsTrue (engine.Job()->Pending() > 0,
                            L"backpressure leaves the rest queued in the ring");
        }


        TEST_METHOD (HostFormFeedSlewsPlaten)
        {
            auto            ring     = std::make_unique<PrinterByteRing> ();   // heap: 64KB (C6262)
            PrinterEngine   engine;
            int64_t         nowMs    = 1000;
            int             row      = 0;
            int             col      = 0;

            engine.Start (*ring);
            engine.FormFeed();                 // host Form Feed button

            TickFor (engine, nowMs, 60, 100);  // slew the whole page in

            engine.HeadPosition (row, col);
            Assert::IsTrue  (row > 500, L"the host form feed slews the platen up the page");
            Assert::IsFalse (engine.HeadMoving(), L"the feed finishes and the head parks");
        }


        TEST_METHOD (SnapshotAndInkExtentReadThroughToRaster)
        {
            auto            ring   = std::make_unique<PrinterByteRing> ();   // heap: 64KB (C6262)
            PrinterEngine   engine;
            PrintRaster     strip;
            int64_t         nowMs  = 1000;

            engine.Start (*ring);
            PushGraphicsLine (*ring, 40);
            TickFor (engine, nowMs, 4, 100);

            Assert::IsTrue (engine.SnapshotStrip (strip), L"snapshot succeeds with an active job");
            Assert::IsTrue (strip.RowsUsed() > 0,         L"the snapshot carries the printed rows");
            Assert::IsTrue (engine.SpanInkExtent (0, PrinterGrid::kPinBandRows - 1) > 0,
                            L"the live pin band reports ink for the audio gate");
        }
    };
}
