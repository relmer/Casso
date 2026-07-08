#include "Pch.h"

#include "Print/PrinterWorker.h"

#include "Devices/Printer/PrinterByteRing.h"




static constexpr int   s_kIdleSleepMs = 4;   // nap only when the ring was empty




////////////////////////////////////////////////////////////////////////////////
//
//  ~PrinterWorker
//
////////////////////////////////////////////////////////////////////////////////

PrinterWorker::~PrinterWorker ()
{
    Stop ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  OpenCaptureFile
//
//  TEMPORARY (T011): tees the raw card stream to a fixed temp file so a real
//  Print Shop print can be captured and used to pin the interpreter's
//  provisional command bytes. Remove once that is done.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::OpenCaptureFile ()
{
    wchar_t   tempDir[MAX_PATH] = {};
    DWORD     count             = GetTempPathW (MAX_PATH, tempDir);

    if (count == 0 || count > MAX_PATH)
    {
        return;
    }

    fs::path   path = fs::path (tempDir) / L"casso-printer-capture.bin";
    m_capture.open (path, std::ios::binary | std::ios::trunc);
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
        Stop ();
    }

    m_job = make_unique<PrinterJob> (ring);

    // Restore a persisted pending strip before the thread starts, so new
    // strikes continue on the restored paper at its saved feed position.
    if (seed.RowsUsed () > 0)
    {
        m_job->Raster () = std::move (seed);
    }

    // Reflect any restored strip so the indicator shows Pending immediately.
    m_hasContent.store (m_job->HasContent (), std::memory_order_relaxed);

    OpenCaptureFile ();

    if (m_capture.is_open ())
    {
        m_job->SetByteObserver ([this] (const Byte * data, size_t size)
        {
            m_capture.write ((const char *) data, (streamsize) size);
            m_capture.flush ();
        });
    }

    m_stopRequested = false;
    m_running       = true;
    m_thread        = std::thread ([this] { Run (); });
}




////////////////////////////////////////////////////////////////////////////////
//
//  Stop
//
//  Signals and joins the drain thread. The job (and its raster) remain valid
//  afterwards so the UI thread can render/persist the completed strip.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::Stop ()
{
    if (!m_running)
    {
        return;
    }

    m_stopRequested = true;

    if (m_thread.joinable ())
    {
        m_thread.join ();
    }

    m_running = false;

    if (m_capture.is_open ())
    {
        m_capture.close ();
    }
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
//  Run
//
//  Drain loop: pull whatever the guest has emitted, then nap briefly only when
//  the ring was empty so a busy print stays responsive without spinning.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::Run ()
{
    vector<PrinterEvent>   events;

    while (!m_stopRequested)
    {
        size_t   drained = m_job->Drain (events);

        events.clear ();   // presentation events unused until the panel/audio land

        if (drained > 0)
        {
            // Publish activity + content for the UI-thread status sampler.
            m_activity.fetch_add (drained, std::memory_order_relaxed);
            m_hasContent.store   (m_job->HasContent (), std::memory_order_relaxed);
        }
        else
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (s_kIdleSleepMs));
        }
    }
}
