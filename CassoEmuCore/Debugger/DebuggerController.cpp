#include "Pch.h"

#include "Debugger/DebuggerController.h"

#include "Shell/CpuManager.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController::DebuggerController
//
//  The session starts in whatever state the machine is in. A debugger opened
//  on a game already playing must not stop it, which is why `--debugger` opens
//  at machine start without pausing.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerController::DebuggerController (MachineHost & host, CpuManager & cpuManager, IPipeTransport & transport,
                                        IFileSystem & files, InstanceDescriber describe, uint32_t processId) :
    m_host       (host),
    m_cpuManager (cpuManager),
    m_describe   (std::move (describe)),
    m_processId  (processId),
    m_target     (host),
    m_driver     (host, cpuManager, m_target.GetRunHook()),
    m_server     (transport, *this),
    m_session    (m_target, m_server, cpuManager.IsPaused() ? RunState::Paused : RunState::FreeRunning)
{
    m_target.SetRunDriver (&m_driver);
    m_session.SetFileSystem (&files);
    m_handlers.Attach (m_session);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController::~DebuggerController
//
////////////////////////////////////////////////////////////////////////////////

DebuggerController::~DebuggerController()
{
    Close();
    m_target.SetRunDriver (nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebuggerController::Open()
{
    return m_server.Open();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController::Close
//
//  Closes the channel only. See the class comment for why the session, its
//  breakpoints and the machine's pause state are left alone.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerController::Close()
{
    m_server.Close();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController::Pump
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerController::Pump()
{
    m_server.Pump();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController::RunLine
//
//  One request's line. A budget the request carried applies to a run this
//  line starts and to nothing after it, so the session's own budget is put
//  back once the line is done; a mode it named likewise applies to this line
//  only and never switches the mode other clients are using.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerController::RunLine (const std::string          & line,
                                   std::optional<CommandMode>   mode,
                                   std::optional<uint64_t>      budget)
{
    std::optional<uint64_t>  previousBudget = m_session.GetBudget();
    CommandMode              lineMode       = mode.value_or (m_session.GetMode());
    Reply                    reply;



    if (budget.has_value())
    {
        m_session.SetBudget (*budget == 0 ? std::optional<uint64_t>() : budget);
    }

    reply = m_session.ExecuteLine (line, lineMode);
    m_session.FormatReply (reply, lineMode);

    if (budget.has_value())
    {
        m_session.SetBudget (previousBudget);
    }

    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController::RequestPause
//
//  During a run, the driver ends it at the next slice and the stop carries the
//  run's cycle count. With no run the machine is simply stopped where it is,
//  and the session announces that like any other stop.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerController::RequestPause()
{
    if (m_driver.IsRunning())
    {
        m_driver.Pause();
        return;
    }

    m_cpuManager.SetPaused (true);
    m_session.OnUserPaused();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController::GetInstance
//
////////////////////////////////////////////////////////////////////////////////

ChannelHello DebuggerController::GetInstance() const
{
    ChannelHello      hello;
    DebugMachineInfo  info = m_target.GetMachineInfo();



    //  An empty drive is reported as null rather than as an empty name, so a
    //  client can tell "no disk" from a disk it cannot name.
    for (const std::string & disk : info.disks)
    {
        hello.disks.push_back (disk.empty() ? std::optional<std::string>() : std::optional<std::string> (disk));
    }

    hello.protocol = ChannelProtocol::kProtocolVersion;
    hello.pid      = m_processId;
    hello.mode     = m_session.GetMode();
    hello.isPaused = m_cpuManager.IsPaused();

    if (m_describe)
    {
        m_describe (hello);
    }

    return hello;
}
