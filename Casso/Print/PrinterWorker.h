#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterEngine.h"

class PrinterByteRing;





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterWorker
//
//  The exe-side platform edge for the printer: the background thread that paces
//  the drain. It owns ONLY the thread and its lifecycle -- read the wall clock,
//  call PrinterEngine::Tick, sleep, repeat -- and forwards every other call to
//  the engine. All the real work (drain, pacing, head mechanics, preview state)
//  lives in the core PrinterEngine, which the UnitTest project drives directly
//  with a synthetic clock (Constitution VI: thin executable, testable core).
//
//  Threading: the drain thread is the sole writer of the raster while running.
//  The UI thread must call Stop() (which joins) before it touches the raster for
//  eject/persistence; FlushNow() then drains any final bytes single-threaded.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterWorker
{
public:
    ~PrinterWorker ();

    // `seed` restores a persisted pending strip into the engine before the drain
    // thread starts (no race); default-empty seed starts a fresh sheet.
    void          Start    (PrinterByteRing & ring, PrintRaster seed = PrintRaster ());
    void          Stop     ();
    bool          Running  () const { return m_running; }

    // Wire the guest's monotonic cycle accumulator so the drain runs at real
    // ImageWriter speed in EMULATED time (backpressure). Set once at machine
    // build (the pointer is stable across worker restarts); null drains
    // unthrottled. See PrinterEngine::SetCycleClock.
    void          SetCycleClock (const uint64_t * cycles) { m_engine.SetCycleClock (cycles); }

    // Valid from Start() until the next Start()/dtor. Touch the raster only after
    // Stop().
    PrinterJob *  Job      () { return m_engine.Job(); }

    // Synchronous drain of any bytes still in the ring; call only after Stop().
    size_t        FlushNow (vector<PrinterEvent> & events) { return m_engine.FlushNow (events); }

    // Consistent preview copies taken while the drain thread keeps running (the
    // engine's raster mutex makes them safe from the UI thread). See PrinterEngine.
    bool          TrySnapshotStrip         (PrintRaster & out)                       { return m_engine.TrySnapshotStrip (out); }
    bool          TrySnapshotStripSpan     (int firstRow, int lastRow, PrintRaster & out) { return m_engine.TrySnapshotStripSpan (firstRow, lastRow, out); }
    bool          TrySnapshotPresentedSpan (int firstRow, int lastRow, PrintRaster & out) { return m_engine.TrySnapshotPresentedSpan (firstRow, lastRow, out); }

    // One past the rightmost inked dot over the live pin band (0 == blank); drives
    // the preview's audio ink gate. Safe from the UI thread.
    int           SpanInkExtent (int firstRow, int lastRow) { return m_engine.SpanInkExtent (firstRow, lastRow); }

    // Host form feed (the preview's Form Feed button): the next drain tick slews
    // the page in with feed sound. Safe to call from the UI thread.
    void          FormFeed () { m_engine.FormFeed(); }

    // Thread-safe status signals for the chrome indicator / panel, forwarded from
    // the engine's lock-free published state. Safe to read from the UI thread
    // while the drain thread runs.
    uint64_t      ActivityCount () const { return m_engine.ActivityCount(); }
    bool          HasContent    () const { return m_engine.HasContent(); }
    int           RowsUsed      () const { return m_engine.RowsUsed(); }
    void          HeadPosition  (int & row, int & colDots) const { m_engine.HeadPosition (row, colDots); }
    bool          HeadSweepLtr  () const { return m_engine.HeadSweepLtr(); }
    int           RevealBandTop () const { return m_engine.RevealBandTop(); }
    bool          HeadMoving    () const { return m_engine.HeadMoving(); }
    int           CarriageCol   () const { return m_engine.CarriageCol(); }

private:
    void          Run ();

    PrinterEngine        m_engine;                     // all drain / pacing / preview logic (core, tested)
    std::thread          m_thread;
    std::atomic<bool>    m_stopRequested { false };
    bool                 m_running       = false;
};
