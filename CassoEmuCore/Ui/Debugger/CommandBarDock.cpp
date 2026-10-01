#include "Pch.h"

#include "Ui/Debugger/CommandBarDock.h"





static constexpr std::pair<CommandBarDock::Edge, const wchar_t *>  s_kEdgeWords[] =
{
    { CommandBarDock::Edge::Top,    L"top"    },
    { CommandBarDock::Edge::Bottom, L"bottom" },
    { CommandBarDock::Edge::Left,   L"left"   },
    { CommandBarDock::Edge::Right,  L"right"  },
};





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarDock::ToText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CommandBarDock::ToText() const
{
    std::wstring  word = L"top";



    if (floating)
    {
        return std::format (L"float {} {}", floatPx.x, floatPx.y);
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
//  CommandBarDock::FromText
//
////////////////////////////////////////////////////////////////////////////////

CommandBarDock CommandBarDock::FromText (const std::wstring & text)
{
    CommandBarDock  dock;
    size_t          space  = text.find (L' ');
    std::wstring    word   = text.substr (0, space);
    bool            known  = false;
    wchar_t       * end    = nullptr;
    long            offset = 0;



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
        return CommandBarDock {};
    }

    offset = wcstol (text.c_str() + space + 1, &end, 10);

    if (end == text.c_str() + space + 1 || *end != L'\0' || offset < 0)
    {
        return CommandBarDock {};
    }

    dock.offsetDip = (int) offset;
    return dock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarDock::ReadFloating
//
//  "300 -40" after the "float": two numbers and nothing else, or the
//  default place.
//
////////////////////////////////////////////////////////////////////////////////

CommandBarDock CommandBarDock::ReadFloating (const std::wstring & text)
{
    CommandBarDock    dock;
    const wchar_t   * start = text.c_str();
    wchar_t         * end   = nullptr;
    long              x     = 0;
    long              y     = 0;



    x = wcstol (start, &end, 10);

    if (end == start || *end != L' ')
    {
        return CommandBarDock {};
    }

    start = end + 1;
    y     = wcstol (start, &end, 10);

    if (end == start || *end != L'\0')
    {
        return CommandBarDock {};
    }

    dock.floating = true;
    dock.floatPx  = POINT { x, y };
    return dock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarDock::PickForDrop
//
////////////////////////////////////////////////////////////////////////////////

CommandBarDock CommandBarDock::PickForDrop (POINT pointer, POINT grab, const RECT & area, int dpi)
{
    CommandBarDock  dock;
    int             toTop    = pointer.y - area.top;
    int             toBottom = area.bottom - pointer.y;
    int             toLeft   = pointer.x - area.left;
    int             toRight  = area.right - pointer.x;
    int             nearest  = (std::min) ((std::min) (toTop, toBottom), (std::min) (toLeft, toRight));
    int             offsetPx = 0;



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
//  CommandBarDock::ClampOffset
//
////////////////////////////////////////////////////////////////////////////////

int CommandBarDock::ClampOffset (int offsetPx, int edgeLength, int barLength)
{
    return std::clamp (offsetPx, 0, (std::max) (0, edgeLength - barLength));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarDock::IsInDockBand
//
////////////////////////////////////////////////////////////////////////////////

bool CommandBarDock::IsInDockBand (POINT pointer, const RECT & area, int bandPx)
{
    int  toTop    = pointer.y - area.top;
    int  toBottom = area.bottom - pointer.y;
    int  toLeft   = pointer.x - area.left;
    int  toRight  = area.right - pointer.x;
    int  nearest  = (std::min) ((std::min) (toTop, toBottom), (std::min) (toLeft, toRight));



    return nearest >= 0 && nearest <= bandPx;
}
