#include "Pch.h"

#include "Debugger/DebuggerController.h"

#include "Core/TextEncoding.h"
#include "Debugger/Channel/GuardedFileSystem.h"
#include "Debugger/Channel/HostAccessGuard.h"
#include "Debugger/Channel/HostAccessPolicy.h"
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
    m_files      (files),
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
//  The call stack is recorded from here on: the debugger is attached.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebuggerController::Open()
{
    HRESULT  hr = S_OK;



    hr = m_server.Open();
    CHR (hr);

    m_session.SetCallRecording (true);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController::Close
//
//  Closes the channel only. See the class comment for why the session, its
//  breakpoints and the machine's pause state are left alone. The call-stack
//  record is not: it is kept only while the debugger is attached (FR-064).
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerController::Close()
{
    m_server.Close();
    m_session.SetCallRecording (false);
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
//  The line's host file access goes through the client's policy: the
//  session's file system is wrapped for the line, and the session has the
//  guard, so a command that accesses a host file another way checks it too.
//  The console lines for the access are sent once the line is done.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerController::RunLine (const std::string          & line,
                                   std::optional<CommandMode>   mode,
                                   std::optional<uint64_t>      budget,
                                   const ChannelClient        & client,
                                   bool                         overwrite)
{
    HostAccessPolicy           policy (*m_resolver, GetAllowedFolders (client));
    HostAccessGuard            guard  (policy, client.name, client.allowScripts, overwrite);
    GuardedFileSystem          guarded (m_files, guard);
    std::vector<std::string>   lines;
    Reply                      reply;



    m_session.SetFileSystem      (&guarded);
    m_session.SetHostAccessGuard (&guard);

    reply = RunGuardedLine (line, mode, budget);

    m_session.SetHostAccessGuard (nullptr);
    m_session.SetFileSystem      (&m_files);

    lines = guard.TakeConsoleLines();

    if (!lines.empty() && m_consoleSink)
    {
        m_consoleSink (lines);
    }

    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController::RunGuardedLine
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerController::RunGuardedLine (const std::string          & line,
                                          std::optional<CommandMode>   mode,
                                          std::optional<uint64_t>      budget)
{
    std::optional<uint64_t>  previousBudget = m_session.GetBudget();
    CommandMode              lineMode       = mode.value_or (m_session.GetMode());
    Reply                    reply;



    m_linesRun++;

    //  A blank line matters only inside an A block, where it ends the block.
    //  Elsewhere it is nothing, as it is in a batch script.
    if (line.find_first_not_of (" \t\r") == std::string::npos && !m_session.IsAssembling())
    {
        reply.command = line;
        return reply;
    }

    if (budget.has_value())
    {
        m_session.SetBudget (*budget == 0 ? std::optional<uint64_t>() : budget);
    }

    reply = m_session.ExecuteLine (line, lineMode);
    m_session.FormatReply (reply, lineMode);

    //  The request's budget is for this line alone, unless the line set a
    //  budget of its own, which then stands.
    if (budget.has_value() && m_session.GetBudget() == (*budget == 0 ? std::optional<uint64_t>() : budget))
    {
        m_session.SetBudget (previousBudget);
    }

    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController::RequestPause
//
//  A running machine, in a debugger run or free running, is stopped by the
//  driver at the point in the frame that matches when the pause was asked for,
//  and the driver announces the stop. A machine already paused is stopped where
//  it is, and the session announces that like any other stop.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerController::RequestPause()
{
    if (m_driver.IsRunning() || !m_cpuManager.IsPaused())
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





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController::GetAllowedFolders
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DebuggerController::GetAllowedFolders (const ChannelClient & client) const
{
    std::vector<std::wstring>  folders = client.allowPaths;
    DebugMachineInfo           info    = m_target.GetMachineInfo();
    const DebugSession       & session = m_session;



    for (const std::string & disk : info.disks)
    {
        if (!disk.empty())
        {
            folders.push_back (GetFolder (TextEncoding::NarrowToWide (disk)));
        }
    }

    if (session.HasDebugFile())
    {
        folders.push_back (GetFolder (session.GetDebugFilePath()));
    }

    for (const DebugSession::SymbolLoad & load : session.GetSymbolLoads())
    {
        folders.push_back (GetFolder (load.path));
    }

    std::erase_if (folders, [] (const std::wstring & folder) { return folder.empty(); });
    return folders;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerController::GetFolder
//
//  The folder a file is in; empty for a name with no folder.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerController::GetFolder (const std::wstring & path)
{
    size_t  slash = path.find_last_of (L"\\/");



    return (slash == std::wstring::npos) ? std::wstring() : path.substr (0, slash);
}
