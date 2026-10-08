#include "Pch.h"

#include "Debugger/CapturedDebugTarget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CapturedDebugTarget
//
////////////////////////////////////////////////////////////////////////////////

CapturedDebugTarget::CapturedDebugTarget (std::shared_ptr<const DebugViewCapture> capture) :
    m_capture (std::move (capture))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CapturedDebugTarget::TryPeek
//
//  A byte the machine's peek would not have read, I/O among them, reads as
//  zero and says so.
//
////////////////////////////////////////////////////////////////////////////////

bool CapturedDebugTarget::TryPeek (Word address, Byte & value) const
{
    bool  isReadable = m_capture->memory.readable[address];



    value = isReadable ? m_capture->memory.bytes[address] : (Byte) 0;
    return isReadable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CapturedDebugTarget::GetRegion
//
////////////////////////////////////////////////////////////////////////////////

MemoryRegion CapturedDebugTarget::GetRegion (Word address) const
{
    return m_capture->memory.regions[address / DebugMemoryImage::kPageBytes];
}





////////////////////////////////////////////////////////////////////////////////
//
//  CapturedDebugTarget::TryGetLastBranch
//
////////////////////////////////////////////////////////////////////////////////

bool CapturedDebugTarget::TryGetLastBranch (Word & from) const
{
    from = m_capture->lastBranch.value_or (0);
    return m_capture->lastBranch.has_value();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CapturedDebugTarget::GetTraceWindow
//
//  The part of the asked-for window that the capture holds. The capture
//  holds the window the trace pane showed when it was taken, so a request
//  outside it comes back short.
//
////////////////////////////////////////////////////////////////////////////////

void CapturedDebugTarget::GetTraceWindow (
    size_t                       first,
    size_t                       count,
    std::vector<TraceRecord>   & entries) const
{
    const DebugViewCapture  & capture = *m_capture;
    size_t                    held    = capture.traceFirst + capture.trace.size();
    size_t                    end     = (std::min) (first + count, held);



    entries.clear();

    for (size_t index = (std::max) (first, capture.traceFirst); index < end; index++)
    {
        entries.push_back (capture.trace[index - capture.traceFirst]);
    }
}
