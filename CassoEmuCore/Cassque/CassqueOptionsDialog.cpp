#include "Pch.h"

#include "Cassque/CassqueOptionsDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueOptionsPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassqueOptionsPanel::Init (DxuiLabel & namingLabel, DxuiComboBox & naming)
{
    m_namingLabel = &namingLabel;
    m_naming      = &naming;

    Adopt (namingLabel);
    Adopt (naming);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueOptionsPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassqueOptionsPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  labelBottom = boundsDip.top + scaler.ToPx (kLabelHeightDip);
    int  fieldTop    = labelBottom + scaler.ToPx (kGapDip);



    SetBounds (boundsDip);

    m_namingLabel->Layout (RECT { boundsDip.left, boundsDip.top, boundsDip.right, labelBottom }, scaler);
    m_naming->Layout      (RECT { boundsDip.left, fieldTop, boundsDip.right, fieldTop + scaler.ToPx (kFieldHeightDip) }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueOptionsPanel::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueOptionsPanel::OnMouse (const DxuiMouseEvent & ev)
{
    return m_naming->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueOptionsDialog::GetNamingLabels
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassqueOptionsDialog::GetNamingLabels()
{
    return { L"Descriptive (HELLO.Applesoft BASIC.txt)", L"CiderPress (HELLO#FC0801)" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueOptionsDialog::OnCreate
//
//  OK keeps its own click so the choices are read before the dialog ends.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueOptionsDialog::OnCreate()
{
    DxuiButton *  ok = nullptr;



    m_namingLabel.SetText      (L"Host file names");
    m_namingLabel.SetTextRole  (DxuiTextRole::Body);
    m_namingLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_naming.SetPopupHost (GetPopupHost());
    m_naming.SetItems     (GetNamingLabels());
    m_naming.SetSelected  (m_choices.hostNaming);

    m_body = CreateDialogContent<CassqueOptionsPanel>();
    m_body->Init (m_namingLabel, m_naming);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        m_choices.hostNaming = m_naming.GetSelectedIndex();
        m_confirmed          = true;
        EndDialog (IDOK);
    });

    SetInitialFocus (&m_naming);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueOptionsDialog::Ask
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueOptionsDialog::Ask (HWND owner, const IDxuiTheme * theme, Choices & inOutChoices)
{
    HRESULT                   hr     = S_OK;
    CassqueOptionsDialog      dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme   = theme;
    dialog.m_choices = inOutChoices;

    params.title                    = L"Options";
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

    if (dialog.m_confirmed)
    {
        inOutChoices = dialog.m_choices;
    }

    return dialog.m_confirmed;
}
