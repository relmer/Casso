#include "Pch.h"

#include "Ui/Debugger/Panes/CommSignalGraph.h"
#include "Devices/Network/W5100Registers.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/Panes/FlameGraphView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommSignalGraph::SetLanes
//
////////////////////////////////////////////////////////////////////////////////

void CommSignalGraph::SetLanes (
    std::vector<CommLane>  lanes,
    std::vector<bool>      shown,
    uint64_t               first,
    uint64_t               last,
    bool                   isNetwork)
{
    m_lanes     = std::move (lanes);
    m_shown     = std::move (shown);
    m_first     = first;
    m_last      = std::max (last, first + 1);
    m_isNetwork = isNetwork;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommSignalGraph::GetHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int CommSignalGraph::GetHeightDip() const
{
    int  count = 0;



    for (size_t i = 0; i < m_lanes.size(); i++)
    {
        count += (i >= m_shown.size() || m_shown[i]) ? 1 : 0;
    }

    return count * kLaneDip + kAxisDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommSignalGraph::GetX
//
////////////////////////////////////////////////////////////////////////////////

float CommSignalGraph::GetX (uint64_t cycle) const
{
    float     left  = (float) m_boundsDip.left + m_scaler.ToPxf ((float) kLabelDip);
    float     width = std::max (1.0f, (float) m_boundsDip.right - left);
    uint64_t  clamp = std::clamp (cycle, m_first, m_last);



    return left + width * (float) (clamp - m_first) / (float) (m_last - m_first);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommSignalGraph::GetCycleAt
//
//  None left of the lanes, in the labels.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<uint64_t> CommSignalGraph::GetCycleAt (POINT point) const
{
    float  left  = (float) m_boundsDip.left + m_scaler.ToPxf ((float) kLabelDip);
    float  width = (float) m_boundsDip.right - left;



    if (!m_visible || width <= 0.0f || (float) point.x < left || point.x >= m_boundsDip.right || point.y < m_boundsDip.top || point.y >= m_boundsDip.bottom)
    {
        return std::nullopt;
    }

    return m_first + (uint64_t) ((double) (m_last - m_first) * ((double) point.x - left) / (double) width);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommSignalGraph::TryGetTipAt
//
////////////////////////////////////////////////////////////////////////////////

bool CommSignalGraph::TryGetTipAt (POINT point, std::wstring & text) const
{
    std::optional<uint64_t>  cycle = GetCycleAt (point);



    if (!cycle.has_value() || m_lanes.empty())
    {
        return false;
    }

    text = CommGraph::GetTip (m_lanes, m_shown, *cycle);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommSignalGraph::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CommSignalGraph::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_boundsDip = boundsPx;
    m_scaler    = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommSignalGraph::GetStateColor
//
//  A socket's state by kind: none closed, amber on the way to a connection,
//  green connected, orange closing, blue for UDP and the raw modes.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t CommSignalGraph::GetStateColor (Byte status)
{
    using R = W5100Registers;

    switch (status)
    {
    case R::kSockClosed:      return 0;
    case R::kSockInit:
    case R::kSockListen:
    case R::kSockSynSent:
    case R::kSockSynReceived: return 0xFFD9A441;
    case R::kSockEstablished: return 0xFF4CAF50;
    case R::kSockUdp:
    case R::kSockIpRaw:
    case R::kSockMacRaw:      return 0xFF4A90D9;
    default:                  return 0xFFE07B39;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommSignalGraph::PaintLevels
//
//  The trace at the lane's rest level between segments, and at each
//  segment's level across it, with an edge wherever the level changes. A
//  segment that begins before the span or ends after it has no edge at the
//  plot's end, where the level does not change.
//
////////////////////////////////////////////////////////////////////////////////

void CommSignalGraph::PaintLevels (
    IDxuiPainter    & painter,
    const CommLane  & lane,
    float             top,
    float             bottom,
    uint32_t          color) const
{
    float     thick  = m_scaler.ToPxf (1.5f);
    float     inset  = m_scaler.ToPxf (4.0f);
    float     high   = top + inset;
    float     low    = bottom - inset;
    int       level  = lane.restLevel;
    float     x      = GetX (m_first);
    uint64_t  reach  = m_first;
    auto      levelY = [&] (int value) { return (value != 0) ? high : low; };
    auto      run    = [&] (float to, int value)
    {
        if (value != level)
        {
            painter.DrawLine (x, high, x, low, thick, color);
            level = value;
        }

        if (to > x)
        {
            painter.DrawLine (x, levelY (level), to, levelY (level), thick, color);
            x = to;
        }
    };



    for (const CommSegment & segment : lane.segments)
    {
        if (segment.last <= m_first || segment.first >= m_last)
        {
            continue;
        }

        if (segment.first < m_first)
        {
            level = segment.level;
        }
        else if (segment.first > reach)
        {
            run (GetX (segment.first), lane.restLevel);
        }

        run (GetX (std::min (segment.last, m_last)), segment.level);
        reach = std::max (reach, segment.last);
    }

    if (reach < m_last)
    {
        run (GetX (m_last), lane.restLevel);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommSignalGraph::PaintStates
//
////////////////////////////////////////////////////////////////////////////////

void CommSignalGraph::PaintStates (
    IDxuiPainter    & painter,
    const CommLane  & lane,
    float             top,
    float             bottom,
    uint32_t          tick) const
{
    float  inset = m_scaler.ToPxf (3.0f);
    float  mid   = (top + bottom) / 2.0f;
    float  x     = 0.0f;
    float  width = 0.0f;



    for (const CommSegment & segment : lane.segments)
    {
        uint32_t  color = GetStateColor ((Byte) segment.level);

        if (color == 0 || segment.last <= m_first || segment.first >= m_last)
        {
            continue;
        }

        x     = GetX (segment.first);
        width = std::max (1.0f, GetX (segment.last) - x);
        painter.FillRect (x, top + inset, width, bottom - top - 2.0f * inset, color);
    }

    for (const CommMark & mark : lane.marks)
    {
        if (mark.cycle < m_first || mark.cycle >= m_last)
        {
            continue;
        }

        x = GetX (mark.cycle);
        painter.FillRect (x, mark.isReceived ? mid : top + 1.0f, m_scaler.ToPxf (2.0f), mid - top - 1.0f, tick);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommSignalGraph::Paint
//
////////////////////////////////////////////////////////////////////////////////

void CommSignalGraph::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font   = theme.BodyFont();
    float           size   = m_scaler.ToPxf (font.sizeDip);
    float           laneH  = m_scaler.ToPxf ((float) kLaneDip);
    float           labelW = m_scaler.ToPxf ((float) kLabelDip);
    float           left   = (float) m_boundsDip.left;
    float           width  = (float) (m_boundsDip.right - m_boundsDip.left);
    float           y      = (float) m_boundsDip.top;
    float           axisH  = m_scaler.ToPxf ((float) kAxisDip);
    HRESULT         hr     = S_OK;



    if (!m_visible || width <= labelW || m_boundsDip.bottom <= m_boundsDip.top)
    {
        return;
    }

    painter.FillRect (left, y, width, (float) (m_boundsDip.bottom - m_boundsDip.top), theme.ContentBackground());

    for (size_t i = 0; i < m_lanes.size(); i++)
    {
        if (i < m_shown.size() && !m_shown[i])
        {
            continue;
        }

        hr = text.DrawString (m_lanes[i].name.c_str(), left + m_scaler.ToPxf (4.0f), y, labelW, laneH, theme.ForegroundMuted(), size, font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (m_isNetwork)
        {
            PaintStates (painter, m_lanes[i], y, y + laneH, theme.Foreground());
        }
        else
        {
            PaintLevels (painter, m_lanes[i], y, y + laneH, FlameGraphView::GetRoutineColor (i));
        }

        painter.FillRect (left, y + laneH - 1.0f, width, 1.0f, theme.Divider());
        y += laneH;
    }

    hr = text.DrawString (std::format (L"Cycle {}", DebuggerStatusText::FormatCount (m_first)).c_str(), left + labelW, y, width - labelW, axisH,
                          theme.ForegroundMuted(), size, font.face, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (std::format (L"{} cycles", DebuggerStatusText::FormatCount (m_last - m_first)).c_str(), left + labelW, y, width - labelW - m_scaler.ToPxf (4.0f), axisH,
                          theme.ForegroundMuted(), size, font.face, DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
