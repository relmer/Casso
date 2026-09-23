#include "Pch.h"

#include "CassoExplorer/CassoExplorerOptionsDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsPanel::Init (DxuiLabel & namingLabel, DxuiComboBox & naming)
{
    m_namingLabel = &namingLabel;
    m_naming      = &naming;

    Adopt (namingLabel);
    Adopt (naming);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  labelBottom = boundsDip.top + scaler.ToPx (kLabelHeightDip);
    int  fieldTop    = labelBottom + scaler.ToPx (kGapDip);



    SetBounds (boundsDip);

    m_namingLabel->Layout (RECT { boundsDip.left, boundsDip.top, boundsDip.right, labelBottom }, scaler);
    m_naming->Layout      (RECT { boundsDip.left, fieldTop, boundsDip.right, fieldTop + scaler.ToPx (kFieldHeightDip) }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsPanel::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerOptionsPanel::OnMouse (const DxuiMouseEvent & ev)
{
    return m_naming->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::GetNamingLabels
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassoExplorerOptionsDialog::GetNamingLabels()
{
    return { L"Descriptive (HELLO.Applesoft BASIC.txt)", L"CiderPress (HELLO#FC0801)", L"AppleSingle (HELLO.as)" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::OnCreate
//
//  OK keeps its own click so the choices are read before the dialog ends.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsDialog::OnCreate()
{
    DxuiButton *  ok = nullptr;



    m_namingLabel.SetText      (L"Host file names");
    m_namingLabel.SetTextRole  (DxuiTextRole::Body);
    m_namingLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_naming.SetPopupHost (GetPopupHost());
    m_naming.SetItems     (GetNamingLabels());
    m_naming.SetSelected  (m_choices.hostNaming);

    m_body = CreateDialogContent<CassoExplorerOptionsPanel>();
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
//  CassoExplorerOptionsDialog::Ask
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerOptionsDialog::Ask (HWND owner, const IDxuiTheme * theme, Choices & inOutChoices)
{
    HRESULT                     hr     = S_OK;
    CassoExplorerOptionsDialog  dialog;
    DxuiWindow::CreateParams    params;



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
