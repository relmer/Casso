#include "Pch.h"

#include "DxuiDialogWindow.h"

#include "Core/DxuiPanel.h"
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
    int   pad     = scaler.Px (DxuiButtonRow::kEdgePadDip);
    int   rowH    = scaler.Px (DxuiButtonRow::kRowHeightDip);
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
                          return DxuiButtonRow::StandardRank (a->commandId) <
                                 DxuiButtonRow::StandardRank (b->commandId);
                      });

    {
        std::vector<int>   rWidths (right.size(), DxuiButtonRow::kButtonWidthDip);
        std::vector<int>   lWidths (left.size(),  DxuiButtonRow::kButtonWidthDip);
        std::vector<RECT>  rRects  (right.size());
        std::vector<RECT>  lRects  (left.size());

        DxuiButtonRow::LayoutRightGroup (boundsPx, scaler, rWidths, rRects);
        DxuiButtonRow::LayoutLeftGroup  (boundsPx, scaler, lWidths, lRects);

        for (i = 0; i < right.size(); ++i)
        {
            right[i]->button->Layout (rRects[i]);
            right[i]->button->SetDpi  (scaler.Dpi());
        }
        for (i = 0; i < left.size(); ++i)
        {
            left[i]->button->Layout (lRects[i]);
            left[i]->button->SetDpi  (scaler.Dpi());
        }
    }
}
