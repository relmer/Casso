#include "Pch.h"

#include "Ui/Debugger/Panes/FlameGraphView.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerStatusText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView::SetData
//
//  A snapshot of a running machine has no profile; the last one read stays.
//  A new tree has another node order, so the zoom and a click waiting for
//  its second move to the node with the same routines from Top level down;
//  with none, the zoom goes back to Top level and the click is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void FlameGraphView::SetData (const std::optional<ProfileData> & profile)
{
    std::vector<uint64_t>  zoomPath;
    std::vector<uint64_t>  clickPath;
    std::optional<size_t>  clicked;



    if (!profile.has_value())
    {
        return;
    }

    zoomPath = GetPath (m_tree, m_routines, m_zoom);

    if (m_click.has_value())
    {
        clickPath = GetPath (m_tree, m_routines, m_click->node);
    }

    m_tree     = profile->tree;
    m_routines = profile->routines;
    m_zoom     = FindPath (m_tree, m_routines, zoomPath).value_or (0);
    clicked    = FindPath (m_tree, m_routines, clickPath);

    if (m_click.has_value() && clicked.has_value())
    {
        m_click->node = *clicked;
    }
    else
    {
        m_click.reset();
    }

    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView::GetPath
//
//  The routines from Top level down to node, each its address and kind;
//  empty for a node past the tree's end. A node's caller is the nearest
//  node before it one level up.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint64_t> FlameGraphView::GetPath (const std::vector<ProfileTreeEntry> & tree, const std::vector<ProfileRoutineEntry> & routines, size_t node)
{
    std::vector<uint64_t>  path;
    int                    depth = 0;



    if (node >= tree.size())
    {
        return path;
    }

    depth = tree[node].depth;

    for (size_t i = node + 1; i > 0 && depth >= 0; i--)
    {
        const ProfileTreeEntry  & entry = tree[i - 1];

        if (entry.depth == depth && entry.routine < routines.size())
        {
            path.push_back (((uint64_t) routines[entry.routine].kind << 16) | routines[entry.routine].address);
            depth--;
        }
    }

    std::ranges::reverse (path);
    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView::FindPath
//
//  The node whose routines from Top level down are path; none for an empty
//  path or one the tree no longer has.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<size_t> FlameGraphView::FindPath (const std::vector<ProfileTreeEntry> & tree, const std::vector<ProfileRoutineEntry> & routines, const std::vector<uint64_t> & path)
{
    for (size_t i = 0; i < tree.size() && !path.empty(); i++)
    {
        if (tree[i].depth + 1 == (int) path.size() && GetPath (tree, routines, i) == path)
        {
            return i;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView::SetZoom
//
////////////////////////////////////////////////////////////////////////////////

void FlameGraphView::SetZoom (size_t node)
{
    m_zoom = (node < m_tree.size()) ? node : 0;
    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView::LayoutBars
//
//  The tree is depth first, so a node's parent is the last node before it a
//  level up, and its callees are the nodes after it a level down until one
//  at its level or above.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<FlameGraphView::Bar> FlameGraphView::LayoutBars (const std::vector<ProfileTreeEntry> & tree, size_t zoom, float left, float width)
{
    std::vector<Bar>                  bars;
    std::vector<int>                  parents (tree.size(), -1);
    std::vector<std::vector<size_t>>  callees (tree.size());
    std::vector<size_t>               open;
    std::vector<size_t>               chain;
    std::vector<Bar>                  pending;
    int                               above   = 0;



    if (zoom >= tree.size() || width <= 0.0f)
    {
        return bars;
    }

    for (size_t i = 0; i < tree.size(); i++)
    {
        while (!open.empty() && tree[open.back()].depth >= tree[i].depth)
        {
            open.pop_back();
        }

        if (!open.empty())
        {
            parents[i] = (int) open.back();
            callees[open.back()].push_back (i);
        }

        open.push_back (i);
    }

    for (int node = parents[zoom]; node >= 0; node = parents[(size_t) node])
    {
        chain.push_back ((size_t) node);
    }

    for (size_t i = chain.size(); i-- > 0;)
    {
        bars.push_back (Bar { chain[i], above++, left, width });
    }

    pending.push_back (Bar { zoom, above, left, width });

    while (!pending.empty())
    {
        Bar    bar  = pending.back();
        float  x    = bar.x;

        pending.pop_back();
        bars.push_back (bar);

        for (size_t callee : callees[bar.node])
        {
            float  share = (tree[bar.node].total != 0) ? bar.width * (float) tree[callee].total / (float) tree[bar.node].total : 0.0f;

            if (share >= 1.0f)
            {
                pending.push_back (Bar { callee, bar.row + 1, x, share });
            }

            x += share;
        }
    }

    return bars;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView::GetRoutineColor
//
//  Mid-tones, light enough for dark text and dark enough for the light theme's
//  page around them.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t FlameGraphView::GetRoutineColor (size_t routine)
{
    static constexpr std::array<uint32_t, 8>  kColors =
    {
        0xFF5B9BD5, 0xFFED9A4C, 0xFF6DBE6D, 0xFFB884D6, 0xFFE3C55A, 0xFFE07070, 0xFF56C3B6, 0xFFD784B0,
    };



    return kColors[routine % kColors.size()];
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView::FindBarAt
//
////////////////////////////////////////////////////////////////////////////////

std::optional<size_t> FlameGraphView::FindBarAt (POINT point) const
{
    float  row = m_scaler.ToPxf ((float) kRowDip);



    for (size_t i = 0; i < m_bars.size(); i++)
    {
        const Bar  & bar = m_bars[i];
        float        top = GetRowTop (bar.row);

        if ((float) point.x >= bar.x && (float) point.x < bar.x + bar.width && (float) point.y >= top && (float) point.y < top + row)
        {
            return i;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView::TryGetTipAt
//
//  The routine, its own and its total cycles with their shares of the
//  whole profile, and its calls.
//
////////////////////////////////////////////////////////////////////////////////

bool FlameGraphView::TryGetTipAt (POINT point, std::wstring & text) const
{
    std::optional<size_t>      bar  = FindBarAt (point);
    const ProfileTreeEntry   * node = nullptr;
    double                     all  = 0.0;



    if (!m_visible || !bar.has_value())
    {
        return false;
    }

    node = &m_tree[m_bars[*bar].node];
    all  = std::max (1.0, (double) m_tree[0].total);

    text = std::format (L"{}\nSelf: {} cycles ({:.1f}%)\nTotal: {} cycles ({:.1f}%)\nCalls: {}",
                        TextEncoding::NarrowToWide (m_routines[node->routine].GetName()),
                        DebuggerStatusText::FormatCount (node->self), 100.0 * (double) node->self / all,
                        DebuggerStatusText::FormatCount (node->total), 100.0 * (double) node->total / all, DebuggerStatusText::FormatCount (node->calls));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void FlameGraphView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_boundsDip = boundsPx;
    m_scaler    = scaler;

    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView::Relayout
//
////////////////////////////////////////////////////////////////////////////////

void FlameGraphView::Relayout()
{
    m_bars = LayoutBars (m_tree, m_zoom, (float) m_boundsDip.left, (float) (m_boundsDip.right - m_boundsDip.left));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView::GetRowTop
//
////////////////////////////////////////////////////////////////////////////////

float FlameGraphView::GetRowTop (int row) const
{
    return (float) m_boundsDip.top + m_scaler.ToPxf ((float) kRowDip) * (float) row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView::Paint
//
//  Each bar a pixel short of its row and of its neighbor, so the bars stay
//  apart, with its routine's name where it fits.
//
////////////////////////////////////////////////////////////////////////////////

void FlameGraphView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font  = theme.BodyFont();
    float           row   = m_scaler.ToPxf ((float) kRowDip);
    float           label = m_scaler.ToPxf ((float) kLabelDip);
    float           pad   = m_scaler.ToPxf (4.0f);
    std::wstring    name;
    HRESULT         hr    = S_OK;



    if (!m_visible || m_boundsDip.right <= m_boundsDip.left)
    {
        return;
    }

    painter.PushClip ((float) m_boundsDip.left, (float) m_boundsDip.top, (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top));

    for (const Bar & bar : m_bars)
    {
        float     top   = GetRowTop (bar.row);
        uint32_t  fill  = GetRoutineColor (m_tree[bar.node].routine);

        if (top >= (float) m_boundsDip.bottom)
        {
            continue;
        }

        painter.FillRect (bar.x, top, std::max (1.0f, bar.width - 1.0f), row - 1.0f, fill);

        if (bar.width < label)
        {
            continue;
        }

        name = TextEncoding::NarrowToWide (m_routines[m_tree[bar.node].routine].GetName());

        hr = text.PushClipRect (bar.x, top, bar.width - 1.0f, row - 1.0f);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (name.c_str(), bar.x + pad, top, bar.width - 2.0f * pad, row - 1.0f,
                              DxuiColor::ChooseInkFor (fill, 0xFF101010, 0xFFF8F8F8), m_scaler.ToPxf (font.sizeDip), font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    painter.PopClip();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView::OnMouse
//
//  A second click within the system's double-click time and distance of the
//  first goes to the routine the first was on; otherwise a click zooms.
//
////////////////////////////////////////////////////////////////////////////////

bool FlameGraphView::OnMouse (const DxuiMouseEvent & ev)
{
    std::optional<size_t>  bar      = FindBarAt (ev.positionDip);
    ULONGLONG              now      = GetTickCount64();
    bool                   isDouble = false;
    Word                   address  = 0;



    if (ev.kind != DxuiMouseEventKind::Up || ev.button != DxuiMouseButton::Left || !bar.has_value())
    {
        return false;
    }

    isDouble = m_click.has_value() && now - m_click->time <= (ULONGLONG) GetDoubleClickTime() &&
               std::abs (ev.positionDip.x - m_click->at.x) <= GetSystemMetrics (SM_CXDOUBLECLK) &&
               std::abs (ev.positionDip.y - m_click->at.y) <= GetSystemMetrics (SM_CYDOUBLECLK);

    if (isDouble)
    {
        const ProfileRoutineEntry  & routine = m_routines[m_tree[m_click->node].routine];

        address = routine.address;
        m_click.reset();

        if (routine.kind != RoutineKind::Top && m_activate)
        {
            m_activate (address);
        }

        return true;
    }

    m_click = Click { now, ev.positionDip, m_bars[*bar].node };
    SetZoom (m_bars[*bar].node);
    return true;
}
