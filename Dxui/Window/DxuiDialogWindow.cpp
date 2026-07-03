#include "Pch.h"

#include "DxuiDialogWindow.h"

#include "Core/DxuiPanel.h"
#include "Widgets/DxuiButton.h"


static constexpr int  s_kButtonRowHeightDip  = 44;
static constexpr int  s_kButtonWidthDip      = 96;
static constexpr int  s_kButtonHeightDip     = 28;
static constexpr int  s_kButtonGapDip        = 8;
static constexpr int  s_kButtonRowEdgePadDip = 16;
static constexpr int  s_kContentPadDip       = 16;




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

DxuiButton * DxuiDialogWindow::AddDialogButton (const std::wstring & label, int commandId)
{
    DxuiButton *  button = CreateChild<DxuiButton> (label);


    button->SetCommandId (commandId);
    m_dialogButtons.push_back (button);

    return button;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  The host lays this window (content root) out in physical pixels, below
//  its own caption. Reserve a fixed-height bottom strip for the buttons,
//  inset and fill the remainder with the content, then size + right-align
//  the fixed-width buttons in registration order (first leftmost).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogWindow::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int   pad      = scaler.Px (s_kContentPadDip);
    int   rowH     = scaler.Px (s_kButtonRowHeightDip);
    int   btnW     = scaler.Px (s_kButtonWidthDip);
    int   btnH     = scaler.Px (s_kButtonHeightDip);
    int   gapPx    = scaler.Px (s_kButtonGapDip);
    int   edgePx   = scaler.Px (s_kButtonRowEdgePadDip);
    bool  hasRow   = !m_dialogButtons.empty();
    int   count    = (int) m_dialogButtons.size();
    int   total    = (count * btnW) + ((count - 1) * gapPx);
    int   x        = boundsPx.right - edgePx - total;
    int   y        = boundsPx.bottom - pad - btnH;
    RECT  content  = boundsPx;
    int   i        = 0;



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

    for (i = 0; i < count; ++i)
    {
        RECT  b = { x, y, x + btnW, y + btnH };

        m_dialogButtons[(size_t) i]->Layout (b);
        m_dialogButtons[(size_t) i]->SetDpi  (scaler.Dpi());
        x += btnW + gapPx;
    }
}
