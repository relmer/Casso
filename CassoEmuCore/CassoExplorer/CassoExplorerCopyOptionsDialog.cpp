#include "Pch.h"

#include "CassoExplorer/CassoExplorerCopyOptionsDialog.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCopyOptionsPanel::Init (const std::vector<Line> & lines)
{
    m_lines = lines;

    for (const Line & line : m_lines)
    {
        for (IDxuiControl * control : { (IDxuiControl *) line.label, (IDxuiControl *) line.box, (IDxuiControl *) line.check, (IDxuiControl *) line.note,
                                         (IDxuiControl *) line.value, (IDxuiControl *) line.button })
        {
            if (control != nullptr)
            {
                Adopt (*control);
            }
        }
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
    if (m_onMouse)
    {
        m_onMouse (ev);
    }

    for (const Line & line : m_lines)
    {
        if ((line.box != nullptr && line.box->OnMouse (ev)) || (line.check != nullptr && line.check->OnMouse (ev))
            || (line.button != nullptr && line.button->OnMouse (ev)))
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
//  Returns the height of the lines and the space below them, above the
//  buttons. Each group space above a line is three times the space between
//  lines. A note is one line of text, close under the line above it.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerCopyOptionsPanel::Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place)
{
    int   width  = bounds.right - bounds.left;
    int   gap    = scaler.ToPx (ExplorerMetrics::kStackGapDip);
    int   field  = scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);
    int   labelW = (std::min) (scaler.ToPx (kLabelWidthDip), width / 2);
    int   top    = bounds.top;
    int   noteH  = scaler.ToPx (kNoteHeightDip);
    int   inset  = scaler.ToPx (DxuiComboBox::kTextInsetDip);
    int   button = scaler.ToPx (kButtonWidthDip);
    RECT  column = {};



    for (const Line & line : m_lines)
    {
        top    += (top > bounds.top) ? gap * 2 * line.groupGaps : 0;
        top    -= (line.note != nullptr) ? gap : 0;
        column  = RECT { bounds.left + labelW, top, bounds.right, top + ((line.note != nullptr) ? noteH : field) };

        if (place && line.label != nullptr)
        {
            line.label->Layout (RECT { bounds.left, top, bounds.left + labelW, top + field }, scaler);
        }

        if (place && line.box != nullptr)
        {
            line.box->Layout (column, scaler);
        }

        if (place && line.check != nullptr)
        {
            line.check->Layout (column, scaler);
        }

        //  A note's text lines up with the dropdowns' text.
        if (place && line.note != nullptr)
        {
            line.note->Layout (RECT { column.left + inset, column.top, column.right, column.bottom }, scaler);
        }

        if (place && line.value != nullptr)
        {
            line.value->Layout (RECT { column.left + inset, column.top, column.right, column.bottom }, scaler);
        }

        if (place && line.button != nullptr)
        {
            line.button->Layout (RECT { column.left, column.top, column.left + button, column.bottom }, scaler);
        }

        top += ((line.note != nullptr) ? noteH : field) + gap;
    }

    return (top > bounds.top) ? top - gap - bounds.top + gap * 3 : 0;
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
        case CopyOptionRows::Row::HexDump:      return L"File conversions:";
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
    static const std::vector<std::wstring>  s_hexDump     = { L"By file type", L"Hex dump of every file" };
    static const std::vector<std::wstring>  s_none;



    switch (row)
    {
        case CopyOptionRows::Row::Charset:      return s_charset;
        case CopyOptionRows::Row::Spreadsheet:  return s_spreadsheet;
        case CopyOptionRows::Row::HiRes:        return s_hiRes;
        case CopyOptionRows::Row::DoubleHiRes:  return s_doubleHiRes;
        case CopyOptionRows::Row::Basic:        return s_basic;
        case CopyOptionRows::Row::Code:         return s_code;
        case CopyOptionRows::Row::HexDump:      return s_hexDump;
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
    DxuiButton                                      * copy  = nullptr;



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

        //  The 65816's check boxes follow the code row.
        lines.push_back ({ .label = &m_labels[index], .box = &m_boxes[index] });

        if (row == CopyOptionRows::Row::Code)
        {
            lines.push_back ({ .check = &m_longRegisters });
            lines.push_back ({ .check = &m_followFlags });
        }
    }

    AddNamingRows     (lines);
    AddDestinationRow (lines);

    m_longRegisters.SetLabel    (L"65816 code starts with 16-bit registers");
    m_longRegisters.SetChecked  (m_options.longRegisters);
    m_longRegisters.SetOnChange ([this] (bool checked) { m_options.longRegisters = checked; });

    m_followFlags.SetLabel    (L"65816 register widths follow REP and SEP");
    m_followFlags.SetChecked  (m_options.followFlags);
    m_followFlags.SetOnChange ([this] (bool checked) { m_options.followFlags = checked; });

    UpdateChecks();

    m_body = CreateDialogContent<CassoExplorerCopyOptionsPanel>();
    m_body->Init (lines);
    m_body->SetOnMouse ([this] (const DxuiMouseEvent & ev) { ShowTip (ev); });

    m_tip.SetPopupHost (GetPopupHost());

    if (m_tipTheme != nullptr)
    {
        m_tip.SetTheme (*m_tipTheme);
    }

    SetDialogTickIntervalMs (kTipTickMs);

    copy   = AddDialogButton (L"Copy", IDOK);
    m_copy = copy;
    AddDialogButton (L"Cancel", IDCANCEL);

    copy->SetOnClick ([this]()
    {
        m_confirmed = true;
        EndDialog (IDOK);
    });

    UpdateCopyEnabled();
    SetInitialFocus (copy);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog::AddNamingRows
//
//  At the end, as a group: the naming style, the global one to start, with
//  the first selected file's name in it under it, and then which files
//  take it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCopyOptionsDialog::AddNamingRows (std::vector<CassoExplorerCopyOptionsPanel::Line> & ioLines)
{
    static const std::vector<std::wstring>  s_styles = { L"Descriptive", L"CiderPress", L"AppleSingle" };
    static const std::vector<std::wstring>  s_scopes = { L"No files", L"Files without a conversion", L"All files" };



    for (DxuiLabel * label : { &m_styleLabel, &m_scopeLabel, &m_namePreview })
    {
        ExplorerTextStyles::Apply (*label, ExplorerTextStyles::kFormLabel);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_namePreview.SetTextRole (DxuiTextRole::Muted);
    m_namePreview.SetElide    (DxuiElide::Tail);

    m_styleLabel.SetText     (L"File names:");
    m_styleBox.SetPopupHost  (GetPopupHost());
    m_styleBox.SetItems      (s_styles);
    m_styleBox.SetSelected   ((int) m_naming.style);
    m_styleBox.SetSelect     ([this] (int choice) { m_naming.style = (HostFileNaming::Style) choice; UpdateNamePreview(); });

    m_scopeLabel.SetText     (L"Apply names to:");
    m_scopeBox.SetPopupHost  (GetPopupHost());
    m_scopeBox.SetItems      (s_scopes);
    m_scopeBox.SetSelected   ((int) m_naming.scope);
    m_scopeBox.SetSelect     ([this] (int choice) { m_naming.scope = (ActionsTypes::NamingScope) choice; });

    UpdateNamePreview();

    ioLines.push_back ({ .label = &m_styleLabel, .box = &m_styleBox, .groupGaps = 1 });
    ioLines.push_back ({ .note = &m_namePreview });
    ioLines.push_back ({ .label = &m_scopeLabel, .box = &m_scopeBox });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog::AddDestinationRow
//
//  Last, below a double group space: the folder the files are copied to,
//  and under it Browse to pick another.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCopyOptionsDialog::AddDestinationRow (std::vector<CassoExplorerCopyOptionsPanel::Line> & ioLines)
{
    for (DxuiLabel * label : { &m_folderLabel, &m_folderPath })
    {
        ExplorerTextStyles::Apply (*label, ExplorerTextStyles::kFormLabel);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_folderLabel.SetText (L"Destination:");
    m_folderPath.SetText  (m_folder);
    m_folderPath.SetElide (DxuiElide::Middle);

    m_browse.SetLabel   (L"Browse...");
    m_browse.SetOnClick ([this]()
    {
        if (m_pick && m_pick (GetHwnd(), m_folder))
        {
            m_folderPath.SetText (m_folder);
            UpdateCopyEnabled();
        }
    });

    ioLines.push_back ({ .label = &m_folderLabel, .value = &m_folderPath, .groupGaps = 2 });
    ioLines.push_back ({ .button = &m_browse });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog::ShowTip
//
//  With the pointer over the destination, the tip has its full path.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCopyOptionsDialog::ShowTip (const DxuiMouseEvent & ev)
{
    int64_t  now  = (int64_t) GetTickCount64();
    RECT     rect = m_folderPath.GetRect();



    if (ev.kind != DxuiMouseEventKind::Move)
    {
        return;
    }

    if (!m_folder.empty() && ev.positionDip.x >= rect.left && ev.positionDip.x < rect.right && ev.positionDip.y >= rect.top && ev.positionDip.y < rect.bottom)
    {
        m_tip.RequestShow (m_folderPath.GetRect(), m_folder, now);
    }
    else
    {
        m_tip.RequestHide (now);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog::OnDialogTick
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCopyOptionsDialog::OnDialogTick()
{
    m_tip.Tick ((int64_t) GetTickCount64());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog::UpdateCopyEnabled
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCopyOptionsDialog::UpdateCopyEnabled()
{
    std::error_code  error;
    bool             exists = !m_folder.empty() && fs::is_directory (m_folder, error);



    if (m_copy != nullptr)
    {
        m_copy->SetEnabled (exists);
    }

    if (GetHwnd() != nullptr)
    {
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog::UpdateNamePreview
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCopyOptionsDialog::UpdateNamePreview()
{
    m_namePreview.SetText (m_nameFor ? m_nameFor (m_naming.style) : std::wstring());

    if (GetHwnd() != nullptr)
    {
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog::UpdateChecks
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCopyOptionsDialog::UpdateChecks()
{
    bool  converted = !m_options.hexDump;
    bool  wdc65816  = converted && m_options.disassembly == DisassemblyCpu::Wdc65816;



    m_styleBox.SetEnabled     (converted);
    m_scopeBox.SetEnabled     (converted);
    m_styleLabel.SetTextRole  (converted ? *ExplorerTextStyles::kFormLabel.role : DxuiTextRole::Disabled);
    m_scopeLabel.SetTextRole  (converted ? *ExplorerTextStyles::kFormLabel.role : DxuiTextRole::Disabled);
    m_namePreview.SetTextRole (converted ? DxuiTextRole::Muted : DxuiTextRole::Disabled);

    for (size_t index = 0; index < m_rows.size() && index < kRowCountMax; index++)
    {
        if (m_rows[index] != CopyOptionRows::Row::HexDump)
        {
            m_boxes[index].SetEnabled   (converted);
            m_labels[index].SetTextRole (converted ? *ExplorerTextStyles::kFormLabel.role : DxuiTextRole::Disabled);
        }
    }

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
                                             DecodeOptions                            & ioOptions,
                                             Naming                                   & ioNaming,
                                             std::wstring                             & ioFolder,
                                             const NameFn                             & nameFor,
                                             const PickFn                             & pick)
{
    HRESULT                         hr     = S_OK;
    CassoExplorerCopyOptionsDialog  dialog;
    DxuiWindow::CreateParams        params;
    bool                            code   = std::find (rows.begin(), rows.end(), CopyOptionRows::Row::Code) != rows.end();
    int                             lines  = (int) rows.size() + 7 + (code ? 2 : 0);



    dialog.m_rows     = rows;
    dialog.m_options  = ioOptions;
    dialog.m_naming   = ioNaming;
    dialog.m_nameFor  = nameFor;
    dialog.m_folder   = ioFolder;
    dialog.m_pick     = pick;
    dialog.m_tipTheme = theme;

    params.title                    = L"Copy to folder with options";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 720, 172 + 36 * lines };
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
        ioNaming  = dialog.m_naming;
        ioFolder  = dialog.m_folder;
    }

    return dialog.m_confirmed;
}
