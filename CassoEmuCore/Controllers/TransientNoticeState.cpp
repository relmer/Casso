#include "Pch.h"

#include "Controllers/TransientNoticeState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
//  A second notice replaces the first rather than queueing behind it: what
//  just happened is what the user is looking for, and a queue would show
//  stale text while the reason for it had already scrolled past.
//
////////////////////////////////////////////////////////////////////////////////

void TransientNoticeState::Show (const std::wstring & text, int64_t nowMs, int64_t durationMs)
{
    m_text    = text;
    m_untilMs = nowMs + durationMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
////////////////////////////////////////////////////////////////////////////////

void TransientNoticeState::Clear()
{
    m_text.clear();
    m_untilMs = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsShowing
//
//  Empty text never shows, whatever the expiry says, so a notice cleared by
//  something else does not come back as an empty band.
//
////////////////////////////////////////////////////////////////////////////////

bool TransientNoticeState::IsShowing (int64_t nowMs) const
{
    return !m_text.empty() && nowMs < m_untilMs;
}
