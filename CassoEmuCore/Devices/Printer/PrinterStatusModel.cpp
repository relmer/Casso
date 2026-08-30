#include "Pch.h"

#include "PrinterStatusModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterStatusModel::PrinterStatusModel
//
////////////////////////////////////////////////////////////////////////////////

PrinterStatusModel::PrinterStatusModel (const Config & cfg)
    : m_cfg (cfg)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterStatusModel::Update
//
//  Derives the printer's status LED from an activity counter and a clock.
//
//  Activity is inferred from a COUNTER CHANGING rather than from an event,
//  because the card has no "started printing" signal -- bytes simply arrive.
//  Receiving therefore means "the counter moved recently", which needs a
//  window: the LED would otherwise flicker off between every byte of a slow
//  print.
//
//  The FIRST sample only establishes a baseline. Without that, opening the
//  panel against an already-advanced counter would read as a burst of activity
//  just now and light the LED for a print that finished minutes ago.
//
//  Precedence is deliberate -- Error over Receiving over Pending over Idle --
//  so a failure is never hidden by traffic still arriving, and pending content
//  is never hidden by the machine being momentarily quiet.
//
//  Time is passed IN rather than read, so the model is deterministic under
//  test and has no clock of its own.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterStatusModel::Update (uint64_t activityCount, double nowMs, bool hasContent, bool hasError)
{
    bool   receiving = false;



    if (!m_primed)
    {
        // First sample establishes the baseline only -- an already-advanced
        // counter must not read as a burst of activity "just now".
        m_primed       = true;
        m_lastActivity = activityCount;
    }
    else if (activityCount != m_lastActivity)
    {
        m_lastActivity   = activityCount;
        m_lastActivityMs = nowMs;
        m_sawActivity    = true;
    }

    receiving = m_sawActivity && ((nowMs - m_lastActivityMs) < m_cfg.receivingWindowMs);

    m_status = hasError   ? PrinterStatus::Error
             : receiving  ? PrinterStatus::Receiving
             : hasContent ? PrinterStatus::Pending
                          : PrinterStatus::Idle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterStatusModel::GetStatus
//
////////////////////////////////////////////////////////////////////////////////

PrinterStatus PrinterStatusModel::GetStatus() const
{
    return m_status;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterStatusModel::Reset
//
////////////////////////////////////////////////////////////////////////////////

void PrinterStatusModel::Reset()
{
    m_lastActivity   = 0;
    m_lastActivityMs = 0.0;
    m_primed         = false;
    m_sawActivity    = false;
    m_status         = PrinterStatus::Idle;
}
