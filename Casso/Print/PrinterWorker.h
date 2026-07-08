#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterJob.h"

class PrinterByteRing;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterWorker
//
//  The exe-side platform edge for the printer drain: a background thread that
//  paces PrinterJob::Drain so the card's ring never overflows and the strip
//  builds up as the guest prints. All real work lives in the core PrinterJob;
//  this owns only the thread and its lifecycle (irreducible platform code per
//  Constitution VI).
//
//  Threading: the drain thread is the sole writer of the raster while running.
//  The UI thread must call Stop() (which joins) before it touches the raster
//  for eject/persistence; FlushNow() then drains any final bytes single-
//  threaded.
//
//  A temporary raw-byte capture tee (to %TEMP%\casso-printer-capture.bin) is
//  wired here to grab a real Print Shop stream for the T011 checkpoint; remove
//  it once the interpreter's provisional command bytes are pinned.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterWorker
{
public:
    ~PrinterWorker ();

    // `seed` restores a persisted pending strip into the job before the drain
    // thread starts (no race); default-empty seed starts a fresh sheet.
    void          Start    (PrinterByteRing & ring, PrintRaster seed = PrintRaster ());
    void          Stop     ();
    bool          Running  () const { return m_running; }

    // Valid from Start() until the next Start()/dtor. Touch the raster only
    // after Stop().
    PrinterJob *  Job      () { return m_job.get (); }

    // Synchronous drain of any bytes still in the ring; call only after Stop().
    size_t        FlushNow (vector<PrinterEvent> & events);

    // Thread-safe status signals for the chrome indicator / panel: a monotonic
    // counter that advances as guest bytes are drained (sample the delta to see
    // "receiving"), and whether the strip currently holds printed content. Safe
    // to read from the UI thread while the drain thread runs.
    uint64_t      ActivityCount () const { return m_activity.load    (std::memory_order_relaxed); }
    bool          HasContent    () const { return m_hasContent.load (std::memory_order_relaxed); }

private:
    void          Run             ();
    void          OpenCaptureFile ();

    unique_ptr<PrinterJob>   m_job;
    std::thread              m_thread;
    std::atomic<bool>        m_stopRequested { false };
    std::atomic<uint64_t>    m_activity      { 0 };
    std::atomic<bool>        m_hasContent    { false };
    bool                     m_running       = false;
    std::ofstream            m_capture;
};
