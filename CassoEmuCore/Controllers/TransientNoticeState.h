#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TransientNoticeState
//
//  What the window has to say about itself for a few seconds: a screenshot's
//  filename, which controller was just chosen. Text and an expiry, and the
//  rules about when it is showing -- no layout, no drawing, no clock of its
//  own, so the expiry can be asserted without waiting for it.
//
//  A NOTICE IS NEVER MODAL. These say something the user did not ask about,
//  so stopping them to acknowledge it would interrupt the machine to report
//  something they may not care about at all.
//
////////////////////////////////////////////////////////////////////////////////

class TransientNoticeState
{
public:

    // Long enough to read a filename, short enough not to sit on the picture.
    static constexpr int64_t  kDefaultDurationMs = 4000;

    void          Show      (const std::wstring & text, int64_t nowMs, int64_t durationMs = kDefaultDurationMs);
    void          Clear     ();
    bool          IsShowing (int64_t nowMs) const;

    std::wstring  GetText   () const { return m_text; }

private:

    std::wstring  m_text;
    int64_t       m_untilMs = 0;
};
