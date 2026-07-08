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
//  PrinterStatusModel::Status
//
////////////////////////////////////////////////////////////////////////////////

PrinterStatus PrinterStatusModel::Status () const
{
    return m_status;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterStatusModel::Reset
//
////////////////////////////////////////////////////////////////////////////////

void PrinterStatusModel::Reset ()
{
    m_lastActivity   = 0;
    m_lastActivityMs = 0.0;
    m_primed         = false;
    m_sawActivity    = false;
    m_status         = PrinterStatus::Idle;
}
