#include "Pch.h"

#include "Core/DxuiDockLayout.h"
#include "Core/DxuiThread.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockLayout::SetDock
//
//  Records the dock side for a child. Children without an explicit
//  mapping default to Fill.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockLayout::SetDock (IDxuiControl & child, DxuiDock side)
{
    DXUI_ASSERT_UI_THREAD();

    m_docks[&child] = side;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockLayout::ClearDock
//
//  Forgets a previously recorded dock side. The child reverts to the
//  default (Fill). Useful when a child is being removed from the
//  parent panel so the layout doesn't keep a stale pointer entry.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockLayout::ClearDock (IDxuiControl & child)
{
    DXUI_ASSERT_UI_THREAD();

    m_docks.erase (&child);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockLayout::DockOf
//
//  Returns the dock side recorded for a child, or `Fill` if none was
//  set. Provided for tests and consumers that need to inspect layout
//  state.
//
////////////////////////////////////////////////////////////////////////////////

DxuiDock DxuiDockLayout::DockOf (const IDxuiControl & child) const
{
    return LookupDock (&child);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockLayout::LookupDock
//
//  Internal lookup that maps a null / unregistered child to Fill.
//
////////////////////////////////////////////////////////////////////////////////

DxuiDock DxuiDockLayout::LookupDock (const IDxuiControl * child) const
{
    auto  it = m_docks.find (child);

    if (it == m_docks.end())
    {
        return DxuiDock::Fill;
    }
    return it->second;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockLayout::Arrange
//
//  Walk children in registration order. For each non-Fill child, peel
//  a slab off the matching edge of the remaining rect equal to the
//  child's natural Bounds() extent on the docked axis. The first
//  Fill child receives whatever rect is left. Subsequent Fill
//  children collapse to a zero-extent rect at the remaining top-left.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockLayout::Arrange (
    const RECT                          & boundsDip,
    const DxuiDpiScaler                 & /*scaler*/,
    std::span<IDxuiControl * const>       children)
{
    RECT  remaining = boundsDip;
    bool  fillAssigned = false;

    DXUI_ASSERT_UI_THREAD();

    for (IDxuiControl * child : children)
    {
        DxuiDock  side       = LookupDock (child);
        RECT      curBounds  = child->Bounds();
        LONG      naturalW   = curBounds.right  - curBounds.left;
        LONG      naturalH   = curBounds.bottom - curBounds.top;
        RECT      assigned   = {};

        if (side == DxuiDock::Fill)
        {
            if (fillAssigned)
            {
                assigned.left   = remaining.left;
                assigned.top    = remaining.top;
                assigned.right  = remaining.left;
                assigned.bottom = remaining.top;
            }
            else
            {
                assigned     = remaining;
                fillAssigned = true;
            }
            child->SetBounds (assigned);
            continue;
        }

        switch (side)
        {
        case DxuiDock::Top:
        {
            LONG  slabH = std::min<LONG> (naturalH, remaining.bottom - remaining.top);

            assigned.left   = remaining.left;
            assigned.top    = remaining.top;
            assigned.right  = remaining.right;
            assigned.bottom = remaining.top + slabH;
            remaining.top  += slabH;
            break;
        }

        case DxuiDock::Bottom:
        {
            LONG  slabH = std::min<LONG> (naturalH, remaining.bottom - remaining.top);

            assigned.left      = remaining.left;
            assigned.top       = remaining.bottom - slabH;
            assigned.right     = remaining.right;
            assigned.bottom    = remaining.bottom;
            remaining.bottom  -= slabH;
            break;
        }

        case DxuiDock::Left:
        {
            LONG  slabW = std::min<LONG> (naturalW, remaining.right - remaining.left);

            assigned.left    = remaining.left;
            assigned.top     = remaining.top;
            assigned.right   = remaining.left + slabW;
            assigned.bottom  = remaining.bottom;
            remaining.left  += slabW;
            break;
        }

        case DxuiDock::Right:
        {
            LONG  slabW = std::min<LONG> (naturalW, remaining.right - remaining.left);

            assigned.left    = remaining.right - slabW;
            assigned.top     = remaining.top;
            assigned.right   = remaining.right;
            assigned.bottom  = remaining.bottom;
            remaining.right -= slabW;
            break;
        }

        case DxuiDock::Fill:
            // Handled above.
            break;
        }

        child->SetBounds (assigned);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockLayout::ContainerSizeForFill
//
//  Inverse computation: given a desired Fill-region size and the
//  non-fill children that will surround it, return the container size
//  that yields exactly that Fill region. Top/Bottom contribute to
//  height; Left/Right contribute to width. Fill entries in the
//  provided span are ignored (the desired size already accounts for
//  the fill slot).
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiDockLayout::ContainerSizeForFill (
    SIZE                                          desiredFillDip,
    std::span<IDxuiControl * const>               nonFillChildren) const
{
    LONG  widthAdd  = 0;
    LONG  heightAdd = 0;
    SIZE  result    = desiredFillDip;

    for (IDxuiControl * child : nonFillChildren)
    {
        DxuiDock  side      = LookupDock (child);
        RECT      curBounds = child->Bounds();
        LONG      naturalW  = curBounds.right  - curBounds.left;
        LONG      naturalH  = curBounds.bottom - curBounds.top;

        switch (side)
        {
        case DxuiDock::Top:
        case DxuiDock::Bottom:
            heightAdd += naturalH;
            break;

        case DxuiDock::Left:
        case DxuiDock::Right:
            widthAdd += naturalW;
            break;

        case DxuiDock::Fill:
            // Ignored: the caller's desiredFillDip already represents
            // the fill region.
            break;
        }
    }

    result.cx += widthAdd;
    result.cy += heightAdd;
    return result;
}
