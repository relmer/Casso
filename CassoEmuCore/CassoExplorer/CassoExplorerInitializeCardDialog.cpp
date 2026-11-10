#include "Pch.h"

#include "CassoExplorer/CassoExplorerInitializeCardDialog.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerInitializeCardPanel::Init (DxuiLabel & question, DxuiComboBox & choices, DxuiLabel & description, const std::vector<std::wstring> & descriptions, DxuiLabel & contents, DxuiLabel & warning)
{
    m_question     = &question;
    m_choices      = &choices;
    m_description  = &description;
    m_descriptions = descriptions;
    m_contents     = &contents;
    m_warning      = &warning;

    Adopt (question);
    Adopt (choices);
    Adopt (description);
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
//  CassoExplorerInitializeCardPanel::GetDescriptionHeightPx
//
//  The tallest of the descriptions, measured with the label each in turn.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerInitializeCardPanel::GetDescriptionHeightPx (int widthPx, const DxuiDpiScaler & scaler)
{
    std::wstring  current  = m_description->GetText();
    int           heightPx = 0;



    for (const std::wstring & text : m_descriptions)
    {
        m_description->SetText (text);
        heightPx = std::max (heightPx, GetLabelHeightPx (*m_description, widthPx, scaler));
    }

    m_description->SetText (current);

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
    int  width       = bounds.right - bounds.left;
    int  gap         = scaler.ToPx (kSectionGapDip);
    int  nearGap     = scaler.ToPx (kGapDip);
    int  top         = bounds.top;
    int  question    = GetLabelHeightPx (*m_question, width, scaler);
    int  field       = scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);
    int  description = GetDescriptionHeightPx (width, scaler);
    int  contents    = GetLabelHeightPx (*m_contents, width, scaler);
    int  warning     = GetLabelHeightPx (*m_warning,  width, scaler);



    if (place)
    {
        m_question->Layout    (RECT { bounds.left, top, bounds.right, top + question }, scaler);
        m_choices->Layout     (RECT { bounds.left, top + question + gap, bounds.right, top + question + gap + field }, scaler);
        m_description->Layout (RECT { bounds.left, top + question + gap + field + nearGap, bounds.right, top + question + gap + field + nearGap + description }, scaler);
    }

    top += question + gap + field + nearGap + description + gap;

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
//  The erase button reads the chosen configuration before the dialog ends.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerInitializeCardDialog::OnCreate()
{
    DxuiButton                 * erase  = nullptr;
    DxuiButton                 * cancel = nullptr;
    std::vector<std::wstring>    labels;
    std::vector<std::wstring>    descriptions;



    m_question.SetText (m_questionText);
    ExplorerTextStyles::Apply (m_question, ExplorerTextStyles::kFormLabel);
    m_question.SetFontWeight (DxuiFontWeight::SemiBold);

    for (const InitializeCardChoice & choice : m_choiceList)
    {
        labels.push_back (choice.label);
        descriptions.push_back (choice.description);
    }

    m_choices.SetPopupHost (GetPopupHost());
    m_choices.SetItems (labels);
    m_choices.SetSelected ((int) m_default);
    m_choices.SetSelect ([this] (int index)
    {
        if (index >= 0 && (size_t) index < m_choiceList.size())
        {
            m_description.SetText (m_choiceList[(size_t) index].description);
            Invalidate();
        }
    });

    m_description.SetText (m_choiceList[m_default].description);
    ExplorerTextStyles::Apply (m_description, ExplorerTextStyles::kFormNote);

    m_contents.SetText (m_contentsText);
    ExplorerTextStyles::Apply (m_contents, ExplorerTextStyles::kFormLabel);

    m_warning.SetText (L"ALL DATA ON THIS CARD WILL BE ERASED AND CANNOT BE RECOVERED");
    m_warning.SetTextRole (DxuiTextRole::Error);
    m_warning.SetFontWeight (DxuiFontWeight::SemiBold);

    m_body = CreateDialogContent<CassoExplorerInitializeCardPanel>();
    m_body->Init (m_question, m_choices, m_description, descriptions, m_contents, m_warning);

    erase  = AddDialogButton (L"Erase and initialize", IDOK);
    cancel = AddDialogButton (L"Cancel", IDCANCEL);

    erase->SetVariant (DxuiButton::Variant::Danger);
    erase->SetOnClick ([this]()
    {
        int  selected = m_choices.GetSelectedIndex();

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
