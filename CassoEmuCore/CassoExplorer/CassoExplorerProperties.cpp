#include "Pch.h"

#include "CassoExplorer/CassoExplorerProperties.h"
#include "CassoExplorer/CassoExplorerBrowser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerProperties::DescribeEntry
//
//  Explorer's groups: what the file is; where it is and how big; when it was
//  written; its attributes. A DOS 3.3 file records no date and no exact
//  length, so it has no Modified row, and its size is what its sectors hold.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<CassoExplorerProperties::Row> CassoExplorerProperties::DescribeEntry (const FileEntry & entry, VolumeKind kind, const std::wstring & location)
{
    std::vector<Row>  rows;
    uint64_t          unit    = CatalogModel::GetUnitBytes (kind);
    uint64_t          onDisk  = (uint64_t) entry.sizeUnits * unit;
    std::wstring      type    = CatalogModel::GetTypeText (entry.type, kind);
    wchar_t           code[8] = {};
    const wchar_t   * units  = (kind == VolumeKind::ProDos) ? L"blocks" : L"sectors";



    if (kind == VolumeKind::ProDos)
    {
        swprintf_s (code, L" ($%02X)", (unsigned) entry.type);
        type += code;
    }

    rows.push_back (Row { L"Type of file:", entry.isDirectory ? L"Directory" : type, true });

    rows.push_back (Row { s_kLocationLabel, location, true });

    if (!entry.isDirectory)
    {
        rows.push_back (Row { L"Size:", FormatBytes (entry.hasEofBytes ? entry.eofBytes : onDisk) });
    }

    rows.push_back (Row { L"Size on disk:", FormatBytes (onDisk) + L", " + std::to_wstring (entry.sizeUnits) + L" " + units });

    if (entry.hasLoadAddress)
    {
        rows.push_back (Row { L"Load address:", CatalogModel::FormatAddress (entry.loadAddress) });
    }
    else if (entry.hasAuxType && !entry.isDirectory)
    {
        rows.push_back (Row { L"Aux type:", CatalogModel::FormatAddress (entry.auxType) });
    }

    if (entry.hasModified)
    {
        rows.push_back (Row { L"Modified:", CassoExplorerBrowser::FormatModified (entry.modifiedUnix, true), true });
    }

    rows.push_back (Row { L"Attributes:", entry.isLocked ? L"Locked" : L"Not locked", true });

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerProperties::FormatBytes
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerProperties::FormatBytes (uint64_t bytes)
{
    std::wstring  digits = std::to_wstring (bytes);
    std::wstring  grouped;
    size_t        i      = 0;



    for (i = 0; i < digits.size(); i++)
    {
        if (i > 0 && (digits.size() - i) % 3 == 0)
        {
            grouped += L',';
        }

        grouped += digits[i];
    }

    return CassoExplorerBrowser::FormatSize (bytes) + L" (" + grouped + ((bytes == 1) ? L" byte)" : L" bytes)");
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPropertiesPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerPropertiesPanel::Init (DxuiLabel * name, std::vector<Line> lines)
{
    m_name  = name;
    m_lines = std::move (lines);

    Adopt (*m_name);

    for (const Line & line : m_lines)
    {
        Adopt (*line.label);
        Adopt (*line.value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPropertiesPanel::GetRequiredHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerPropertiesPanel::GetRequiredHeightDip() const
{
    int  height = kNameHeightDip;



    for (const Line & line : m_lines)
    {
        height += (line.groupStart ? kGroupGapDip : 0) + kRowHeightDip;
    }

    return height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPropertiesPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerPropertiesPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  row    = scaler.ToPx (kRowHeightDip);
    int  gap    = scaler.ToPx (kGroupGapDip);
    int  valueX = boundsDip.left + scaler.ToPx (kLabelWidthDip);
    int  y      = boundsDip.top;



    SetBounds (boundsDip);

    m_name->Layout (RECT { boundsDip.left, y, boundsDip.right, y + scaler.ToPx (kNameHeightDip) }, scaler);
    y += scaler.ToPx (kNameHeightDip);

    m_dividerY.clear();

    for (const Line & line : m_lines)
    {
        if (line.groupStart)
        {
            m_dividerY.push_back (y + gap / 2);
            y += gap;
        }

        line.label->Layout (RECT { boundsDip.left, y, valueX, y + row }, scaler);
        line.value->Layout (RECT { valueX, y, boundsDip.right, y + row }, scaler);
        y += row;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPropertiesPanel::Paint
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerPropertiesPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  bounds = GetBounds();



    for (int y : m_dividerY)
    {
        painter.FillRect ((float) bounds.left, (float) y, (float) (bounds.right - bounds.left), 1.0f, theme.Divider());
    }

    DxuiPanel::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPropertiesDialog::Show
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerPropertiesDialog::Show (HWND owner, const IDxuiTheme * theme, const std::wstring & name, const std::vector<CassoExplorerProperties::Row> & rows)
{
    HRESULT                        hr     = S_OK;
    CassoExplorerPropertiesDialog  dialog;
    DxuiWindow::CreateParams       params;
    int                            height = CassoExplorerPropertiesPanel::kNameHeightDip;



    for (const CassoExplorerProperties::Row & row : rows)
    {
        height += (row.groupStart ? CassoExplorerPropertiesPanel::kGroupGapDip : 0) + CassoExplorerPropertiesPanel::kRowHeightDip;
    }

    dialog.m_theme = theme;
    dialog.m_rows  = rows;
    dialog.m_name.SetText (name);

    params.title                    = name + L" Properties";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 520, height + s_kChromeDip };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return;
    }

    dialog.SetTheme (theme);
    dialog.ShowModalDialog (IDOK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPropertiesDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerPropertiesDialog::OnCreate()
{
    std::vector<CassoExplorerPropertiesPanel::Line>  lines;
    DxuiButton                                     * ok    = nullptr;



    m_name.SetTextRole   (DxuiTextRole::Heading);
    m_name.SetTextAlign  (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    m_name.SetFontWeight (DxuiFontWeight::SemiBold);

    for (const CassoExplorerProperties::Row & row : m_rows)
    {
        CassoExplorerPropertiesPanel::Line  line;

        m_labels.push_back (std::make_unique<DxuiLabel>());
        line.label = m_labels.back().get();
        m_labels.push_back (std::make_unique<DxuiLabel>());
        line.value = m_labels.back().get();

        line.label->SetText      (row.label);
        line.label->SetTextRole  (DxuiTextRole::Muted);
        line.value->SetText      (row.value);
        line.value->SetTextRole  (DxuiTextRole::Body);
        line.groupStart          = row.groupStart;

        for (DxuiLabel * label : { line.label, line.value })
        {
            label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
            label->SetElide     (DxuiElide::Tail);
        }

        //  A path keeps its end, where the image's name is, as a path cut
        //  short anywhere in Explorer does.
        if (row.label == CassoExplorerProperties::s_kLocationLabel)
        {
            line.value->SetElide (DxuiElide::PathHead);
        }

        lines.push_back (line);
    }

    m_body = CreateDialogContent<CassoExplorerPropertiesPanel>();
    m_body->Init (&m_name, std::move (lines));

    ok = AddDialogButton (L"OK", IDOK);
    ok->SetOnClick ([this]() { EndDialog (IDOK); });

    SetInitialFocus (ok);
}
