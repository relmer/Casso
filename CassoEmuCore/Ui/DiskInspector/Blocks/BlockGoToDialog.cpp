#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockGoToDialog.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"





static constexpr int     s_kPadDip       = 16;
static constexpr int     s_kRowDip       = 28;
static constexpr int     s_kGapDip       = 8;
static constexpr int     s_kLabelDip     = 110;
static constexpr size_t  s_kMaxChars     = 10;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockGoToDialog::Body
//
//  The label and the number on one row, and the error under them.
//
////////////////////////////////////////////////////////////////////////////////

class BlockGoToDialog::Body : public DxuiPanel
{
public:
    Body (DxuiLabel & label, DxuiTextInput & input, DxuiLabel & error) :
        m_label (label),
        m_input (input),
        m_error (error)
    {
        Adopt (m_label);
        Adopt (m_input);
        Adopt (m_error);
    }

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override
    {
        int  pad   = scaler.ToPx (s_kPadDip);
        int  row   = scaler.ToPx (s_kRowDip);
        int  gap   = scaler.ToPx (s_kGapDip);
        int  label = scaler.ToPx (s_kLabelDip);
        int  top   = boundsDip.top + pad;

        SetBounds (boundsDip);
        m_label.Layout ({ boundsDip.left + pad, top, boundsDip.left + pad + label, top + row }, scaler);
        m_input.Layout ({ boundsDip.left + pad + label + gap, top, boundsDip.right - pad, top + row }, scaler);
        m_error.Layout ({ boundsDip.left + pad, top + row + gap, boundsDip.right - pad, top + 2 * row + gap }, scaler);
    }

private:
    DxuiLabel      & m_label;
    DxuiTextInput  & m_input;
    DxuiLabel      & m_error;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockGoToDialog::Choose
//
////////////////////////////////////////////////////////////////////////////////

bool BlockGoToDialog::Choose (HWND owner, const IDxuiTheme * theme, uint32_t blockCount, uint32_t current, uint32_t & outBlock)
{
    HRESULT                   hr     = S_OK;
    BlockGoToDialog           dialog;
    DxuiWindow::CreateParams  params;
    int                       result = IDCANCEL;



    dialog.m_theme      = theme;
    dialog.m_blockCount = blockCount;
    dialog.m_current    = current;

    params.title          = L"Go to block";
    params.hInstance      = reinterpret_cast<HINSTANCE> (GetWindowLongPtrW (owner, GWLP_HINSTANCE));
    params.ownerHwnd      = owner;
    params.initialSizeDip = kSizeDip;
    params.minSizeDip     = kSizeDip;
    params.resizable      = false;
    params.placement      = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (theme);
    result = dialog.ShowModalDialog (IDOK);

    if (result == IDOK && dialog.m_isValid)
    {
        outBlock = dialog.m_block;
    }

Error:
    return SUCCEEDED (hr) && result == IDOK && dialog.m_isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockGoToDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void BlockGoToDialog::OnCreate()
{
    m_label.SetText      (L"Block number");
    m_label.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_error.SetTextRole  (DxuiTextRole::Muted);
    m_error.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_input.SetTheme        (m_theme);
    m_input.SetHwnd         (GetHwnd());
    m_input.SetTextRenderer (GetTextRenderer());
    m_input.SetMaxLength    (s_kMaxChars);
    m_input.SetText         (std::to_wstring (m_current));
    m_input.SetOnChange     ([this] (const std::wstring &) { Validate(); });

    m_body = CreateDialogContent<Body> (m_label, m_input, m_error);
    m_ok   = AddDialogButton (L"OK", IDOK);
    (void) AddDialogButton (L"Cancel", IDCANCEL);

    SetInitialFocus (&m_input);
    Validate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockGoToDialog::Validate
//
////////////////////////////////////////////////////////////////////////////////

void BlockGoToDialog::Validate()
{
    uint32_t  block    = 0;
    bool      isNumber = BlockInspectorPanel::TryParseBlock (m_input.GetText(), block);



    m_isValid = isNumber && block < m_blockCount;
    m_block   = block;

    m_error.SetText (m_isValid ? std::wstring (L"Decimal, or hex after $.")
                               : std::format (L"Enter a block from 0 to {}, in decimal or in hex after $.", m_blockCount == 0 ? 0 : m_blockCount - 1));

    if (m_ok != nullptr)
    {
        m_ok->SetEnabled (m_isValid);
    }
}
