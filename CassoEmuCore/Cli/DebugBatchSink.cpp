#include "Pch.h"

#include "Cli/DebugBatchSink.h"

#include "Debugger/AppleWinFormatter.h"
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
    m_lastStop = stop.reason;
    m_pending += m_json ? ReplyJson::WriteStopped (stop, std::nullopt) : AppleWinFormatter::FormatStop (stop);
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
