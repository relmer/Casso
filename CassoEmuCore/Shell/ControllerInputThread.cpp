#include "Pch.h"

#include "Shell/ControllerInputThread.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ~ControllerInputThread
//
////////////////////////////////////////////////////////////////////////////////

ControllerInputThread::~ControllerInputThread()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Start
//
//  The backend is initialized ON the new thread, not here: it creates the
//  window the device notifications arrive at, and that window belongs to the
//  thread that pumps it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ControllerInputThread::Start (
    IControllerBackend        * pBackend,
    IControllerBackendEvents  * pEvents,
    TickFn                      tick)
{
    HRESULT  hr        = S_OK;
    bool     isRunning = m_thread.joinable();



    CBRAEx (pBackend != nullptr && pEvents != nullptr && tick, E_INVALIDARG);
    CBRA   (!isRunning);

    m_wakeEvent = CreateEventW (nullptr, FALSE, FALSE, nullptr);
    CWRA (m_wakeEvent);

    m_backend  = pBackend;
    m_events   = pEvents;
    m_tick     = std::move (tick);
    m_stopping = false;
    m_thread   = std::thread ([this] { Run(); });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Stop
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputThread::Stop()
{
    m_stopping = true;

    Wake();

    if (m_thread.joinable())
    {
        m_thread.join();
    }

    if (m_wakeEvent != nullptr)
    {
        CloseHandle (m_wakeEvent);
        m_wakeEvent = nullptr;
    }

    m_backend = nullptr;
    m_events  = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Wake
//
//  Brings the thread out of its wait early, for a selection change that makes
//  a different controller the one being read.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputThread::Wake()
{
    if (m_wakeEvent != nullptr)
    {
        SetEvent (m_wakeEvent);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Run
//
//  Controller thread. COM is initialized for DirectInput's internals; an
//  apartment already chosen by something else is not an error here, since
//  nothing on this thread depends on which one it is.
//
//  The wait covers three things at once: the backend's change events, the
//  wake event, and the window messages the device notifications arrive as.
//  MsgWaitForMultipleObjectsEx is what lets one wait serve all three.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputThread::Run()
{
    HRESULT              hr           = S_OK;
    HRESULT              hrCom        = S_OK;
    std::vector<HANDLE>  waitHandles;
    std::optional<DWORD> timeout;



    hrCom = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
    IGNORE_RETURN_VALUE (hrCom, S_OK);

    hr = m_backend->Initialize (m_events);
    IGNORE_RETURN_VALUE (hr, S_OK);

    while (!m_stopping)
    {
        MSG    message      = {};
        DWORD  waitResult   = WAIT_FAILED;
        DWORD  waitDuration = INFINITE;

        timeout = m_tick();

        if (m_stopping)
        {
            break;
        }

        waitHandles.clear();
        waitHandles.push_back (m_wakeEvent);

        // The tick decided which controller is being read, so the backend's
        // wake sources for it are current as of now.
        if (timeout.has_value())
        {
            waitDuration = timeout.value();
        }

        waitResult = MsgWaitForMultipleObjectsEx ((DWORD) waitHandles.size(), waitHandles.data(),
                                                  waitDuration, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        IGNORE_RETURN_VALUE (waitResult, WAIT_FAILED);

        while (PeekMessageW (&message, nullptr, 0, 0, PM_REMOVE))
        {
            DispatchMessageW (&message);
        }
    }

    m_backend->Shutdown();

    if (SUCCEEDED (hrCom))
    {
        CoUninitialize();
    }
}
