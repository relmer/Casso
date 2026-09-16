#pragma once

#include "Debugger/Channel/DebugChannelServer.h"
#include "Debugger/Channel/IDebugCommandRunner.h"
#include "Debugger/CpuManagerRunDriver.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/DebugSession.h"
#include "Debugger/MachineDebugTarget.h"

class CpuManager;
class IPipeTransport;
class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController
//
//  The debugger inside a running emulator: one session over the machine, the
//  run driver that executes its runs on the CPU thread, and the channel server
//  clients reach it through.
//
//  OPENING AND CLOSING ARE ABOUT THE CHANNEL, NOT THE SESSION. Open starts the
//  server; Close sends `closing`, drops every client and stops listening. The
//  session outlives both, so breakpoints a client set stay armed and a paused
//  machine stays paused -- a debugger closed to free the pipe is not a request
//  to forget where the user was, and reopening finds everything as it was left.
//
//  Everything here runs on the CPU thread. Pump is called once per frame, and
//  a command carried out from it runs where the machine is safe to touch, so
//  neither the session nor the driver needs a lock.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerController : public IDebugCommandRunner
{
public:
    //  Fills in what the handshake reports about this instance that the
    //  controller cannot know itself: the window title, the machine and the
    //  disks. The rest -- process id, protocol, mode, paused -- it supplies.
    using InstanceDescriber = std::function<void (ChannelHello & hello)>;

    DebuggerController (MachineHost & host, CpuManager & cpuManager, IPipeTransport & transport,
                        InstanceDescriber describe, uint32_t processId);
    ~DebuggerController() override;

    DebuggerController             (const DebuggerController &) = delete;
    DebuggerController & operator= (const DebuggerController &) = delete;

    HRESULT  Open    ();
    void     Close   ();
    bool     IsOpen  () const { return m_server.IsOpen(); }

    //  Once per frame, on the CPU thread. A closed controller does nothing.
    void     Pump    ();

    DebugSession         & GetSession   () { return m_session; }
    CpuManagerRunDriver  & GetRunDriver () { return m_driver; }

    // IDebugCommandRunner
    Reply         RunLine      (const std::string          & line,
                                std::optional<CommandMode>   mode,
                                std::optional<uint64_t>      budget) override;
    void          RequestPause () override;
    ChannelHello  GetInstance  () const override;

private:
    MachineHost          & m_host;
    CpuManager           & m_cpuManager;
    InstanceDescriber      m_describe;
    uint32_t               m_processId = 0;

    //  Declared in construction order: the session attaches itself to the
    //  target, and the server is the session's notification sink.
    MachineDebugTarget     m_target;
    CpuManagerRunDriver    m_driver;
    DebugChannelServer     m_server;
    DebugSession           m_session;
    DebugHandlerSet        m_handlers;
};
