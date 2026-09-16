#pragma once

#include "Debugger/DebugSession.h"
#include "Debugger/IDebugNotificationSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchSink
//
//  Batch mode's notification sink. A stop always prints; the other
//  notifications print at LOG INFO and above, and a mode change only as
//  JSON. Lines are held until the command that caused them has printed its
//  reply, so a run's reply comes before its stop, as it does over the
//  channel.
//
////////////////////////////////////////////////////////////////////////////////

class DebugBatchSink : public IDebugNotificationSink
{
public:
    void  SetJson    (bool json)                    { m_json = json; }
    void  SetSession (const DebugSession * session) { m_session = session; }

    // The held lines, each ending in a newline, and then nothing held.
    std::string  TakePending ();

    std::optional<StopReason>  GetLastStopReason () const { return m_lastStop; }

    void  OnStopped        (const StopEvent & stop) override;
    void  OnResumed        () override;
    void  OnReset          (bool isPowerCycle) override;
    void  OnMachineChanged (const std::string & machineName) override;
    void  OnModeChanged    (CommandMode mode) override;

private:
    bool  IsInfoShown () const;

    bool                        m_json    = false;
    const DebugSession        * m_session = nullptr;
    std::string                 m_pending;
    std::optional<StopReason>   m_lastStop;
};
