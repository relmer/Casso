#pragma once

#include "Pch.h"

#include "Widgets/DxuiPopupMenuTypes.h"

class DxuiPopupMenu;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout
//
//  A popup menu's metrics and scrolling: each row's height and top, the
//  icon row's buttons, the measured width and the columns the labels and
//  accelerators start at, the height the screen allows, and the rows
//  scrolled past when the menu is taller than that.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPopupMenuLayout
{
public:
    explicit DxuiPopupMenuLayout (DxuiPopupMenu & menu);

    int   GetRowHeightPx     (int index) const;
    int   GetIconButtonPx    () const;
    int   GetIconDividerPx   () const;
    int   GetIconRowLeftPx   () const;
    int   GetRowTopPx        (int index) const;
    int   GetContentHeightPx () const;
    int   GetScrollPx        () const;
    int   GetMaxScrollRow    () const;
    int   GetHeightLimitPx   (const RECT & anchor, DxuiPopupMenuAnchoring anchoring) const;
    void  ScrollByRows       (int rows);
    void  EnsureRowVisible   (int index);
    int   MeasureRunPx       (const std::wstring & run, float fontDip, IDxuiTextRenderer & text) const;
    int   MeasureWidthPx     (IDxuiTextRenderer & text);
    int   GetRowAtOffset     (int relY) const;

private:
    friend class DxuiPopupMenu;
    friend class DxuiPopupMenuPainter;

    DxuiPopupMenu  & m_menu;

    bool  m_hasGutter    = false;
    bool  m_hasIcons     = false;
    int   m_labelLeftPx  = 0;
    int   m_accelLeftPx  = 0;
    int   m_accelWidthPx = 0;
    int   m_maxHeightPx  = 0;
    int   m_viewportPx   = 0;
    int   m_scrollRow    = 0;
};