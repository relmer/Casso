#pragma once

#include "Pch.h"

#include "Seams/IControllerBackend.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerInputThread
//
//  Owns the thread every controller is read on, and nothing else: the devices
//  belong to the backend, and what to do with a sample belongs to the tick
//  callback. The loop is this thin on purpose, because a thread is the one
//  part of this the unit tests cannot drive.
//
//  It waits on the backend's own change events, so a DirectInput device that
//  signals its state costs nothing between changes. An XInput controller has
//  no such event, so the wait takes a timeout while one is selected. With
//  nothing selected there is no timeout at all: the thread sleeps until a
//  device arrives or it is asked to stop.
//
////////////////////////////////////////////////////////////////////////////////

class ControllerInputThread
{
public:

    // Runs on the controller thread on every wake, and says what to wait on
    // before the next one: the devices' own change events, a timeout, or
    // neither.
    using TickFn = std::function<ControllerWaitSources()>;

    ~ControllerInputThread ();

    HRESULT  Start  (IControllerBackend * pBackend, IControllerBackendEvents * pEvents, TickFn tick);
    void     Stop   ();
    void     Wake   ();

private:

    void  Run ();

    IControllerBackend        * m_backend   = nullptr;
    IControllerBackendEvents  * m_events    = nullptr;
    TickFn                      m_tick;
    std::thread                 m_thread;
    HANDLE                      m_wakeEvent = nullptr;
    std::atomic<bool>           m_stopping  {false};
};
