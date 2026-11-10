#include "Pch.h"

#include "CassoExplorer/CassoExplorerInitializeCardDialog.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerInitializeCardPanel::Init (DxuiLabel & question, DxuiRadioGroup & choices, DxuiLabel & contents, DxuiLabel & warning)
{
    m_question = &question;
    m_choices  = &choices;
    m_contents = &contents;
    m_warning  = &warning;

    Adopt (question);
    Adopt (choices);
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
    return m_choices->OnMouse (ev);
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
//  Returns the height of the stack. Each configuration is as tall as its
//  description wraps.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerInitializeCardPanel::Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place)
{
    int                           width    = bounds.right - bounds.left;
    int                           gap      = scaler.ToPx (kSectionGapDip);
    int                           top      = bounds.top;
    int                           question = GetLabelHeightPx (*m_question, width, scaler);
    int                           contents = GetLabelHeightPx (*m_contents, width, scaler);
    int                           warning  = GetLabelHeightPx (*m_warning,  width, scaler);
    int                           choices  = 0;
    std::vector<DxuiRadioOption>  options  = m_choices->GetOptions();



    if (place)
    {
        m_question->Layout (RECT { bounds.left, top, bounds.right, top + question }, scaler);
    }

    top += question + gap;
    choices = top;

    for (DxuiRadioOption & option : options)
    {
        int  optionH = DxuiRadioGroup::MeasureOptionHeightPx (option, width, scaler) + scaler.ToPx (kOptionGapDip);

        option.rect = RECT { bounds.left, top, bounds.right, top + optionH };
        top        += optionH;
    }

    if (place)
    {
        m_choices->SetOptions (std::move (options));
        m_choices->Layout (RECT { bounds.left, choices, bounds.right, top }, scaler);
    }

    top += gap;

    if (place)
    {
        m_contents->Layout (RECT { bounds.left, top, bounds.right, top + contents }, scaler);
    }

    top += contents + gap;

    if (place)
    {
        m_warning->Layout (RECT { bounds.left, top, bounds.right, top + warning }, scaler);
    }

    top += warning;

    return top - bounds.top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardDialog::OnCreate
//
//  The erase button reads the chosen configuration before the dialog ends.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerInitializeCardDialog::OnCreate()
{
    DxuiButton                    * erase  = nullptr;
    DxuiButton                    * cancel = nullptr;
    std::vector<DxuiRadioOption>    options;



    m_question.SetText (m_questionText);
    ExplorerTextStyles::Apply (m_question, ExplorerTextStyles::kFormLabel);
    m_question.SetFontWeight (DxuiFontWeight::SemiBold);

    for (const InitializeCardChoice & choice : m_choiceList)
    {
        options.push_back (DxuiRadioOption { {}, choice.label, choice.description, {} });
    }

    m_choices.SetOptions (std::move (options));
    m_choices.SetSelected ((int) m_default);

    m_contents.SetText (m_contentsText);
    ExplorerTextStyles::Apply (m_contents, ExplorerTextStyles::kFormLabel);

    m_warning.SetText (L"ALL DATA ON THIS CARD WILL BE ERASED AND CANNOT BE RECOVERED");
    m_warning.SetTextRole (DxuiTextRole::Error);
    m_warning.SetFontWeight (DxuiFontWeight::SemiBold);

    m_body = CreateDialogContent<CassoExplorerInitializeCardPanel>();
    m_body->Init (m_question, m_choices, m_contents, m_warning);

    erase  = AddDialogButton (L"Erase and initialize", IDOK);
    cancel = AddDialogButton (L"Cancel", IDCANCEL);

    erase->SetVariant (DxuiButton::Variant::Danger);
    erase->SetOnClick ([this]()
    {
        int  selected = m_choices.GetSelected();

        if (selected >= 0 && (size_t) selected < m_choiceList.size())
        {
            m_layout    = m_choiceList[(size_t) selected].layout;
            m_confirmed = true;
        }

        EndDialog (IDOK);
    });

    SetInitialFocus (cancel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardDialog::Confirm
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerInitializeCardDialog::Confirm (HWND                                       owner,
                                                 const IDxuiTheme                         * theme,
                                                 const std::wstring                       & question,
                                                 const std::vector<InitializeCardChoice>  & choices,
                                                 size_t                                     defaultChoice,
                                                 const std::wstring                       & contents,
                                                 uint64_t                                 & outLayout)
{
    HRESULT                            hr = S_OK;
    CassoExplorerInitializeCardDialog  dialog;
    DxuiWindow::CreateParams           params;



    outLayout = 0;

    if (choices.empty())
    {
        return false;
    }

    dialog.m_questionText = question;
    dialog.m_contentsText = contents;
    dialog.m_choiceList   = choices;
    dialog.m_default      = std::min (defaultChoice, choices.size() - 1);

    params.title                    = L"Initialize card for Apple";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 520, 420 };
    params.minSizeDip               = { 420, 300 };
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
