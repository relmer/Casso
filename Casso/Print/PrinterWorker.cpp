#include "Pch.h"

#include "Print/PrinterWorker.h"

#include "Devices/Printer/PrinterByteRing.h"




static constexpr int   s_kIdleSleepMs = 4;   // loop period: paces the head in ~4 ms steps





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
//  Hands the ring (and any restored strip) to the engine, then spawns the drain
//  thread. A running worker is stopped first so Start is idempotent across
//  machine rebuilds.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::Start (PrinterByteRing & ring, PrintRaster seed)
{
    if (m_running)
    {
        Stop();
    }

    m_engine.Start (ring, std::move (seed));

    m_stopRequested = false;
    m_running       = true;
    m_thread        = std::thread ([this] { Run(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Stop
//
//  Signals and joins the drain thread. The engine (and its raster) remain valid
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
//  Run
//
//  The platform edge: each ~4 ms it reads the wall clock and drives one engine
//  Tick, which does all the drain / pacing / preview work. Nothing else lives
//  here.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::Run()
{
    while (!m_stopRequested)
    {
        int64_t   nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                              std::chrono::steady_clock::now().time_since_epoch()).count();

        m_engine.Tick (nowMs);

        std::this_thread::sleep_for (std::chrono::milliseconds (s_kIdleSleepMs));
    }
}
