#include "Pch.h"

#include "CassoExplorer/CassoExplorerInitializeCardDialog.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerInitializeCardPanel::Init (DxuiLabel & question, CardLayoutBar & bar, DxuiLabel & contents, DxuiLabel & warning)
{
    m_question = &question;
    m_bar      = &bar;
    m_contents = &contents;
    m_warning  = &warning;

    Adopt (question);
    Adopt (bar);
    Adopt (contents);
    Adopt (warning);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerInitializeCardPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);

    m_scaler = scaler;

    Arrange (boundsDip, scaler, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardPanel::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerInitializeCardPanel::OnMouse (const DxuiMouseEvent & ev)
{
    return m_bar->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardPanel::TryMeasureHeightPx
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerInitializeCardPanel::TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx)
{
    RECT  bounds = { 0, 0, widthPx, 0 };



    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);

    heightPx = Arrange (bounds, m_scaler, false);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardPanel::GetLabelHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerInitializeCardPanel::GetLabelHeightPx (const DxuiLabel & label, int widthPx, const DxuiDpiScaler & scaler)
{
    HRESULT  hr       = S_OK;
    float    widthDip = (float) widthPx * (float) DxuiDpiScaler::kBaseDpi / (float) scaler.GetDpi();
    float    textDip  = 0.0f;
    int      heightPx = scaler.ToPx (ExplorerMetrics::kStackLabelHeightDip);



    hr = label.MeasureWrappedDip (widthDip, textDip);

    if (SUCCEEDED (hr))
    {
        heightPx = std::max (heightPx, scaler.ToPx ((int) std::ceil (textDip)));
    }

    return heightPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardPanel::Arrange
//
//  Returns the height of the stack.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerInitializeCardPanel::Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place)
{
    int  width    = bounds.right - bounds.left;
    int  gap      = scaler.ToPx (kSectionGapDip);
    int  top      = bounds.top;
    int  question = GetLabelHeightPx (*m_question, width, scaler);
    int  bar      = 0;
    int  contents = GetLabelHeightPx (*m_contents, width, scaler);
    int  warning  = GetLabelHeightPx (*m_warning,  width, scaler);



    if (place)
    {
        m_question->Layout (RECT { bounds.left, top, bounds.right, top + question }, scaler);
    }

    top += question + gap;

    bar = m_bar->MeasureHeightPx (width, scaler);

    if (place)
    {
        m_bar->Layout (RECT { bounds.left, top, bounds.right, top + bar }, scaler);
    }

    top += bar + gap;

    if (place)
    {
        m_contents->Layout (RECT { bounds.left, top, bounds.right, top + contents }, scaler);
        m_warning->Layout  (RECT { bounds.left, top + contents + gap, bounds.right, top + contents + gap + warning }, scaler);
    }

    top += contents + gap + warning;

    return top - bounds.top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardDialog::OnCreate
//
//  The erase button reads the chosen position before the dialog ends.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerInitializeCardDialog::OnCreate()
{
    DxuiButton  * erase  = nullptr;
    DxuiButton  * cancel = nullptr;



    m_question.SetText (m_questionText);
    ExplorerTextStyles::Apply (m_question, ExplorerTextStyles::kFormLabel);
    m_question.SetFontWeight (DxuiFontWeight::SemiBold);

    m_bar.SetPosition (m_model.GetDefaultPosition());
    m_bar.SetOnChange ([this] (int) { Invalidate(); });

    m_contents.SetText (m_contentsText);
    ExplorerTextStyles::Apply (m_contents, ExplorerTextStyles::kFormLabel);

    m_warning.SetText (L"ALL DATA ON THIS CARD WILL BE ERASED AND CANNOT BE RECOVERED");
    m_warning.SetTextRole (DxuiTextRole::Error);
    m_warning.SetFontWeight (DxuiFontWeight::SemiBold);

    m_body = CreateDialogContent<CassoExplorerInitializeCardPanel>();
    m_body->Init (m_question, m_bar, m_contents, m_warning);

    erase  = AddDialogButton (L"Erase and initialize", IDOK);
    cancel = AddDialogButton (L"Cancel", IDCANCEL);

    erase->SetVariant (DxuiButton::Variant::Danger);
    erase->SetOnClick ([this]()
    {
        m_layout    = m_model.GetLayout (m_bar.GetPosition());
        m_confirmed = true;

        EndDialog (IDOK);
    });

    SetInitialFocus (cancel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardDialog::OnDialogTick
//
//  The FAT32 slot grows or shrinks a step each tick, and the window is drawn
//  again while it does.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerInitializeCardDialog::OnDialogTick()
{
    if (m_bar.AdvanceAnimation ((int64_t) GetTickCount64()))
    {
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardDialog::Confirm
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerInitializeCardDialog::Confirm (HWND                       owner,
                                                 const IDxuiTheme         * theme,
                                                 const std::wstring       & question,
                                                 const CardLayoutModel    & model,
                                                 const std::wstring       & contents,
                                                 uint64_t                 & outLayout)
{
    HRESULT                            hr = S_OK;
    CassoExplorerInitializeCardDialog  dialog (model);
    DxuiWindow::CreateParams           params;



    outLayout = 0;

    if (model.GetPositionCount() == 0)
    {
        return false;
    }

    dialog.m_questionText = question;
    dialog.m_contentsText = contents;

    params.title                    = L"Initialize card for Apple";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 600, 560 };
    params.minSizeDip               = { 480, 400 };
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

    outLayout = dialog.m_layout;

    return dialog.m_confirmed;
}
