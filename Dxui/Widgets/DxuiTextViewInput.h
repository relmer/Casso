#pragma once

#include "Pch.h"
#include "Core/DxuiEvents.h"
#include "Core/IDxuiControl.h"
#include "DxuiTextPosition.h"


class DxuiScrollbar;
class DxuiTextLayout;
class DxuiTextViewSelection;
class IDxuiTextInputHost;
class IDxuiTextScroller;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewInput
//
//  A text view's mouse and keys: the wheel and the keys scroll, a press and
//  a drag select by character, a double-click by word and a triple-click
//  all of it, and a click on a link follows it. Copy and Select all are the
//  view's standard commands.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextViewInput
{
public:
    using Position = DxuiTextPosition;

    static constexpr int  kWheelLines   = 3;
    static constexpr int  kWheelColumns = 8;

    DxuiTextViewInput (IDxuiTextInputHost           & host,
                       const DxuiTextLayout         & layout,
                       DxuiTextViewSelection        & selection,
                       IDxuiTextScroller            & scroller,
                       DxuiScrollbar                & vertBar,
                       DxuiScrollbar                & horzBar) : m_host (host), m_layout (layout), m_selection (selection), m_scroller (scroller), m_vertBar (vertBar), m_horzBar (horzBar) {}

    bool  OnMouse       (const DxuiMouseEvent & ev);
    bool  OnKey         (const DxuiKeyEvent & ev);
    bool  QueryCommand  (DxuiStandardCommand command, bool & outEnabled) const;
    bool  InvokeCommand (DxuiStandardCommand command);

    bool  IsDragging    () const { return m_dragging; }

    //  The rows replaced, and rows dropped from the front of the text.
    void  Reset         ()            { m_dragging = false; m_pressRow = -1; }
    void  DropFront     (int dropped) { m_pressRow = (m_pressRow >= dropped) ? m_pressRow - dropped : -1; }

private:
    bool  OnWheel     (const DxuiMouseEvent & ev);
    bool  OnPress     (const DxuiMouseEvent & ev);
    bool  OnDragMove  (POINT point);
    bool  OnRelease   (POINT point);

    IDxuiTextInputHost     & m_host;
    const DxuiTextLayout   & m_layout;
    DxuiTextViewSelection  & m_selection;
    IDxuiTextScroller      & m_scroller;
    DxuiScrollbar          & m_vertBar;
    DxuiScrollbar          & m_horzBar;
    Position                 m_lastClick;
    int64_t                  m_lastClickMs  = 0;
    int                      m_clicks       = 0;      // the clicks in a row on one spot, each within the double-click time
    float                    m_wheelLines   = 0.0f;   // a touchpad's part notches, until they make a line
    float                    m_wheelColumns = 0.0f;   // and until they make a column
    bool                     m_dragging     = false;
    bool                     m_wordDrag     = false;
    Position                 m_wordFirst;
    Position                 m_wordLast;
    int                      m_pressRow     = -1;     // the link a press began on, -1 for none
    int                      m_pressSpan    = -1;
};
