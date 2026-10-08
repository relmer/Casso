#include "Pch.h"
#include "DxuiSuggestionList.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"
#include "Theme/IDxuiTheme.h"
#include "Theme/DxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSuggestionList::Show
//
//  An open list with as many rows as before takes the new items in place,
//  redrawn rather than closed and opened, so typing does not make it blink.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSuggestionList::Show (const RECT & anchorClientPx, std::vector<std::wstring> items)
{
    HRESULT                    hr       = S_OK;
    HWND                       owner    = nullptr;
    POINT                      tl       = {};
    POINT                      br       = {};
    DxuiPopupHost::ShowParams  params;
    int                        rows     = 0;
    bool                       acquired = false;



    if (items.empty() || m_popupHost == nullptr)
    {
        Close();
        return;
    }

    rows        = (std::min) ((int) items.size(), kMaxVisibleRows);
    m_highlight = -1;
    m_topRow    = 0;

    //  The same number of rows fits the popup already shown.
    if (m_popup != nullptr && rows == (std::min) ((int) m_items.size(), kMaxVisibleRows))
    {
        m_items = std::move (items);
        m_popup->MarkDirty();
        return;
    }

    m_items = std::move (items);
    owner   = m_popupHost->GetHwnd();
    m_scaler.SetDpi (GetDpiForWindow (owner));

    if (m_popup != nullptr)
    {
        m_popup->Close();
        m_popupHost->ReleasePopup (m_popup);
        m_popup = nullptr;
    }

    m_popup  = m_popupHost->AcquirePopup();
    acquired = m_popup != nullptr;
    BAIL_OUT_IF (!acquired, S_OK);

    tl = { anchorClientPx.left,  anchorClientPx.top };
    br = { anchorClientPx.right, anchorClientPx.bottom };
    ClientToScreen (owner, &tl);
    ClientToScreen (owner, &br);

    m_widthPx = br.x - tl.x;

    params.ownerHwnd        = owner;
    params.anchorRectScreen = { tl.x, tl.y, br.x, br.y };
    params.placement        = DxuiPopupPlacement::Below;
    params.flipIfOffscreen  = true;
    params.dismiss          = DxuiPopupDismiss::OnClickOutside;

    //  The mouse stays with the owner, so the box being typed in keeps its
    //  I-beam and its clicks; the owner closes the list when the edit ends.
    params.grabsCapture     = false;
    params.input            = DxuiPopupInput::Interactive;
    params.shadow           = true;
    params.sizeDip.cx       = MulDiv (br.x - tl.x, DxuiDpiScaler::kBaseDpi, (int) m_scaler.GetDpi());
    params.sizeDip.cy       = rows * kRowHeightDip;
    params.backgroundArgb   = (m_theme != nullptr) ? m_theme->BackgroundElevated() : DxuiPopupHost::kDefaultMenuBackgroundArgb;
    params.renderContent    = [this] (IDxuiPainter & p, IDxuiTextRenderer & t) { Render (p, t); };
    params.onMoveInside     = [this] (POINT localPx) { OnMove  (localPx); };
    params.onClickInside    = [this] (POINT localPx) { OnClick (localPx); };
    params.onWheel          = [this] (int delta)     { OnWheel (delta); };
    params.onClosed         = [this] ()
    {
        //  Closed from outside -- a click elsewhere -- hands the popup back.
        if (m_popup != nullptr && m_popupHost != nullptr)
        {
            DxuiPopupHost *  popup = m_popup;

            m_popup = nullptr;
            m_popupHost->ReleasePopup (popup);
        }
    };

    hr = m_popup->Show (std::move (params));
    CHR (hr);

Error:
    if (acquired && FAILED (hr))
    {
        m_popupHost->ReleasePopup (m_popup);
        m_popup = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSuggestionList::Close
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSuggestionList::Close()
{
    DxuiPopupHost *  popup = m_popup;



    m_popup     = nullptr;
    m_highlight = -1;

    if (popup != nullptr)
    {
        popup->Close();
        m_popupHost->ReleasePopup (popup);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSuggestionList::MoveHighlight
//
//  Past the last row, or above the first, the highlight leaves the list and
//  the box shows what was typed, as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiSuggestionList::MoveHighlight (int step)
{
    int  count = (int) m_items.size();



    if (count == 0)
    {
        return std::wstring();
    }

    m_highlight += step;

    if (m_highlight >= count)
    {
        m_highlight = -1;
    }
    else if (m_highlight < -1)
    {
        m_highlight = count - 1;
    }

    if (m_highlight >= 0 && m_highlight < m_topRow)
    {
        m_topRow = m_highlight;
    }
    else if (m_highlight >= m_topRow + kMaxVisibleRows)
    {
        m_topRow = m_highlight - kMaxVisibleRows + 1;
    }

    if (m_popup != nullptr)
    {
        m_popup->MarkDirty();
    }

    return (m_highlight >= 0) ? m_items[(size_t) m_highlight] : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSuggestionList::Render
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSuggestionList::Render (IDxuiPainter & painter, IDxuiTextRenderer & text)
{
    HRESULT   hr       = S_OK;
    int       rowPx    = m_scaler.ToPx (kRowHeightDip);
    float     padPx    = m_scaler.ToPxf ((float) kPadLeftDip);
    float     insetX   = m_scaler.ToPxf (4.0f);
    float     insetY   = m_scaler.ToPxf (2.0f);
    float     fontPx   = m_scaler.ToPxf (DxuiTheme::kBodySizeDip);
    int       width    = m_widthPx;
    uint32_t  fg       = (m_theme != nullptr) ? m_theme->Foreground()      : 0xFFFFFFFFu;
    uint32_t  hover    = (m_theme != nullptr) ? m_theme->HoverBackground() : 0x33FFFFFFu;
    int       i        = 0;



    if (m_popup == nullptr)
    {
        return;
    }

    for (i = m_topRow; i < (int) m_items.size() && i < m_topRow + kMaxVisibleRows; i++)
    {
        float  top = (float) ((i - m_topRow) * rowPx);

        if (i == m_highlight)
        {
            painter.FillRoundedRect (insetX, top + insetY, (float) width - insetX * 2.0f, (float) rowPx - insetY * 2.0f,
                                     m_scaler.ToPxf (DxuiTheme::kCornerRadiusDip), hover);
        }

        hr = text.DrawString (m_items[(size_t) i].c_str(), padPx, top, (float) width - padPx * 2.0f, (float) rowPx,
                              fg, fontPx, DxuiTheme::GetUiFace(),
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSuggestionList::HitTestRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiSuggestionList::HitTestRow (POINT localPx) const
{
    int  rowPx = m_scaler.ToPx (kRowHeightDip);
    int  row   = (rowPx > 0 && localPx.y >= 0) ? m_topRow + localPx.y / rowPx : -1;



    return (row >= 0 && row < (int) m_items.size()) ? row : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSuggestionList::OnMove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSuggestionList::OnMove (POINT localPx)
{
    int  row = HitTestRow (localPx);



    if (row != m_highlight && m_popup != nullptr)
    {
        m_highlight = row;
        m_popup->MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSuggestionList::OnClick
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSuggestionList::OnClick (POINT localPx)
{
    int           row  = HitTestRow (localPx);
    std::wstring  pick;



    if (row < 0)
    {
        return;
    }

    pick = m_items[(size_t) row];
    Close();

    if (m_onPick)
    {
        m_onPick (pick);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSuggestionList::OnWheel
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSuggestionList::OnWheel (int delta)
{
    Scroll (-(delta / WHEEL_DELTA) * 3);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSuggestionList::Scroll
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSuggestionList::Scroll (int rows)
{
    int  maxTop = (std::max) (0, (int) m_items.size() - kMaxVisibleRows);



    m_topRow = (std::max) (0, (std::min) (m_topRow + rows, maxTop));

    if (m_popup != nullptr)
    {
        m_popup->MarkDirty();
    }
}
