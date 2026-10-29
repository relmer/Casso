#include "Pch.h"

#include "Ui/Debugger/Panes/ProfileTimeline.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerStatusText.h"

#include "Ui/Debugger/Panes/FlameGraphView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline::SetData
//
//  A snapshot of a running machine has no profile; the last one read stays.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTimeline::SetData (const std::optional<ProfileData> & profile)
{
    if (!profile.has_value())
    {
        return;
    }

    m_frames   = profile->frames;
    m_routines = profile->routines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline::GetDataSpan
//
//  From the first frame's start to the last one's end.
//
////////////////////////////////////////////////////////////////////////////////

ProfileTimeline::Range ProfileTimeline::GetDataSpan() const
{
    constexpr int64_t  kFrame = (int64_t) RoutineProfile::kCyclesPerFrame;



    if (m_frames.empty())
    {
        return { 0, (int64_t) RoutineProfile::kFrameCount * kFrame };
    }

    return { (int64_t) m_frames.front().index * kFrame, (int64_t) (m_frames.back().index + m_frames.back().count) * kFrame };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline::GetDefaultView
//
////////////////////////////////////////////////////////////////////////////////

ProfileTimeline::Range ProfileTimeline::GetDefaultView() const
{
    Range  span = GetDataSpan();



    return { std::min (span.first, span.second - (int64_t) RoutineProfile::kFrameCount * (int64_t) RoutineProfile::kCyclesPerFrame), span.second };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline::SetView
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTimeline::SetView (int64_t fromCycle, int64_t toCycle)
{
    if (toCycle > fromCycle)
    {
        m_view = Range { fromCycle, toCycle };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline::GetCycleX
//
////////////////////////////////////////////////////////////////////////////////

float ProfileTimeline::GetCycleX (int64_t cycle) const
{
    Range   view  = GetView();
    double  width = (double) (m_boundsDip.right - m_boundsDip.left);



    return (float) ((double) m_boundsDip.left + width * (double) (cycle - view.first) / (double) std::max<int64_t> (view.second - view.first, 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline::GetCycleAt
//
////////////////////////////////////////////////////////////////////////////////

int64_t ProfileTimeline::GetCycleAt (int x) const
{
    Range   view  = GetView();
    double  width = (double) std::max<LONG> (m_boundsDip.right - m_boundsDip.left, 1);



    return view.first + (int64_t) std::llround ((double) (view.second - view.first) * (double) (x - m_boundsDip.left) / width);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline::FindFrameAt
//
////////////////////////////////////////////////////////////////////////////////

std::optional<size_t> ProfileTimeline::FindFrameAt (POINT point) const
{
    constexpr int64_t  kFrame = (int64_t) RoutineProfile::kCyclesPerFrame;
    int64_t            cycle  = GetCycleAt (point.x);



    if (point.x < m_boundsDip.left || point.x >= m_boundsDip.right || point.y < m_boundsDip.top || point.y >= m_boundsDip.bottom)
    {
        return std::nullopt;
    }

    for (size_t i = 0; i < m_frames.size(); i++)
    {
        if (cycle >= (int64_t) m_frames[i].index * kFrame && cycle < (int64_t) (m_frames[i].index + m_frames[i].count) * kFrame)
        {
            return i;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline::GetFrameTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ProfileTimeline::GetFrameTip (
    const ProfileFrameEntry                 & frame,
    const std::vector<ProfileRoutineEntry>  & routines,
    uint32_t                                  otherArgb,
    std::vector<uint32_t>                   & swatches)
{
    std::vector<std::pair<size_t, uint64_t>>  shares = frame.shares;
    uint64_t                                  total  = frame.other;
    uint64_t                                  rest   = frame.other;
    std::wstring                              text;



    for (const auto & [routine, cycles] : shares)
    {
        total += cycles;
    }

    std::stable_sort (shares.begin(), shares.end(), [] (const auto & a, const auto & b) { return a.second > b.second; });

    text = (frame.count > 1) ? std::format (L"Frames {} to {}: {} cycles", DebuggerStatusText::FormatCount (frame.index),
                                            DebuggerStatusText::FormatCount (frame.index + frame.count - 1), DebuggerStatusText::FormatCount (total))
                             : std::format (L"Frame {}: {} cycles", DebuggerStatusText::FormatCount (frame.index), DebuggerStatusText::FormatCount (total));

    swatches.assign (1, 0);

    for (size_t i = 0; i < shares.size(); i++)
    {
        if (i >= kTipRoutines)
        {
            rest += shares[i].second;
            continue;
        }

        text += std::format (L"\n{}: {} cycles ({:.1f}%)", TextEncoding::NarrowToWide (routines[shares[i].first].GetName()),
                             DebuggerStatusText::FormatCount (shares[i].second), 100.0 * (double) shares[i].second / (double) std::max<uint64_t> (total, 1));
        swatches.push_back (GetShareColor (shares[i].first, otherArgb));
    }

    if (rest != 0)
    {
        text += std::format (L"\nOther routines: {} cycles", DebuggerStatusText::FormatCount (rest));
        swatches.push_back (otherArgb);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline::SetPointer
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileTimeline::SetPointer (std::optional<int> x)
{
    bool  isChanged = x != m_pointerX;



    m_pointerX = x;
    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline::GetShareColor
//
//  The kColoredRoutines hottest routines have the flame graph's colors, and
//  the rest, SIZE_MAX among them, the color drawn for them together.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t ProfileTimeline::GetShareColor (size_t routine, uint32_t otherArgb)
{
    return (routine < kColoredRoutines) ? FlameGraphView::GetRoutineColor (routine) : otherArgb;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline::TryGetTipAt
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileTimeline::TryGetTipAt (
    POINT                    point,
    std::wstring           & text,
    std::vector<uint32_t>  & swatches) const
{
    std::optional<size_t>  frame = FindFrameAt (point);



    if (!m_visible || !frame.has_value())
    {
        return false;
    }

    text = GetFrameTip (m_frames[*frame], m_routines, m_otherArgb, swatches);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTimeline::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_boundsDip = boundsPx;
    m_scaler    = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline::Paint
//
//  A frame's routines are stacked from the bottom in their order in the
//  routines, hottest first, so a routine sits at the same height in frame
//  after frame.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTimeline::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr int64_t  kFrame = (int64_t) RoutineProfile::kCyclesPerFrame;
    float              height = (float) (m_boundsDip.bottom - m_boundsDip.top);
    float              bottom = (float) m_boundsDip.bottom;
    float              left   = (float) m_boundsDip.left;
    float              right  = (float) m_boundsDip.right;



    (void) text;

    if (!m_visible || height <= 0.0f || m_boundsDip.right <= m_boundsDip.left)
    {
        return;
    }

    m_otherArgb = theme.Divider();
    painter.FillRect ((float) m_boundsDip.left, bottom - 1.0f, (float) (m_boundsDip.right - m_boundsDip.left), 1.0f, theme.Divider());

    for (size_t i = 0; i < m_frames.size(); i++)
    {
        std::vector<std::pair<size_t, uint64_t>>  shares = m_frames[i].shares;
        float                                     x      = std::max (left,  GetCycleX ((int64_t) m_frames[i].index * kFrame));
        float                                     end    = std::min (right, GetCycleX ((int64_t) (m_frames[i].index + m_frames[i].count) * kFrame));
        float                                     width  = std::max (1.0f, end - x);
        float                                     y      = bottom;
        uint64_t                                  rest   = m_frames[i].other;

        //  Outside the range, the frame is not drawn.
        if (end <= x)
        {
            continue;
        }

        for (const auto & [routine, cycles] : shares)
        {
            rest += (routine >= kColoredRoutines) ? cycles : 0;
        }

        std::erase_if    (shares, [] (const auto & share) { return share.first >= kColoredRoutines; });
        std::stable_sort (shares.begin(), shares.end(), [] (const auto & a, const auto & b) { return a.first < b.first; });
        shares.push_back ({ SIZE_MAX, rest });

        for (const auto & [routine, cycles] : shares)
        {
            float  tall = height * (float) cycles / ((float) RoutineProfile::kCyclesPerFrame * (float) std::max<uint64_t> (m_frames[i].count, 1));

            tall = std::min (tall, y - (float) m_boundsDip.top);

            if (tall <= 0.0f)
            {
                continue;
            }

            y -= tall;
            painter.FillRect (x, y, width, tall, GetShareColor (routine, m_otherArgb));
        }
    }

    //  The selection, in the theme's focus color, see-through over the bars,
    //  with its edges solid.
    if (m_selection.has_value())
    {
        constexpr uint32_t  kSeeThrough = 0x50000000;
        uint32_t            accent      = theme.FocusAccent();
        float               from        = std::clamp (GetCycleX (m_selection->first),  left, right);
        float               to          = std::clamp (GetCycleX (m_selection->second), left, right);

        painter.FillRect (from, (float) m_boundsDip.top, std::max (1.0f, to - from), height, (accent & 0x00FFFFFF) | kSeeThrough);
        painter.FillRect (from, (float) m_boundsDip.top, 1.0f, height, accent);
        painter.FillRect (std::max (from, to - 1.0f), (float) m_boundsDip.top, 1.0f, height, accent);
    }

    if (m_pointerX.has_value())
    {
        painter.FillRect ((float) *m_pointerX, (float) m_boundsDip.top, std::max (1.0f, (float) std::lround (m_scaler.ToPxf (1.0f))), height,
                          theme.PlayheadHoverLine());
    }
}
