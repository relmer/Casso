#pragma once

#include "Pch.h"

#include "Core/DxuiDpiScaler.h"


class DxuiHwndSource;
class DxuiPopupHost;
class IDxuiTheme;
class IDxuiPainter;
class IDxuiTextRenderer;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSuggestionList
//
//  The list that drops under a text box as it is typed in -- an address bar's
//  history, then its completions -- as Explorer's does.
//
//  THE KEYBOARD STAYS WITH THE BOX. The list draws in a popup that is never
//  activated, so typing goes on while it shows. The box's owner passes Up,
//  Down and Escape here while the list is open; the pointer highlights and
//  picks in the popup itself.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiSuggestionList
{
public:
    using PickFn = std::function<void (const std::wstring & text)>;

    void  SetPopupHost (DxuiHwndSource * host)  { m_popupHost = host; }
    void  SetTheme     (const IDxuiTheme * theme) { m_theme = theme; }
    void  SetOnPick    (PickFn fn)              { m_onPick = std::move (fn); }

    //  Shows `items` under `anchorClientPx`, a rect in the owner's client
    //  pixels, as wide as it; replaces the items if already open. No items
    //  closes the list.
    void  Show  (const RECT & anchorClientPx, std::vector<std::wstring> items);
    void  Close ();

    bool  IsOpen () const { return m_popup != nullptr; }

    //  Moves the highlight a step, wrapping past either end to no highlight,
    //  and returns the highlighted text, or empty for none.
    std::wstring  MoveHighlight (int step);

    const std::vector<std::wstring> &  GetItems () const { return m_items; }

    static constexpr int  kMaxVisibleRows = 12;
    static constexpr int  kRowHeightDip   = 32;
    static constexpr int  kPadLeftDip     = 12;

private:
    void  Render      (IDxuiPainter & painter, IDxuiTextRenderer & text);
    void  OnMove      (POINT localPx);
    void  OnClick     (POINT localPx);
    void  OnWheel     (int delta);
    int   HitTestRow  (POINT localPx) const;
    void  Scroll      (int rows);

    DxuiHwndSource             * m_popupHost = nullptr;
    DxuiPopupHost              * m_popup     = nullptr;
    const IDxuiTheme           * m_theme     = nullptr;
    PickFn                       m_onPick;
    std::vector<std::wstring>    m_items;
    DxuiDpiScaler                m_scaler;
    int                          m_highlight = -1;
    int                          m_topRow    = 0;
    int                          m_widthPx   = 0;
};
