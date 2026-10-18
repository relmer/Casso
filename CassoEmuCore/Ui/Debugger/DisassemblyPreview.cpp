#include "Pch.h"

#include "Ui/Debugger/DisassemblyPreview.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/GutterGlyph.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyPreview::DisassemblyPreview
//
//  The look of a disassembly view, with nothing to select or focus.
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyPreview::DisassemblyPreview()
    : m_lines (MakeSampleLines())
{
    m_focusable = false;

    DebuggerWindow::MakeDense (this);
    DisassemblyCells::Configure (*this);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyPreview::SetOptions
//
//  The rows are built at the next paint, which has the theme's colors.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyPreview::SetOptions (const DisassemblyOptions & options)
{
    m_options = options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyPreview::MakeSampleLines
//
//  A loop that prints five bytes through a pointer, stopped on its branch
//  back with Y not yet zero: source lines 10 to 15 of one file.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DebuggerViewSnapshot::CodeLine> DisassemblyPreview::MakeSampleLines()
{
    std::vector<DebuggerViewSnapshot::CodeLine>  lines (6);



    lines[0].address                      = 0x0300;
    lines[0].bytes                        = "A0 05";
    lines[0].label                        = "START";
    lines[0].instruction                  = "LDY #$05";

    lines[1].address                      = 0x0302;
    lines[1].bytes                        = "B1 3E";
    lines[1].label                        = "LOOP";
    lines[1].instruction                  = "LDA (PTR),Y";
    lines[1].shownOperand                 = "PTR";
    lines[1].memoryOperand                = "$3E";
    lines[1].annotationParts.registers    = "Y=05";
    lines[1].annotationParts.pointer      = "($003E)=$1230";
    lines[1].annotationParts.operandValue = "$1235=C1";

    lines[2].address                      = 0x0304;
    lines[2].bytes                        = "20 ED FD";
    lines[2].instruction                  = "JSR COUT";
    lines[2].shownOperand                 = "COUT";
    lines[2].memoryOperand                = "$FDED";

    lines[3].address                      = 0x0307;
    lines[3].bytes                        = "88";
    lines[3].instruction                  = "DEY";
    lines[3].annotationParts.registers    = "Y=05";

    lines[4].address                      = 0x0308;
    lines[4].bytes                        = "D0 F8";
    lines[4].instruction                  = "BNE LOOP";
    lines[4].shownOperand                 = "LOOP";
    lines[4].memoryOperand                = "$0302";
    lines[4].isCurrent                    = true;
    lines[4].target                       = Word (0x0302);
    lines[4].isTargetTaken                = true;
    lines[4].annotationParts.flags        = "Z=0";
    lines[4].effect                       = "PC=0302; 3 cycles";

    lines[5].address                      = 0x030A;
    lines[5].bytes                        = "60";
    lines[5].instruction                  = "RTS";

    for (size_t i = 0; i < lines.size(); i++)
    {
        lines[i].sourceFileId = 0;
        lines[i].sourceLine   = 10 + (int) i;
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyPreview::GetSampleSource
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DisassemblyPreview::GetSampleSource (int sourceLine)
{
    static const wchar_t * const  s_kSource[] =
    {
        L"START   LDY #5",
        L"LOOP    LDA (PTR),Y",
        L"        JSR COUT",
        L"        DEY",
        L"        BNE LOOP",
        L"        RTS",
    };
    size_t  index = (size_t) (sourceLine - 10);



    return (index < std::size (s_kSource)) ? std::wstring (s_kSource[index]) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyPreview::BuildRows
//
//  The rows a disassembly view builds from these lines and options, with the
//  PC's arrow in the glyph margin and its row's fill, and the kind of row
//  the options dim drawn dimmed.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyPreview::BuildRows (const DebuggerTextColors::Set & colors, const IDxuiTheme & theme, uint32_t pcMarkerArgb)
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;
    bool                                          source  = m_options.IsOn (DisassemblyOptions::Option::Source);
    bool                                          numbers = source && m_options.IsOn (DisassemblyOptions::Option::LineNumbers);
    bool                                          isPc    = false;



    DisassemblyCells::ShowColumns (*this, m_options);

    m_rows = DisassemblyOptions::BuildRows (m_lines, source, [] (int, int line) { return !GetSampleSource (line).empty(); });

    for (const DisassemblyOptions::Row & each : m_rows)
    {
        std::vector<DxuiListView::Cell>  cells (DisassemblyCells::kColumnCount);
        uint32_t                         fill = 0;



        isPc = each.codeLine >= 0 && m_lines[(size_t) each.codeLine].isCurrent;

        if (each.codeLine < 0)
        {
            DisassemblyCells::FillSourceRow (cells, *this, each.sourceLine, GetSampleSource (each.sourceLine), numbers, colors, theme.ForegroundMuted());
        }
        else
        {
            (void) DisassemblyCells::FillCodeRow (cells, m_lines[(size_t) each.codeLine], m_options, colors);
        }

        if (DisassemblyCells::IsDimmed (m_options, source, each.codeLine < 0, isPc))
        {
            DisassemblyCells::DimRow (cells, theme.Foreground(), theme.TextViewBackground());
        }

        if (isPc)
        {
            cells[DisassemblyCells::kGutterColumn].icon = GutterGlyph::MakeMarked (nullptr, GutterGlyph::MakeArrow (pcMarkerArgb));
            fill                                        = colors.pcRow;
        }

        for (DxuiListView::Cell & cell : cells)
        {
            cell.background = fill;
        }

        rows.push_back (std::move (cells));
    }

    SetRows (std::move (rows));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyPreview::Paint
//
//  The rows are built in the theme's colors, then drawn, then the PC's branch
//  arrow over them when the Branch arrow option is on.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyPreview::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    const DxuiTheme          * dxuiTheme = dynamic_cast<const DxuiTheme *> (&theme);
    DebuggerTextColors::Set    colors    = (dxuiTheme != nullptr) ? m_colors.GetFor (*dxuiTheme)
                                                                  : DebuggerTextColors::Make (0xFF000000, 0xFFFFFFFF, 0xFFC0C0C0, 0u);
    uint32_t                   pcMarker  = DisassemblyCells::GetPcMarkerArgb (theme.ContentBackground());
    BranchArrow::Input         input;
    Word                       goesTo    = 0;
    bool                       isTaken   = true;



    BuildRows (colors, theme, pcMarker);

    DxuiListView::Paint (painter, text, theme);

    if (m_options.IsOn (DisassemblyOptions::Option::BranchArrow) &&
        DisassemblyCells::TryGetBranchArrow (*this, m_lines, m_rows, input, goesTo, isTaken))
    {
        DisassemblyCells::PaintBranchArrow (painter, input, isTaken, pcMarker, theme.ForegroundDisabled());
    }
}
