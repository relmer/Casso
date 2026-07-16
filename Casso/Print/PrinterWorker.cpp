#include "Pch.h"

#include "Print/PrinterWorker.h"

#include "Devices/Printer/PrinterByteRing.h"




static constexpr int   s_kIdleSleepMs = 4;   // nap only when the ring was empty




////////////////////////////////////////////////////////////////////////////////
//
//  ~PrinterWorker
//
////////////////////////////////////////////////////////////////////////////////

PrinterWorker::~PrinterWorker()
{
    Stop();
}




////////////////////////////////////////////////////////////////////////////////
//
//  Start
//
//  Builds a fresh job over the card's ring and spawns the drain thread. A
//  running worker is stopped first so Start is idempotent across machine
//  rebuilds.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::Start (PrinterByteRing & ring, PrintRaster seed)
{
    if (m_running)
    {
        Stop();
    }

    m_job = make_unique<PrinterJob> (ring);

    // Restore a persisted pending strip before the thread starts, so new
    // strikes continue on the restored paper at its saved feed position.
    if (seed.RowsUsed() > 0)
    {
        m_job->Raster() = std::move (seed);
    }

    // Reflect any restored strip so the indicator shows Pending immediately.
    m_hasContent.store (m_job->HasContent(), std::memory_order_relaxed);
    m_rowsUsed.store   (m_job->Raster().RowsUsed(), std::memory_order_relaxed);
    m_headPos.store    (((uint64_t) (uint32_t) m_job->HeadRow() << 32)
                        | (uint32_t) m_job->HeadColumnDots(), std::memory_order_relaxed);

    m_stopRequested = false;
    m_running       = true;
    m_thread        = std::thread ([this] { Run(); });
}




////////////////////////////////////////////////////////////////////////////////
//
//  Stop
//
//  Signals and joins the drain thread. The job (and its raster) remain valid
//  afterwards so the UI thread can render/persist the completed strip.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::Stop()
{
    if (!m_running)
    {
        return;
    }

    m_stopRequested = true;

    if (m_thread.joinable())
    {
        m_thread.join();
    }

    m_running = false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  FlushNow
//
////////////////////////////////////////////////////////////////////////////////

size_t PrinterWorker::FlushNow (vector<PrinterEvent> & events)
{
    if (m_job == nullptr)
    {
        return 0;
    }

    return m_job->Drain (events);
}




////////////////////////////////////////////////////////////////////////////////
//
//  SnapshotStrip
//
//  Copies the strip under the raster lock so the live preview reads a consistent
//  image while the drain thread keeps running -- no Stop()/Start() and no new
//  interpreter, so the guest's in-flight state (line feed, colour, head column)
//  is never reset out from under it.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterWorker::SnapshotStrip (PrintRaster & out)
{
    std::lock_guard<std::mutex>   lock (m_rasterMutex);

    if (m_job == nullptr)
    {
        return false;
    }

    out = m_job->Raster();   // copy under lock
    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SnapshotStripSpan
//
//  Viewport-bounded variant of SnapshotStrip: copies only the requested rows
//  under the raster lock, so the live preview's per-refresh cost stays flat
//  no matter how long the banner grows.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterWorker::SnapshotStripSpan (int firstRow, int lastRow, PrintRaster & out)
{
    std::lock_guard<std::mutex>   lock (m_rasterMutex);

    if (m_job == nullptr)
    {
        return false;
    }

    m_job->Raster().CopyRowSpan (firstRow, lastRow, out);
    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  FormFeed
//
//  Host-initiated page advance through the job's own interpreter path, then
//  the same signal publishing a drain performs -- the panel's viewport and
//  pacing follow the new head position and animate the feed at printer
//  speed. Activity is deliberately NOT bumped: it tracks GUEST bytes (the
//  idle signal that gates this very button and the auto-open logic).
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::FormFeed()
{
    std::lock_guard<std::mutex>   lock (m_rasterMutex);
    vector<PrinterEvent>          events;

    if (m_job == nullptr)
    {
        return;
    }

    m_job->FormFeed (events);

    m_hasContent.store (m_job->HasContent(), std::memory_order_relaxed);
    m_rowsUsed.store   (m_job->Raster().RowsUsed(), std::memory_order_relaxed);
    m_headPos.store    (((uint64_t) (uint32_t) m_job->HeadRow() << 32)
                        | (uint32_t) m_job->HeadColumnDots(), std::memory_order_relaxed);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Run
//
//  Drain loop: pull whatever the guest has emitted, then nap briefly only when
//  the ring was empty so a busy print stays responsive without spinning.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::Run()
{
    vector<PrinterEvent>   events;

    while (!m_stopRequested)
    {
        size_t     drained = 0;
        int        rowsNow = 0;
        uint64_t   headPos = 0;

        {
            // Hold the raster lock only across the mutation so a UI-thread
            // SnapshotStrip sees a consistent strip; the idle nap stays outside.
            std::lock_guard<std::mutex>   lock (m_rasterMutex);
            drained = m_job->Drain (events);
            rowsNow = m_job->Raster().RowsUsed();
            headPos = ((uint64_t) (uint32_t) m_job->HeadRow() << 32)
                      | (uint32_t) m_job->HeadColumnDots();
        }

        events.clear();   // presentation events unused until the panel/audio land

        if (drained > 0)
        {
            // Publish activity + content + strip height + head position for the
            // UI-thread status sampler and the live-preview reveal (FR-034).
            m_activity.fetch_add (drained, std::memory_order_relaxed);
            m_hasContent.store   (m_job->HasContent(), std::memory_order_relaxed);
            m_rowsUsed.store     (rowsNow, std::memory_order_relaxed);
            m_headPos.store      (headPos, std::memory_order_relaxed);
        }
        else
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (s_kIdleSleepMs));
        }
    }
}
