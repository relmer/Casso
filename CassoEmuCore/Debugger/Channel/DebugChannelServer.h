#pragma once

#include "Debugger/Channel/IDebugCommandRunner.h"
#include "Debugger/Channel/IPipeTransport.h"
#include "Debugger/IDebugNotificationSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugChannelServer
//
//  The debug channel: accepts clients, carries out one request at a time in
//  the order they arrived, answers each to whoever asked, and sends every
//  notification to everyone.
//
//  ONE AT A TIME IN ARRIVAL ORDER is the whole concurrency story, and it is a
//  property of pumping rather than of locking: Pump reads one line, carries it
//  out, and only then reads the next. Several clients cannot interleave halfway
//  through a command because nothing is ever halfway through one.
//
//  NOTIFICATIONS ARRIVE FROM ANOTHER THREAD. The session raises them on the CPU
//  thread while this pumps on its own, so they are queued under a lock and
//  written out from the pump. That is the only shared state here, which is why
//  it is the only lock.
//
//  A STOP CAUSED BY A COMMAND NAMES THAT COMMAND. A client that asked for a run
//  can tell its own outcome from one another client started, which it could not
//  do from a notification everybody receives.
//
////////////////////////////////////////////////////////////////////////////////

class DebugChannelServer : public IDebugNotificationSink
{
public:
    DebugChannelServer (IPipeTransport & transport, IDebugCommandRunner & runner);

    DebugChannelServer             (const DebugChannelServer &) = delete;
    DebugChannelServer & operator= (const DebugChannelServer &) = delete;

    HRESULT  Open   ();

    //  Accepts whoever arrived, carries out what they sent, and flushes the
    //  notifications either produced. Never blocks.
    void     Pump   ();

    //  Tells every client the channel is going before it goes.
    void     Close  ();

    bool     IsOpen () const { return m_isOpen; }

    //  IDebugNotificationSink, called from the CPU thread.
    void  OnStopped        (const StopEvent & stop) override;
    void  OnResumed        () override;
    void  OnReset          (bool isPowerCycle) override;
    void  OnMachineChanged (const std::string & machineName) override;
    void  OnModeChanged    (CommandMode mode) override;

private:
    void  HandleLine         (ChannelConnectionId connection, const std::string & line);
    void  HandleRequest      (ChannelConnectionId connection, const ChannelRequest & request);
    void  Queue              (std::string && record);
    void  FlushNotifications ();
    void  Broadcast          (const std::string & record);

    IPipeTransport            & m_transport;
    IDebugCommandRunner       & m_runner;
    std::atomic<bool>           m_isOpen  = false;

    //  The command whose run is still outstanding, so its stop can name it.
    //  Read and written under the lock, because the stop comes from the CPU
    //  thread and the command was taken on this one.
    std::optional<int64_t>      m_causeId;

    mutable std::mutex          m_lock;
    std::vector<std::string>    m_pending;
};
