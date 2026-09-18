#include "Pch.h"

#include "Cassque/CassquePromptDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePromptPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassquePromptPanel::Init (DxuiLabel & label, DxuiTextInput & input)
{
    m_label = &label;
    m_input = &input;

    Adopt (label);
    Adopt (input);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePromptPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassquePromptPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  labelBottom = boundsDip.top + scaler.ToPx (kLabelHeightDip);
    int  inputTop    = labelBottom + scaler.ToPx (kGapDip);



    SetBounds (boundsDip);

    m_label->Layout (RECT { boundsDip.left, boundsDip.top, boundsDip.right, labelBottom }, scaler);
    m_input->Layout (RECT { boundsDip.left, inputTop, boundsDip.right, inputTop + scaler.ToPx (kInputHeightDip) }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePromptPanel::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CassquePromptPanel::OnMouse (const DxuiMouseEvent & ev)
{
    return m_input->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePromptDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void CassquePromptDialog::Configure (const std::wstring & label, const std::wstring & initialText, const IDxuiTheme * theme, size_t maxLength)
{
    m_labelText = label;
    m_text      = initialText;
    m_theme     = theme;
    m_maxLength = maxLength;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePromptDialog::OnCreate
//
//  OK keeps its own click so the text is read before the dialog ends.
//
////////////////////////////////////////////////////////////////////////////////

void CassquePromptDialog::OnCreate()
{
    DxuiButton *  ok = nullptr;



    m_label.SetText      (m_labelText);
    m_label.SetTextRole  (DxuiTextRole::Body);
    m_label.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_input.SetTheme        (m_theme);
    m_input.SetHwnd         (GetHwnd());
    m_input.SetTextRenderer (GetTextRenderer());
    m_input.SetMaxLength    (m_maxLength);
    m_input.SetText         (m_text);

    m_body = CreateDialogContent<CassquePromptPanel>();
    m_body->Init (m_label, m_input);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        m_text      = m_input.GetText();
        m_confirmed = true;
        EndDialog (IDOK);
    });

    SetInitialFocus (&m_input);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePromptDialog::Ask
//
////////////////////////////////////////////////////////////////////////////////

bool CassquePromptDialog::Ask (HWND                  owner,
                               const IDxuiTheme    * theme,
                               const std::wstring  & title,
                               const std::wstring  & label,
                               const std::wstring  & initialText,
                               size_t                maxLength,
                               std::wstring        & outText)
{
    HRESULT                   hr     = S_OK;
    CassquePromptDialog       dialog;
    DxuiWindow::CreateParams  params;



    dialog.Configure (label, initialText, theme, maxLength);

    params.title                    = title;
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 420, 180 };
    params.minSizeDip               = { 320, 180 };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return false;
    }

    dialog.SetTheme (theme);
    dialog.ShowModalDialog (IDOK);

    outText = dialog.GetText();

    return dialog.IsConfirmed() && !outText.empty();
}
