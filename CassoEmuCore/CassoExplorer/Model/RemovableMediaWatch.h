#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RemovableMediaWatch
//
//  Which drives have media in them, checked on a worker thread. The check
//  sends each drive a device request, and a card reader or a locked volume
//  can take half a second or more to answer it, so the check never runs on
//  the window's thread. The worker checks again every interval, or at once
//  after Kick. Until the first check ends, every drive counts as having
//  media.
//
////////////////////////////////////////////////////////////////////////////////

class RemovableMediaWatch
{
public:
    using ProbeFn  = std::function<bool (const std::wstring & root)>;
    using DrivesFn = std::function<DWORD()>;

    RemovableMediaWatch (ProbeFn probe, DrivesFn drives);
    ~RemovableMediaWatch();

    void   Start     (DWORD intervalMs);
    void   Stop      ();
    void   Kick      ();
    DWORD  GetMask   (DWORD driveMask) const;
    bool   HasLooked () const { return m_hasLooked.load(); }

    static DWORD  Probe (const ProbeFn & probe, DWORD driveMask);

private:
    void  Run             (std::stop_token stop);
    void  WaitForNextLook (std::stop_token stop);

    ProbeFn                       m_probe;
    DrivesFn                      m_drives;
    DWORD                         m_intervalMs = 0;
    std::atomic<DWORD>            m_mask       = 0;
    std::atomic<bool>             m_hasLooked  = false;
    std::mutex                    m_lock;
    std::condition_variable_any   m_wake;
    bool                          m_kicked     = false;
    std::jthread                  m_worker;
};
