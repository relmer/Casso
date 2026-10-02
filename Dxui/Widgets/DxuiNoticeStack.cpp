#include "Pch.h"

#include "Widgets/DxuiNoticeStack.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::DxuiNoticeStack
//
//  Starts hidden: nothing has been reported yet.
//
////////////////////////////////////////////////////////////////////////////////

DxuiNoticeStack::DxuiNoticeStack()
{
    SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::Push
//
//  APPENDED, NOT REPLACED. The new notice goes below the last one showing and
//  gets the full duration of its own; nothing already up is shortened.
//
//  It appears in place, without a slide of its own. If the notice above it is
//  still sliding up, the new one takes the same slide, so the two move as one
//  rather than the upper one sliding over the lower.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiNoticeStack::Push (const std::wstring & text, int64_t nowMs)
{
    Notice  notice;



    notice.banner = std::make_unique<DxuiTimedInfoBanner>();
    notice.banner->SetParent     (this);
    notice.banner->SetDpi        (m_dpi);
    notice.banner->SetDurationMs (m_durationMs);
    notice.banner->Show          (text, nowMs);

    if (!m_notices.empty())
    {
        notice.slide = m_notices.back().slide;
    }

    m_notices.push_back (std::move (notice));
    m_nowMs = std::max (m_nowMs, nowMs);

    SetVisible (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::Clear
//
////////////////////////////////////////////////////////////////////////////////

void DxuiNoticeStack::Clear()
{
    m_notices.clear();

    SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::IsShowing
//
//  Whether any notice is inside its countdown at nowMs.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiNoticeStack::IsShowing (int64_t nowMs) const
{
    return std::any_of (m_notices.begin(), m_notices.end(),
                        [nowMs] (const Notice & n) { return n.banner->IsShowing (nowMs); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::IsAnimating
//
//  Whether any notice is still sliding into its place at nowMs.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiNoticeStack::IsAnimating (int64_t nowMs) const
{
    return std::any_of (m_notices.begin(), m_notices.end(),
                        [nowMs] (const Notice & n) { return !n.slide.IsDone (nowMs); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::GetNextChangeMs
//
//  The nearest of every notice's expiry and every running slide's end, so a
//  caller that parks between frames knows when to come back. A slide is
//  judged running at the last tick.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int64_t> DxuiNoticeStack::GetNextChangeMs() const
{
    std::optional<int64_t>  nextMs;
    int64_t                 changeMs = 0;



    for (const Notice & notice : m_notices)
    {
        changeMs = notice.banner->GetUntilMs();

        if (!notice.slide.IsDone (m_nowMs))
        {
            changeMs = std::min (changeMs, notice.slide.GetEndMs());
        }

        nextMs = std::min (nextMs.value_or (changeMs), changeMs);
    }

    return nextMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::GetText
//
////////////////////////////////////////////////////////////////////////////////

const std::wstring & DxuiNoticeStack::GetText (size_t index) const
{
    static const std::wstring  kNoText;



    return (index < m_notices.size()) ? m_notices[index].banner->GetText() : kNoText;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::GetOffsetPx
//
//  Where notice `index` sits at nowMs, below the top of the bounds: its
//  resting place under the notices above it, plus whatever of its slide is
//  still to travel.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiNoticeStack::GetOffsetPx (size_t index, int64_t nowMs) const
{
    float  offsetPx = 0.0f;



    if (index < m_notices.size())
    {
        offsetPx = GetRestingTopPx (index) + m_notices[index].slide.GetOffset (nowMs);
    }

    return offsetPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::SetDpi
//
////////////////////////////////////////////////////////////////////////////////

void DxuiNoticeStack::SetDpi (UINT dpi)
{
    m_dpi = dpi;

    for (Notice & notice : m_notices)
    {
        notice.banner->SetDpi (dpi);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::MeasureHeightPx
//
//  Each notice's height is kept, not only the total: an expiry slides the
//  notices below by the height of the one that left, which is known only
//  from the last measurement.
//
//  Heights are rounded up, as the banner's own estimate is, so the last line
//  of a notice never falls outside its strip.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiNoticeStack::MeasureHeightPx (
    IDxuiTextRenderer    * text,
    float                  widthPx,
    const DxuiDpiScaler  & scaler)
{
    float  extentPx = 0.0f;



    m_gapPx = std::ceil (scaler.ToPxf ((float) kGapDip));

    for (Notice & notice : m_notices)
    {
        notice.heightPx = std::ceil ((text != nullptr)
                                     ? notice.banner->GetMeasuredHeightPx (*text, widthPx, scaler)
                                     : notice.banner->GetPreferredHeightPx (widthPx, scaler));
    }

    for (size_t i = 0; i < m_notices.size(); i++)
    {
        extentPx = std::max (extentPx, GetOffsetPx (i, m_nowMs) + m_notices[i].heightPx);
    }

    return extentPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::Layout
//
//  The stack takes the bounds it is given and hangs each notice from their
//  top edge at its place at the last tick, full width, at its measured
//  height.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiNoticeStack::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    RECT  rc = boundsDip;



    SetBounds (boundsDip);

    for (size_t i = 0; i < m_notices.size(); i++)
    {
        rc.top    = boundsDip.top + (LONG) std::lround (GetOffsetPx (i, m_nowMs));
        rc.bottom = rc.top + (LONG) m_notices[i].heightPx;

        m_notices[i].banner->Layout (rc, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiNoticeStack::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    for (Notice & notice : m_notices)
    {
        if (notice.banner->IsVisible())
        {
            notice.banner->Paint (painter, text, theme);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::Tick
//
//  Takes down every notice whose time is up, top to bottom, each starting
//  the notices below it on their slide into its place.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiNoticeStack::Tick (int64_t nowMs)
{
    size_t  i = 0;



    m_nowMs = std::max (m_nowMs, nowMs);

    while (i < m_notices.size())
    {
        if (m_notices[i].banner->IsShowing (m_nowMs))
        {
            i++;
            continue;
        }

        RemoveNotice (i, m_nowMs);
    }

    if (m_notices.empty())
    {
        SetVisible (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::GetChild
//
//  Each notice is a child, so each keeps the banner's label role and its
//  text for accessibility.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DxuiNoticeStack::GetChild (size_t index) const
{
    return (index < m_notices.size()) ? m_notices[index].banner.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::RemoveNotice
//
//  Every notice below the one removed moves up by its height and the gap.
//  A notice already sliding keeps what it has still to travel and adds this
//  distance to it, so a second expiry mid-slide carries on from where the
//  notice is rather than jumping back.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiNoticeStack::RemoveNotice (size_t index, int64_t nowMs)
{
    float  distancePx = m_notices[index].heightPx + m_gapPx;
    float  currentPx  = 0.0f;



    for (size_t i = index + 1; i < m_notices.size(); i++)
    {
        currentPx          = m_notices[i].slide.GetOffset (nowMs);
        m_notices[i].slide = DxuiSlide::Start (currentPx + distancePx, nowMs, m_isAnimated);
    }

    m_notices.erase (m_notices.begin() + (ptrdiff_t) index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack::GetRestingTopPx
//
//  Where notice `index` comes to rest: below every notice above it, each
//  with the gap after it.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiNoticeStack::GetRestingTopPx (size_t index) const
{
    float  topPx = 0.0f;



    for (size_t i = 0; i < index && i < m_notices.size(); i++)
    {
        topPx += m_notices[i].heightPx + m_gapPx;
    }

    return topPx;
}
