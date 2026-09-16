#pragma once

#include "Debugger/Channel/ChannelProtocol.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDebugCommandRunner
//
//  Where a channel request goes to be carried out.
//
//  THE SERVER DOES NOT HOLD THE SESSION. In the emulator a command has to be
//  carried out on the CPU thread, and the server runs on its own; putting a
//  seam here keeps the thread hop on one side of it and the protocol on the
//  other. It is also what lets the server's whole behavior -- arrival order,
//  who each record is addressed to, what a close does -- be tested with no
//  machine at all.
//
//  A run command returns as soon as the run has STARTED. Its outcome arrives
//  later as a stopped notification, which is what the contract promises, so
//  nothing here blocks waiting for a machine.
//
////////////////////////////////////////////////////////////////////////////////

class IDebugCommandRunner
{
public:
    virtual ~IDebugCommandRunner() = default;

    //  One line in the mode the request chose, or the session's when it chose
    //  none, with a budget for a run this line starts. The reply comes back
    //  formatted, because which mode's text it carries is the session's to
    //  decide rather than the channel's.
    virtual Reply  RunLine      (const std::string          & line,
                                 std::optional<CommandMode>   mode,
                                 std::optional<uint64_t>      budget) = 0;

    //  Stops a running machine. The stop itself arrives as a notification.
    virtual void   RequestPause () = 0;

    //  Whether a run a command started is still going. A stop that arrives
    //  later names that command as its cause; once no run is going, a later
    //  stop -- the user pausing a free-running machine -- names none.
    virtual bool   IsRunInProgress () const = 0;

    //  What the handshake reports about this instance.
    virtual ChannelHello  GetInstance () const = 0;
};
