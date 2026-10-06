#include "Pch.h"

#include "Widgets/DxuiToolbarDock.h"





static constexpr std::pair<DxuiToolbarDock::Edge, const wchar_t *>  s_kEdgeWords[] =
{
    { DxuiToolbarDock::Edge::Top,    L"top"    },
    { DxuiToolbarDock::Edge::Bottom, L"bottom" },
    { DxuiToolbarDock::Edge::Left,   L"left"   },
    { DxuiToolbarDock::Edge::Right,  L"right"  },
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock::ToText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiToolbarDock::ToText() const
{
    std::wstring  word = L"top";



    if (floating)
    {
        return std::format (L"float {} {}{}{}", floatPx.x, floatPx.y, floatVertical ? L" vertical" : L"",
                            (floatLengthDip > 0) ? std::format (L" length {}", floatLengthDip) : std::wstring());
    }

    for (const auto & [e, w] : s_kEdgeWords)
    {
        if (e == edge)
        {
            word = w;
        }
    }

    return word + L" " + std::to_wstring (offsetDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock::FromText
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbarDock DxuiToolbarDock::FromText (const std::wstring & text)
{
    DxuiToolbarDock    dock;
    size_t             space  = text.find (L' ');
    std::wstring       word   = text.substr (0, space);
    bool               known  = false;
    wchar_t          * end    = nullptr;
    long               offset = 0;



    if (word == L"float" && space != std::wstring::npos)
    {
        return ReadFloating (text.substr (space + 1));
    }

    for (const auto & [e, w] : s_kEdgeWords)
    {
        if (word == w)
        {
            dock.edge = e;
            known     = true;
        }
    }

    if (!known || space == std::wstring::npos)
    {
        return DxuiToolbarDock {};
    }

    offset = wcstol (text.c_str() + space + 1, &end, 10);

    if (end == text.c_str() + space + 1 || *end != L'\0' || offset < 0)
    {
        return DxuiToolbarDock {};
    }

    dock.offsetDip = (int) offset;
    return dock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock::ReadFloating
//
//  "300 -40" after the "float": two numbers, then " vertical" or nothing,
//  then " length 640" or nothing, or the default place.
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbarDock DxuiToolbarDock::ReadFloating (const std::wstring & text)
{
    constexpr std::wstring_view  kVertical = L" vertical";
    constexpr std::wstring_view  kLength   = L" length ";



    DxuiToolbarDock    dock;
    const wchar_t    * start  = text.c_str();
    wchar_t          * end    = nullptr;
    long               x      = 0;
    long               y      = 0;
    long               length = 0;
    bool               isRead = true;
    std::wstring_view  rest;



    x = wcstol (start, &end, 10);

    if (end == start || *end != L' ')
    {
        return DxuiToolbarDock {};
    }

    start = end + 1;
    y     = wcstol (start, &end, 10);
    rest  = std::wstring_view (end);

    if (end == start)
    {
        return DxuiToolbarDock {};
    }

    dock.floatVertical = rest.starts_with (kVertical);
    rest               = dock.floatVertical ? rest.substr (kVertical.size()) : rest;

    if (rest.starts_with (kLength))
    {
        start  = rest.data() + kLength.size();
        length = wcstol (start, &end, 10);
        isRead = end != start && length > 0;
        rest   = std::wstring_view (end);
    }

    if (!isRead || !rest.empty())
    {
        return DxuiToolbarDock {};
    }

    dock.floating       = true;
    dock.floatPx        = POINT { x, y };
    dock.floatLengthDip = (int) length;
    return dock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock::PickForDrop
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbarDock DxuiToolbarDock::PickForDrop (POINT pointer, POINT grab, const RECT & area, int dpi)
{
    DxuiToolbarDock  dock;
    int              toTop    = pointer.y - area.top;
    int              toBottom = area.bottom - pointer.y;
    int              toLeft   = pointer.x - area.left;
    int              toRight  = area.right - pointer.x;
    int              nearest  = (std::min) ((std::min) (toTop, toBottom), (std::min) (toLeft, toRight));
    int              offsetPx = 0;



    if (nearest == toTop)
    {
        dock.edge = Edge::Top;
    }
    else if (nearest == toBottom)
    {
        dock.edge = Edge::Bottom;
    }
    else if (nearest == toLeft)
    {
        dock.edge = Edge::Left;
    }
    else
    {
        dock.edge = Edge::Right;
    }

    offsetPx       = dock.IsVertical() ? pointer.y - area.top - grab.y : pointer.x - area.left - grab.x;
    dock.offsetDip = (std::max) (0, MulDiv (offsetPx, 96, (std::max) (dpi, 1)));

    return dock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock::ClampOffset
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbarDock::ClampOffset (int offsetPx, int edgeLength, int barLength)
{
    return std::clamp (offsetPx, 0, (std::max) (0, edgeLength - barLength));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock::IsInDockBand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarDock::IsInDockBand (POINT pointer, const RECT & area, int bandPx)
{
    int  toTop    = pointer.y - area.top;
    int  toBottom = area.bottom - pointer.y;
    int  toLeft   = pointer.x - area.left;
    int  toRight  = area.right - pointer.x;
    int  nearest  = (std::min) ((std::min) (toTop, toBottom), (std::min) (toLeft, toRight));



    return nearest >= 0 && nearest <= bandPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock::SlideAlong
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbarDock DxuiToolbarDock::SlideAlong (POINT pointer, POINT grab, const DxuiToolbarDock & current, const RECT & area, int dpi)
{
    DxuiToolbarDock  dock     = current;
    int              offsetPx = 0;



    dock.floating  = false;
    offsetPx       = dock.IsVertical() ? pointer.y - area.top - grab.y : pointer.x - area.left - grab.x;
    dock.offsetDip = (std::max) (0, MulDiv (offsetPx, 96, (std::max) (dpi, 1)));

    return dock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock::IsPulledOut
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarDock::IsPulledOut (POINT pointer, Edge edge, const RECT & area, int bandPx, int pullPx)
{
    int  inward = 0;
    int  along  = 0;
    int  start  = 0;
    int  end    = 0;



    switch (edge)
    {
    case Edge::Top:    inward = pointer.y - area.top;    break;
    case Edge::Bottom: inward = area.bottom - pointer.y; break;
    case Edge::Left:   inward = pointer.x - area.left;   break;
    case Edge::Right:  inward = area.right - pointer.x;  break;
    }

    if (edge == Edge::Top || edge == Edge::Bottom)
    {
        along = pointer.x;
        start = area.left;
        end   = area.right;
    }
    else
    {
        along = pointer.y;
        start = area.top;
        end   = area.bottom;
    }

    return inward > bandPx + pullPx || inward < -pullPx || along < start - pullPx || along > end + pullPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock::PickFloatVertical
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarDock::PickFloatVertical (POINT pointer, const RECT & area, int bandPx, bool current)
{
    int  toTop    = pointer.y - area.top;
    int  toBottom = area.bottom - pointer.y;
    int  toLeft   = pointer.x - area.left;
    int  toRight  = area.right - pointer.x;
    int  nearest  = (std::min) ((std::min) (toTop, toBottom), (std::min) (toLeft, toRight));



    if (nearest < 0 || nearest > bandPx)
    {
        return current;
    }

    return nearest == toLeft || nearest == toRight;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock::GrabForDocking
//
////////////////////////////////////////////////////////////////////////////////

POINT DxuiToolbarDock::GrabForDocking (POINT grabPx, bool vertical, int marginPx)
{
    int  along = vertical ? grabPx.y : grabPx.x;



    return POINT { along + marginPx, along };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock::TryGetFarEndEdge
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarDock::TryGetFarEndEdge (const RECT & toolbar, const RECT & previous, bool vertical, const RECT & area, int bandPx, Edge & outEdge)
{
    int   gap     = vertical ? area.bottom - toolbar.bottom  : area.right - toolbar.right;
    int   gapWas  = vertical ? area.bottom - previous.bottom : area.right - previous.right;
    bool  across  = vertical ? toolbar.right > area.left && toolbar.left < area.right
                             : toolbar.bottom > area.top && toolbar.top < area.bottom;



    if (!across || gap < 0 || gap > bandPx || gap >= gapWas)
    {
        return false;
    }

    outEdge = vertical ? Edge::Bottom : Edge::Right;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock::MakeDocked
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbarDock DxuiToolbarDock::MakeDocked (Edge edge, int offsetPx, int dpi)
{
    DxuiToolbarDock  dock;



    dock.edge      = edge;
    dock.offsetDip = (std::max) (0, MulDiv (offsetPx, USER_DEFAULT_SCREEN_DPI, (std::max) (dpi, 1)));

    return dock;
}




