#include "Pch.h"

#include "CassoExplorer/Model/RemovableMediaWatch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RemovableMediaWatch::RemovableMediaWatch
//
////////////////////////////////////////////////////////////////////////////////

RemovableMediaWatch::RemovableMediaWatch (ProbeFn probe, DrivesFn drives) :
    m_probe  (std::move (probe)),
    m_drives (std::move (drives))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemovableMediaWatch::~RemovableMediaWatch
//
////////////////////////////////////////////////////////////////////////////////

RemovableMediaWatch::~RemovableMediaWatch()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemovableMediaWatch::Start
//
////////////////////////////////////////////////////////////////////////////////

void RemovableMediaWatch::Start (DWORD intervalMs)
{
    if (m_worker.joinable())
    {
        return;
    }

    m_intervalMs = intervalMs;
    m_worker     = std::jthread ([this] (std::stop_token stop) { Run (stop); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemovableMediaWatch::Stop
//
//  Waits for a check in progress to finish.
//
////////////////////////////////////////////////////////////////////////////////

void RemovableMediaWatch::Stop()
{
    if (!m_worker.joinable())
    {
        return;
    }

    m_worker.request_stop();
    m_wake.notify_all();
    m_worker.join();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemovableMediaWatch::Kick
//
//  Looks again now rather than at the end of the interval.
//
////////////////////////////////////////////////////////////////////////////////

void RemovableMediaWatch::Kick()
{
    {
        std::lock_guard  guard (m_lock);

        m_kicked = true;
    }

    m_wake.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemovableMediaWatch::GetMask
//
//  The drives of the mask with media in them at the last look. A drive
//  added since then, and every drive before the first look, counts as
//  having media.
//
////////////////////////////////////////////////////////////////////////////////

DWORD RemovableMediaWatch::GetMask (DWORD driveMask) const
{
    return m_hasLooked.load() ? (driveMask & m_mask.load()) : driveMask;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemovableMediaWatch::Probe
//
//  One bit per drive of the mask with media in it.
//
////////////////////////////////////////////////////////////////////////////////

DWORD RemovableMediaWatch::Probe (const ProbeFn & probe, DWORD driveMask)
{
    constexpr int  kDrives = 26;



    DWORD  media = 0;



    for (int drive = 0; drive < kDrives; drive++)
    {
        std::wstring  root = { (wchar_t) (L'A' + drive), L':', L'\\' };

        if ((driveMask & (1u << drive)) != 0 && probe (root))
        {
            media |= 1u << drive;
        }
    }

    return media;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemovableMediaWatch::Run
//
//  The worker: a look, then a wait for the interval, a kick or a stop. A
//  drive that isn't in the mask, such as one added during the look, counts
//  as having media until the next one.
//
////////////////////////////////////////////////////////////////////////////////

void RemovableMediaWatch::Run (std::stop_token stop)
{
    DWORD  drives = 0;



    while (!stop.stop_requested())
    {
        drives = m_drives();

        m_mask.store (Probe (m_probe, drives) | ~drives);
        m_hasLooked.store (true);

        WaitForNextLook (stop);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemovableMediaWatch::WaitForNextLook
//
////////////////////////////////////////////////////////////////////////////////

void RemovableMediaWatch::WaitForNextLook (std::stop_token stop)
{
    std::unique_lock  guard (m_lock);



    m_wake.wait_for (guard, stop, std::chrono::milliseconds (m_intervalMs), [this] { return m_kicked; });
    m_kicked = false;
}
