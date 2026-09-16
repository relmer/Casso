#include "Pch.h"

#include "Debugger/Channel/DebugChannelServer.h"

#include "Debugger/ReplyJson.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::DebugChannelServer
//
////////////////////////////////////////////////////////////////////////////////

DebugChannelServer::DebugChannelServer (IPipeTransport & transport, IDebugCommandRunner & runner) :
    m_transport (transport),
    m_runner    (runner)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugChannelServer::Open()
{
    HRESULT  hr = m_transport.Listen();



    m_isOpen = SUCCEEDED (hr);
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::Pump
//
//  Accept, then one line at a time, then whatever those lines raised.
//
//  THE LINE IS CARRIED OUT BEFORE THE NEXT IS READ. That is what makes
//  arrival order a promise rather than a hope, and it is why two clients
//  cannot interleave halfway through a command.
//
////////////////////////////////////////////////////////////////////////////////

void DebugChannelServer::Pump()
{
    ChannelConnectionId  connection = 0;
    std::string          line;



    if (!m_isOpen)
    {
        return;
    }

    while (m_transport.TryAccept (connection))
    {
        //  Nothing is sent on accept. A client that wants to know what it
        //  reached says hello, and one that does not may go straight to
        //  commands.
    }

    //  A stop raised since the last pump goes out before anything new is
    //  taken on, so a client sees its run end before the answer to whatever
    //  it sent next.
    FlushNotifications();

    while (m_transport.TryReadLine (connection, line))
    {
        HandleLine (connection, line);
        FlushNotifications();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::Close
//
//  `closing` is the last record a client receives, so a client can tell the
//  debugger being closed from the pipe breaking under it.
//
////////////////////////////////////////////////////////////////////////////////

void DebugChannelServer::Close()
{
    if (!m_isOpen)
    {
        return;
    }

    FlushNotifications();
    Broadcast (ReplyJson::WriteClosing());

    m_isOpen = false;
    m_transport.Close();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::HandleLine
//
//  A line that never became a request is answered to whoever sent it, and the
//  connection stays open: one client's malformed line is not everyone's
//  problem, and is not even that client's session.
//
////////////////////////////////////////////////////////////////////////////////

void DebugChannelServer::HandleLine (ChannelConnectionId connection, const std::string & line)
{
    ChannelRequest  request;
    ReplyError      error;



    if (!ChannelProtocol::TryParseRequest (line, request, error))
    {
        m_transport.WriteLine (connection, ChannelProtocol::WriteError (error.label, error.detail));
        return;
    }

    HandleRequest (connection, request);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::HandleRequest
//
////////////////////////////////////////////////////////////////////////////////

void DebugChannelServer::HandleRequest (ChannelConnectionId connection, const ChannelRequest & request)
{
    ChannelHello  hello;
    Reply         reply;
    bool          isRunning = false;



    switch (request.type)
    {
    case ChannelRequestType::Hello:
        hello    = m_runner.GetInstance();
        hello.id = request.id;
        m_transport.WriteLine (connection, ChannelProtocol::WriteHello (hello));
        return;

    case ChannelRequestType::Pause:
        //  Answered at once rather than when the machine stops. The stop is a
        //  notification, and a client that waited for this reply to mean
        //  "stopped" would wait through the whole of whatever was running.
        m_runner.RequestPause();
        reply.command = "pause";
        m_transport.WriteLine (connection, ReplyJson::WriteReply (reply, request.id));
        return;

    default:
        break;
    }

    //  Named before the line runs, because a synchronous machine can deliver
    //  the stop before RunLine returns.
    {
        std::lock_guard<std::mutex>  held (m_lock);

        m_causeId = request.id;
    }

    reply = m_runner.RunLine (request.line, request.mode, request.budget);

    //  A line that started no run, or whose run has already ended, leaves
    //  nothing for a later stop to be caused by. Without this a pause long
    //  after an ordinary `R` would be reported as that `R`'s outcome.
    isRunning = m_runner.IsRunInProgress();

    if (!isRunning)
    {
        std::lock_guard<std::mutex>  held (m_lock);

        m_causeId.reset();
    }

    m_transport.WriteLine (connection, ReplyJson::WriteReply (reply, request.id, isRunning));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::OnStopped
//
//  The one notification that names a cause: whichever command's run this
//  ended, if a command started it.
//
////////////////////////////////////////////////////////////////////////////////

void DebugChannelServer::OnStopped (const StopEvent & stop)
{
    std::optional<int64_t>  causeId;



    {
        std::lock_guard<std::mutex>  held (m_lock);

        causeId = m_causeId;
        m_causeId.reset();
    }

    Queue (ReplyJson::WriteStopped (stop, causeId));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::OnResumed
//
////////////////////////////////////////////////////////////////////////////////

void DebugChannelServer::OnResumed()
{
    Queue (ReplyJson::WriteResumed());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::OnReset
//
////////////////////////////////////////////////////////////////////////////////

void DebugChannelServer::OnReset (bool isPowerCycle)
{
    Queue (ReplyJson::WriteReset (isPowerCycle));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::OnMachineChanged
//
////////////////////////////////////////////////////////////////////////////////

void DebugChannelServer::OnMachineChanged (const std::string & machineName)
{
    Queue (ReplyJson::WriteMachineChanged (machineName));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::OnModeChanged
//
////////////////////////////////////////////////////////////////////////////////

void DebugChannelServer::OnModeChanged (CommandMode mode)
{
    Queue (ReplyJson::WriteModeChanged (mode));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::Queue
//
//  Built on the raising thread and written from the pump, so the transport is
//  only ever touched by one of them.
//
////////////////////////////////////////////////////////////////////////////////

void DebugChannelServer::Queue (std::string && record)
{
    std::lock_guard<std::mutex>  held (m_lock);



    m_pending.push_back (std::move (record));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::FlushNotifications
//
////////////////////////////////////////////////////////////////////////////////

void DebugChannelServer::FlushNotifications()
{
    std::vector<std::string>  records;



    {
        std::lock_guard<std::mutex>  held (m_lock);

        records.swap (m_pending);
    }

    for (const std::string & record : records)
    {
        Broadcast (record);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer::Broadcast
//
//  Every client, because a notification answers nobody in particular: a
//  breakpoint one client set stops the machine every client is watching.
//
////////////////////////////////////////////////////////////////////////////////

void DebugChannelServer::Broadcast (const std::string & record)
{
    for (ChannelConnectionId connection : m_transport.GetConnections())
    {
        m_transport.WriteLine (connection, record);
    }
}
