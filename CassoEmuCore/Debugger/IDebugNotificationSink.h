#pragma once

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDebugNotificationSink
//
//  Where the session sends the events no command asked for: a run stopping,
//  the machine resuming, a reset, a machine switch, a mode switch. Batch mode
//  prints them; the debug channel sends them to every client.
//
////////////////////////////////////////////////////////////////////////////////

class IDebugNotificationSink
{
public:
    virtual ~IDebugNotificationSink() = default;

    virtual void  OnStopped        (const StopEvent & stop)          = 0;
    virtual void  OnResumed        ()                                = 0;
    virtual void  OnReset          (bool isPowerCycle)               = 0;
    virtual void  OnMachineChanged (const std::string & machineName) = 0;
    virtual void  OnModeChanged    (CommandMode mode)                = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  NullDebugNotificationSink
//
//  For a session no one listens to.
//
////////////////////////////////////////////////////////////////////////////////

class NullDebugNotificationSink : public IDebugNotificationSink
{
public:
    void  OnStopped        (const StopEvent &) override          {}
    void  OnResumed        () override                           {}
    void  OnReset          (bool) override                       {}
    void  OnMachineChanged (const std::string &) override        {}
    void  OnModeChanged    (CommandMode) override                {}
};
