#include "Pch.h"

#include "CassoExplorer/CassoExplorerElevationDialog.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerElevationPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerElevationPanel::Init (DxuiLabel & text, DxuiCheckbox & dontShow)
{
    m_text     = &text;
    m_dontShow = &dontShow;

    Adopt (text);
    Adopt (dontShow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerElevationPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerElevationPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);

    m_scaler = scaler;

    Arrange (boundsDip, scaler, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerElevationPanel::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerElevationPanel::OnMouse (const DxuiMouseEvent & ev)
{
    return m_dontShow->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerElevationPanel::TryMeasureHeightPx
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerElevationPanel::TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx)
{
    RECT  bounds = { 0, 0, widthPx, 0 };



    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);

    heightPx = Arrange (bounds, m_scaler, false);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerElevationPanel::Arrange
//
//  The explanation at its wrapped height, then the checkbox. Returns the
//  height used.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerElevationPanel::Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place)
{
    HRESULT  hr       = S_OK;
    int      width    = bounds.right - bounds.left;
    float    widthDip = (float) width * (float) DxuiDpiScaler::kBaseDpi / (float) scaler.GetDpi();
    float    textDip  = 0.0f;
    int      text     = scaler.ToPx (ExplorerMetrics::kStackLabelHeightDip);
    int      field    = scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);
    int      gap      = scaler.ToPx (kSectionGapDip);



    hr = m_text->MeasureWrappedDip (widthDip, textDip);

    if (SUCCEEDED (hr))
    {
        text = std::max (text, scaler.ToPx ((int) std::ceil (textDip)));
    }

    if (place)
    {
        m_text->Layout     (RECT { bounds.left, bounds.top, bounds.right, bounds.top + text }, scaler);
        m_dontShow->Layout (RECT { bounds.left, bounds.top + text + gap, bounds.right, bounds.top + text + gap + field }, scaler);
    }

    return text + gap + field;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerElevationDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerElevationDialog::OnCreate()
{
    DxuiButton  * ok = nullptr;



    m_text.SetText (std::format (L"Reading and writing {} requires administrator access.\r\n\r\n"
                                 L"Windows opens a drive's blocks only to a program running as administrator, so Casso Explorer "
                                 L"starts a separate program, Casso Storage Card Broker, with that access. The broker opens "
                                 L"storage cards only, never a hard drive, and writes only inside a card's Apple partitions. "
                                 L"It closes when Casso Explorer closes.\r\n\r\n"
                                 L"When you click OK, Windows displays a prompt for permission.", m_card));
    ExplorerTextStyles::Apply (m_text, ExplorerTextStyles::kFormLabel);

    m_dontShow.SetLabel (L"Don't show this again");

    m_body = CreateDialogContent<CassoExplorerElevationPanel>();
    m_body->Init (m_text, m_dontShow);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        m_confirmed = true;

        EndDialog (IDOK);
    });

    SetInitialFocus (ok);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerElevationDialog::Confirm
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerElevationDialog::Confirm (HWND owner, const IDxuiTheme * theme, const std::wstring & card, bool & outDontShow)
{
    HRESULT                       hr = S_OK;
    CassoExplorerElevationDialog  dialog;
    DxuiWindow::CreateParams      params;



    outDontShow = false;

    dialog.m_card = card;

    params.title                    = L"Administrator access";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 520, 300 };
    params.minSizeDip               = { 420, 240 };
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
    dialog.ShowModalDialog (IDCANCEL);

    outDontShow = dialog.m_dontShow.IsChecked();

    return dialog.m_confirmed;
}
