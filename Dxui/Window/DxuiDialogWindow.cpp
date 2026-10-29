#include "Pch.h"

#include "DxuiDialogWindow.h"

#include "Core/DxuiPanel.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "Widgets/DxuiButton.h"
#include "Window/DxuiButtonRow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetDialogContentOwned
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogWindow::SetDialogContentOwned (std::unique_ptr<DxuiPanel> content)
{
    HRESULT  hr = S_OK;



    CBRA (content != nullptr);

    m_contentOwned = std::move (content);
    m_content      = m_contentOwned.get();
    Adopt (*m_content);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddDialogButton
//
////////////////////////////////////////////////////////////////////////////////

DxuiButton * DxuiDialogWindow::AddDialogButton (const std::wstring &  label,
                                                int                   commandId,
                                                DxuiButtonRow::Anchor anchor)
{
    DxuiButton *  button = CreateChild<DxuiButton> (label);



    button->SetCommandId (commandId);
    m_dialogButtons.push_back ({ button, commandId, anchor });

    return button;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  The host lays this window (content root) out in physical pixels, below
//  its own caption. Reserve a fixed-height bottom strip for the buttons,
//  inset and fill the remainder with the content, then place the buttons:
//  the primary group is right-aligned in the canonical Win32 order (OK,
//  Cancel, Apply, ...), while Anchor::Left buttons (e.g. "Browse...") pin
//  to the bottom-left. All share the standard DxuiButtonRow metrics.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogWindow::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int   pad     = scaler.ToPx (DxuiButtonRow::kEdgePadDip);
    int   rowH    = scaler.ToPx (DxuiButtonRow::kRowHeightDip);
    bool  hasRow  = !m_dialogButtons.empty();
    RECT  content = boundsPx;



    std::vector<ButtonEntry *>  right;
    std::vector<ButtonEntry *>  left;
    size_t                      i = 0;


    SetBounds (boundsPx);

    content.bottom -= hasRow ? rowH : 0;
    content.left   += pad;
    content.top    += pad;
    content.right  -= pad;
    content.bottom -= pad;

    if (m_content != nullptr)
    {
        m_content->Layout (content, scaler);
    }

    // Split by anchor; order the right group into the canonical button order
    // (stable, so like-ranked / synthetic ids keep registration order).
    for (ButtonEntry & e : m_dialogButtons)
    {
        (e.anchor == DxuiButtonRow::Anchor::Left ? left : right).push_back (&e);
    }

    std::stable_sort (right.begin(), right.end(),
                      [] (const ButtonEntry * a, const ButtonEntry * b)
                      {
                          return DxuiButtonRow::GetStandardRank (a->commandId) <
                                 DxuiButtonRow::GetStandardRank (b->commandId);
                      });

    {
        std::vector<int>   rWidths (right.size(), DxuiButtonRow::kButtonWidthDip);
        std::vector<int>   lWidths (left.size(),  DxuiButtonRow::kButtonWidthDip);

        // Size each button to its own label. A fixed width clipped anything
        // longer than a short verb.
        for (i = 0; i < right.size(); ++i)
        {
            rWidths[i] = DxuiButtonRow::GetWidthForLabel (right[i]->button->GetAccessibleName());
        }

        for (i = 0; i < left.size(); ++i)
        {
            lWidths[i] = DxuiButtonRow::GetWidthForLabel (left[i]->button->GetAccessibleName());
        }

        std::vector<RECT>  rRects  (right.size());
        std::vector<RECT>  lRects  (left.size());

        DxuiButtonRow::LayoutRightGroup (boundsPx, scaler, rWidths, rRects);
        DxuiButtonRow::LayoutLeftGroup  (boundsPx, scaler, lWidths, lRects);

        for (i = 0; i < right.size(); ++i)
        {
            right[i]->button->Layout (rRects[i]);
            right[i]->button->SetDpi  (scaler.GetDpi());
        }

        for (i = 0; i < left.size(); ++i)
        {
            left[i]->button->Layout (lRects[i]);
            left[i]->button->SetDpi  (scaler.GetDpi());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogWindow::OnDialogShowing
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogWindow::OnDialogShowing()
{
    HRESULT  hr = FitHeightToContent();



    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogWindow::FitHeightToContent
//
//  The caller's size came from an estimate made before the window had a
//  renderer, and an estimate that runs short clips the text. Here the
//  content measures itself at the width it was laid out at, and the window
//  grows or shrinks by the difference, keeping its center where the caller
//  put it. The work area's height is the ceiling.
//
//  The first resize can also change the window's frame, which takes width and
//  height from the content on top of the difference. A second pass measures
//  at the width the content has after that and closes the gap.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDialogWindow::FitHeightToContent()
{
    static constexpr int    s_kMaxPasses = 3;

    HRESULT                 hr        = S_OK;
    IDxuiMeasuredContent  * measured  = dynamic_cast<IDxuiMeasuredContent *> (m_content);
    IDxuiTextRenderer     * text      = GetTextRenderer();
    const IDxuiTheme      * theme     = GetTheme();
    HWND                    hwnd      = GetHwnd();
    RECT                    content   = {};
    RECT                    window    = {};
    int                     needPx    = 0;
    int                     deltaPx   = 0;
    int                     heightPx  = 0;
    int                     top       = 0;
    int                     pass      = 0;
    bool                    ok        = false;
    bool                    atCeiling = false;
    HMONITOR                monitor   = nullptr;
    MONITORINFO             mi        = { sizeof (mi) };



    BAIL_OUT_IF (measured == nullptr, S_OK);
    CBR (text != nullptr && theme != nullptr && hwnd != nullptr);

    for (pass = 0; pass < s_kMaxPasses && !atCeiling; pass++)
    {
        content = m_content->GetBounds();
        ok      = measured->TryMeasureHeightPx (*text, *theme, content.right - content.left, needPx);
        CBR (ok);

        deltaPx = needPx - (content.bottom - content.top);
        BAIL_OUT_IF (deltaPx == 0, S_OK);

        ok = GetWindowRect (hwnd, &window) != FALSE;
        CBR (ok);

        heightPx = (window.bottom - window.top) + deltaPx;
        top      = window.top - deltaPx / 2;
        monitor  = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);

        if (monitor != nullptr && GetMonitorInfoW (monitor, &mi))
        {
            atCeiling = heightPx >= (int) (mi.rcWork.bottom - mi.rcWork.top);
            heightPx  = std::min (heightPx, (int) (mi.rcWork.bottom - mi.rcWork.top));
            top       = std::clamp (top, (int) mi.rcWork.top, (int) mi.rcWork.bottom - heightPx);
        }

        ok = SetWindowPos (hwnd, nullptr, window.left, top, window.right - window.left, heightPx,
                           SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
        CBR (ok);
    }

Error:
    return hr;
}
