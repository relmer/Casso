#include "Pch.h"

#include "Cli/DebugBatchSink.h"

#include "Debugger/AppleWinFormatter.h"
#include "Debugger/MonitorFormatter.h"
#include "Debugger/ReplyJson.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchSink::TakePending
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugBatchSink::TakePending()
{
    std::string  taken = std::move (m_pending);



    m_pending.clear();
    return taken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchSink::OnStopped
//
////////////////////////////////////////////////////////////////////////////////

void DebugBatchSink::OnStopped (const StopEvent & stop)
{
    bool  isMonitor = m_session != nullptr && m_session->GetMode() == CommandMode::Monitor;



    m_lastStop = stop.reason;

    //  A STOP IS PRINTED IN THE MODE THE READER IS WORKING IN. At a `*`
    //  prompt a step shows the Monitor's register line, which is what the
    //  original ][ printed; `Step at $0302` is the AppleWin wording and
    //  belongs to the AppleWin prompt. The JSON form carries the stop as
    //  data and does not vary by mode at all.
    if (m_json)
    {
        m_pending += ReplyJson::WriteStopped (stop, std::nullopt);
    }
    else
    {
        m_pending += isMonitor ? MonitorFormatter::FormatStop (stop) : AppleWinFormatter::FormatStop (stop);
    }

    m_pending += "\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchSink::OnResumed
//
////////////////////////////////////////////////////////////////////////////////

void DebugBatchSink::OnResumed()
{
    if (IsInfoShown())
    {
        m_pending += m_json ? ReplyJson::WriteResumed() : std::string ("Resumed");
        m_pending += "\n";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchSink::OnReset
//
////////////////////////////////////////////////////////////////////////////////

void DebugBatchSink::OnReset (bool isPowerCycle)
{
    if (IsInfoShown())
    {
        m_pending += m_json ? ReplyJson::WriteReset (isPowerCycle) : std::string (isPowerCycle ? "Power cycle" : "Reset");
        m_pending += "\n";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchSink::OnMachineChanged
//
////////////////////////////////////////////////////////////////////////////////

void DebugBatchSink::OnMachineChanged (const std::string & machineName)
{
    if (IsInfoShown())
    {
        m_pending += m_json ? ReplyJson::WriteMachineChanged (machineName) : "Machine: " + machineName;
        m_pending += "\n";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchSink::OnModeChanged
//
//  Only as JSON. In text the MODE command's own reply names the new mode
//  on the line above, and a reader of a transcript does not need it twice.
//
////////////////////////////////////////////////////////////////////////////////

void DebugBatchSink::OnModeChanged (CommandMode mode)
{
    if (m_json && IsInfoShown())
    {
        m_pending += ReplyJson::WriteModeChanged (mode);
        m_pending += "\n";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchSink::IsInfoShown
//
////////////////////////////////////////////////////////////////////////////////

bool DebugBatchSink::IsInfoShown() const
{
    return m_session == nullptr || m_session->GetLogLevel() != LogLevel::Error;
}
