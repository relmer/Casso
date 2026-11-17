#include "Pch.h"

#include "CassoExplorer/CassoExplorerInitializeCardDialog.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerInitializeCardPanel::Init (const Parts & parts)
{
    m_parts = parts;

    Adopt (*parts.question);
    Adopt (*parts.adapterLabel);
    Adopt (*parts.adapter);
    Adopt (*parts.layoutLabel);
    Adopt (*parts.layout);
    Adopt (*parts.quick);
    Adopt (*parts.grid);
    Adopt (*parts.contents);
    Adopt (*parts.warning);
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
    return m_parts.adapter->OnMouse (ev) || m_parts.layout->OnMouse (ev) || m_parts.quick->OnMouse (ev);
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
//  Returns the height of the stack. Each dropdown's label is to its left.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerInitializeCardPanel::Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place)
{
    int  width    = bounds.right - bounds.left;
    int  gap      = scaler.ToPx (kSectionGapDip);
    int  nearGap  = scaler.ToPx (ExplorerMetrics::kStackGapDip);
    int  field    = scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);
    int  labelW   = width * 2 / 5;
    int  top      = bounds.top;
    int  question = GetLabelHeightPx (*m_parts.question, width, scaler);
    int  grid     = m_parts.grid->MeasureHeightPx (width, scaler);
    int  contents = GetLabelHeightPx (*m_parts.contents, width, scaler);
    int  warning  = GetLabelHeightPx (*m_parts.warning,  width, scaler);



    if (place)
    {
        m_parts.question->Layout     (RECT { bounds.left, top, bounds.right, top + question }, scaler);
        top += question + gap;
        m_parts.adapterLabel->Layout (RECT { bounds.left, top, bounds.left + labelW, top + field }, scaler);
        m_parts.adapter->Layout      (RECT { bounds.left + labelW, top, bounds.right, top + field }, scaler);
        top += field + nearGap;
        m_parts.layoutLabel->Layout  (RECT { bounds.left, top, bounds.left + labelW, top + field }, scaler);
        m_parts.layout->Layout       (RECT { bounds.left + labelW, top, bounds.right, top + field }, scaler);
        top += field + nearGap;
        m_parts.quick->Layout        (RECT { bounds.left + labelW, top, bounds.right, top + field }, scaler);
        top += field + gap;
        m_parts.grid->Layout         (RECT { bounds.left, top, bounds.right, top + grid }, scaler);
        top += grid + gap;
        m_parts.contents->Layout     (RECT { bounds.left, top, bounds.right, top + contents }, scaler);
        top += contents + gap;
        m_parts.warning->Layout      (RECT { bounds.left, top, bounds.right, top + warning }, scaler);
        top += warning;
    }
    else
    {
        top += question + gap + field + nearGap + field + nearGap + field + gap + grid + gap + contents + gap + warning;
    }

    return top - bounds.top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardDialog::OnCreate
//
//  The erase button reads the chosen layout before the dialog ends.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerInitializeCardDialog::OnCreate()
{
    DxuiButton                               * cancel = nullptr;
    std::vector<std::wstring>                  adapters;
    CassoExplorerInitializeCardPanel::Parts    parts;



    m_question.SetText (m_questionText);
    ExplorerTextStyles::Apply (m_question, ExplorerTextStyles::kFormLabel);
    m_question.SetFontWeight (DxuiFontWeight::SemiBold);

    for (DxuiLabel * label : { &m_adapterLabel, &m_layoutLabel })
    {
        ExplorerTextStyles::Apply (*label, ExplorerTextStyles::kFormLabel);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_adapterLabel.SetText (L"Which adapter do you have?");
    m_layoutLabel.SetText (L"Layout:");

    for (int index = (int) CardLayoutModel::Adapter::First; index < (int) CardLayoutModel::Adapter::Count; index++)
    {
        adapters.push_back (CardLayoutModel::GetAdapterLabel ((CardLayoutModel::Adapter) index));
    }

    m_adapterBox.SetPopupHost (GetPopupHost());
    m_adapterBox.SetItems (adapters);
    m_adapterBox.SetSelected ((int) m_adapter - (int) CardLayoutModel::Adapter::First);
    m_adapterBox.SetSelect ([this] (int index) { ChooseAdapter ((CardLayoutModel::Adapter) (index + (int) CardLayoutModel::Adapter::First)); });

    m_layoutBox.SetPopupHost (GetPopupHost());
    m_layoutBox.SetSelect ([this] (int index) { ChooseLayout (index); });

    m_quick.SetLabel (L"Quick format");
    m_quick.SetChecked (true);

    m_contents.SetText (m_contentsText);
    ExplorerTextStyles::Apply (m_contents, ExplorerTextStyles::kFormLabel);

    m_warning.SetText (L"ALL DATA ON THIS CARD WILL BE ERASED AND CANNOT BE RECOVERED");
    m_warning.SetTextRole (DxuiTextRole::Error);
    m_warning.SetFontWeight (DxuiFontWeight::SemiBold);

    ChooseAdapter (m_adapter);

    parts = { &m_question, &m_adapterLabel, &m_adapterBox, &m_layoutLabel, &m_layoutBox, &m_quick, &m_grid, &m_contents, &m_warning };

    m_body = CreateDialogContent<CassoExplorerInitializeCardPanel>();
    m_body->Init (parts);

    m_erase = AddDialogButton (L"Erase and initialize", IDOK);
    cancel  = AddDialogButton (L"Cancel", IDCANCEL);

    m_erase->SetVariant (DxuiButton::Variant::Danger);
    m_erase->SetEnabled (!m_positions.empty());
    m_erase->SetOnClick ([this]()
    {
        m_layout    = m_model.GetLayout (m_position);
        m_confirmed = true;

        //  FAT32 is written as Windows' quick format is, its tables only.
        if (!m_quick.IsChecked() && !m_model.IsFat32 (m_position))
        {
            m_layout |= RawDeviceProtocol::kLayoutZeroFill;
        }

        EndDialog (IDOK);
    });

    SetInitialFocus (cancel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardDialog::ChooseAdapter
//
//  The layouts the adapter is offered, its recommended one chosen and
//  marked.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerInitializeCardDialog::ChooseAdapter (CardLayoutModel::Adapter adapter)
{
    int                        recommended = m_model.GetRecommendedPosition (adapter);
    int                        selected    = 0;
    std::vector<std::wstring>  labels;



    m_adapter = adapter;
    m_model.GetChoices (adapter, m_positions);

    for (size_t index = 0; index < m_positions.size(); index++)
    {
        std::wstring  label = m_model.DescribePosition (m_positions[index]);

        if (m_positions[index] == recommended)
        {
            label   += L" (recommended)";
            selected = (int) index;
        }

        labels.push_back (std::move (label));
    }

    //  An A2Pico takes FAT32 only, which a card under about 33 MB is too
    //  small for, so nothing can be erased.
    if (m_positions.empty())
    {
        labels.push_back (L"None. The card is too small for FAT32");
    }

    if (m_erase != nullptr)
    {
        m_erase->SetEnabled (!m_positions.empty());
    }

    m_layoutBox.SetItems (labels);
    m_layoutBox.SetSelected (selected);

    ChooseLayout (selected);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardDialog::ChooseLayout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerInitializeCardDialog::ChooseLayout (int index)
{
    if (index >= 0 && (size_t) index < m_positions.size())
    {
        m_position = m_positions[(size_t) index];
    }

    m_grid.SetChoice (m_position, m_adapter);
    m_quick.SetEnabled (!m_model.IsFat32 (m_position));

    if (GetHwnd() != nullptr)
    {
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardDialog::Confirm
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerInitializeCardDialog::Confirm (HWND                         owner,
                                                 const IDxuiTheme           * theme,
                                                 const std::wstring         & question,
                                                 const CardLayoutModel      & model,
                                                 const std::wstring         & contents,
                                                 CardLayoutModel::Adapter   & ioAdapter,
                                                 uint64_t                   & outLayout)
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
    dialog.m_adapter      = CardLayoutModel::IsAdapter ((int) ioAdapter) ? ioAdapter : CardLayoutModel::kDefaultAdapter;

    params.title                    = L"Initialize card for Apple";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 600, 520 };
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

    ioAdapter = dialog.m_adapter;
    outLayout = dialog.m_layout;

    return dialog.m_confirmed;
}