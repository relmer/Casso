#include "Pch.h"

#include "CassoExplorer/CassoExplorerCopyOptionsDialog.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCopyOptionsPanel::Init (const std::vector<Line> & lines, const std::vector<DxuiCheckbox *> & checks)
{
    m_lines  = lines;
    m_checks = checks;

    for (const Line & line : m_lines)
    {
        Adopt (*line.label);
        Adopt (*line.box);
    }

    for (DxuiCheckbox * check : m_checks)
    {
        Adopt (*check);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCopyOptionsPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);

    m_scaler = scaler;

    Arrange (boundsDip, scaler, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsPanel::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerCopyOptionsPanel::OnMouse (const DxuiMouseEvent & ev)
{
    for (const Line & line : m_lines)
    {
        if (line.box->OnMouse (ev))
        {
            return true;
        }
    }

    for (DxuiCheckbox * check : m_checks)
    {
        if (check->OnMouse (ev))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsPanel::TryMeasureHeightPx
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerCopyOptionsPanel::TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx)
{
    RECT  bounds = { 0, 0, widthPx, 0 };



    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);

    heightPx = Arrange (bounds, m_scaler, false);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsPanel::Arrange
//
//  Returns the height of the rows and check boxes.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerCopyOptionsPanel::Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place)
{
    int  width  = bounds.right - bounds.left;
    int  gap    = scaler.ToPx (ExplorerMetrics::kStackGapDip);
    int  field  = scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);
    int  labelW = width * 2 / 5;
    int  top    = bounds.top;



    for (const Line & line : m_lines)
    {
        if (place)
        {
            line.label->Layout (RECT { bounds.left, top, bounds.left + labelW, top + field }, scaler);
            line.box->Layout   (RECT { bounds.left + labelW, top, bounds.right, top + field }, scaler);
        }

        top += field + gap;
    }

    for (DxuiCheckbox * check : m_checks)
    {
        if (place)
        {
            check->Layout (RECT { bounds.left + labelW, top, bounds.right, top + field }, scaler);
        }

        top += field + gap;
    }

    return (top > bounds.top) ? top - gap - bounds.top : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog::GetRowLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassoExplorerCopyOptionsDialog::GetRowLabel (CopyOptionRows::Row row)
{
    switch (row)
    {
        case CopyOptionRows::Row::Charset:      return L"Text character set:";
        case CopyOptionRows::Row::Spreadsheet:  return L"Spreadsheets:";
        case CopyOptionRows::Row::HiRes:        return L"Hi-res pictures:";
        case CopyOptionRows::Row::DoubleHiRes:  return L"Double hi-res pictures:";
        case CopyOptionRows::Row::Basic:        return L"BASIC programs:";
        case CopyOptionRows::Row::Code:         return L"Code files:";
        default:                                return L"";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog::GetRowChoices
//
//  In the order of the option's values, as CopyOptionRows reads them.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<std::wstring> & CassoExplorerCopyOptionsDialog::GetRowChoices (CopyOptionRows::Row row)
{
    static const std::vector<std::wstring>  s_charset     = { L"Automatic", L"Apple text", L"Mac OS Roman", L"ISO 8859-1" };
    static const std::vector<std::wstring>  s_spreadsheet = { L"Values", L"Formulas" };
    static const std::vector<std::wstring>  s_hiRes       = { L"Color", L"Black and white" };
    static const std::vector<std::wstring>  s_doubleHiRes = { L"Automatic", L"Black and white", L"Simple color", L"Smooth color", L"Smooth color, clean edges" };
    static const std::vector<std::wstring>  s_basic       = { L"Text listing", L"Text listing, control characters as bytes", L"Colored listing (RTF)" };
    static const std::vector<std::wstring>  s_code        = { L"Bytes", L"6502 disassembly", L"6502 disassembly, undocumented opcodes", L"65C02 disassembly", L"65816 disassembly" };
    static const std::vector<std::wstring>  s_none;



    switch (row)
    {
        case CopyOptionRows::Row::Charset:      return s_charset;
        case CopyOptionRows::Row::Spreadsheet:  return s_spreadsheet;
        case CopyOptionRows::Row::HiRes:        return s_hiRes;
        case CopyOptionRows::Row::DoubleHiRes:  return s_doubleHiRes;
        case CopyOptionRows::Row::Basic:        return s_basic;
        case CopyOptionRows::Row::Code:         return s_code;
        default:                                return s_none;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCopyOptionsDialog::OnCreate()
{
    std::vector<CassoExplorerCopyOptionsPanel::Line>  lines;
    std::vector<DxuiCheckbox *>                       checks;
    DxuiButton                                      * copy  = nullptr;
    bool                                              code  = std::find (m_rows.begin(), m_rows.end(), CopyOptionRows::Row::Code) != m_rows.end();



    for (size_t index = 0; index < m_rows.size() && index < kRowCountMax; index++)
    {
        CopyOptionRows::Row  row = m_rows[index];

        m_labels[index].SetText (GetRowLabel (row));
        ExplorerTextStyles::Apply (m_labels[index], ExplorerTextStyles::kFormLabel);
        m_labels[index].SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

        m_boxes[index].SetPopupHost (GetPopupHost());
        m_boxes[index].SetItems     (GetRowChoices (row));
        m_boxes[index].SetSelected  (CopyOptionRows::GetChoice (row, m_options));
        m_boxes[index].SetSelect    ([this, row] (int choice)
        {
            CopyOptionRows::SetChoice (row, choice, m_options);
            UpdateChecks();
        });

        lines.push_back ({ &m_labels[index], &m_boxes[index] });
    }

    m_longRegisters.SetLabel    (L"65816 code starts with 16-bit registers");
    m_longRegisters.SetChecked  (m_options.longRegisters);
    m_longRegisters.SetOnChange ([this] (bool checked) { m_options.longRegisters = checked; });

    m_followFlags.SetLabel    (L"65816 register widths follow REP and SEP");
    m_followFlags.SetChecked  (m_options.followFlags);
    m_followFlags.SetOnChange ([this] (bool checked) { m_options.followFlags = checked; });

    if (code)
    {
        checks = { &m_longRegisters, &m_followFlags };
    }

    UpdateChecks();

    m_body = CreateDialogContent<CassoExplorerCopyOptionsPanel>();
    m_body->Init (lines, checks);

    copy = AddDialogButton (L"Copy", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    copy->SetOnClick ([this]()
    {
        m_confirmed = true;
        EndDialog (IDOK);
    });

    SetInitialFocus (copy);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog::UpdateChecks
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCopyOptionsDialog::UpdateChecks()
{
    bool  wdc65816 = m_options.disassembly == DisassemblyCpu::Wdc65816;



    m_longRegisters.SetEnabled (wdc65816);
    m_followFlags.SetEnabled   (wdc65816);

    if (GetHwnd() != nullptr)
    {
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog::Choose
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerCopyOptionsDialog::Choose (HWND                                       owner,
                                             const IDxuiTheme                         * theme,
                                             const std::vector<CopyOptionRows::Row>   & rows,
                                             DecodeOptions                            & ioOptions)
{
    HRESULT                         hr     = S_OK;
    CassoExplorerCopyOptionsDialog  dialog;
    DxuiWindow::CreateParams        params;
    bool                            code   = std::find (rows.begin(), rows.end(), CopyOptionRows::Row::Code) != rows.end();
    int                             lines  = (int) rows.size() + (code ? 2 : 0);



    dialog.m_rows    = rows;
    dialog.m_options = ioOptions;

    params.title                    = L"Copy to folder with options";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 560, 160 + 36 * lines };
    params.minSizeDip               = { 440, 140 };
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

    if (dialog.m_confirmed)
    {
        ioOptions = dialog.m_options;
    }

    return dialog.m_confirmed;
}
