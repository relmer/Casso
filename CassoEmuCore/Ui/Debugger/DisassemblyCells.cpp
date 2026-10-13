#include "Pch.h"

#include "Ui/Debugger/DisassemblyCells.h"
#include "Core/TextEncoding.h"
#include "Ui/Debugger/Panes/SourcePane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::Configure
//
//  Every column is fitted to its contents and none stretches, so a pane is
//  exactly as wide as its text (FR-026a). The glyph margin alone has a set
//  width, since its glyphs are not text a fit could measure; it contains the
//  breakpoints and the PC's arrow, and the address column follows it. The
//  rows are drawn on a text view's background.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyCells::Configure (DxuiListView & list)
{
    list.SetColumns ({ { L"",                   kGutterColumnDip, false, DxuiTextHAlign::Left },
                       { L"Address",            0,                false, DxuiTextHAlign::Left },
                       { L"Bytes",              0,                false, DxuiTextHAlign::Left },
                       { L"Label",              0,                false, DxuiTextHAlign::Left },
                       { L"Instruction",        0,                false, DxuiTextHAlign::Left },
                       { L"Operand and result", 0,                false, DxuiTextHAlign::Left } });

    list.SetGlyphCenterDip  (kGlyphCenterDip);
    list.SetTextViewSurface (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::ShowColumns
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyCells::ShowColumns (DxuiListView & list, const DisassemblyOptions & options)
{
    list.SetColumnVisible (kAddressColumn, options.IsOn (DisassemblyOptions::Option::Addresses));
    list.SetColumnVisible (kBytesColumn,   options.IsOn (DisassemblyOptions::Option::CodeBytes));
    list.SetColumnVisible (kLabelColumn,   options.IsOn (DisassemblyOptions::Option::Symbols));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::FillSourceRow
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyCells::FillSourceRow (std::vector<DxuiListView::Cell> & cells, const DxuiListView & list, int sourceLine,
                                      const std::wstring & text, bool withNumber, const DebuggerTextColors::Set & colors, uint32_t numberArgb)
{
    std::wstring  prefix = withNumber ? std::format (L"{:>5}  ", sourceLine) : std::wstring();
    size_t        column = kAddressColumn;



    while (column < kColumnCount - 1 && !list.IsColumnVisible (column))
    {
        column++;
    }

    cells[column].text     = prefix + text;
    cells[column].spansRow = true;

    if (withNumber)
    {
        cells[column].colorRanges.push_back ({ 0, (int) prefix.size(), numberArgb });
    }

    for (const SourceSyntax::Run & run : SourceSyntax::GetSourceRuns (text))
    {
        cells[column].colorRanges.push_back ({ (int) prefix.size() + run.start, (int) prefix.size() + run.start + run.length, colors.syntax.Get (run.token) });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::FillCodeRow
//
////////////////////////////////////////////////////////////////////////////////

std::string DisassemblyCells::FillCodeRow (std::vector<DxuiListView::Cell> & cells, const DebuggerViewSnapshot::CodeLine & line,
                                           const DisassemblyOptions & options, const DebuggerTextColors::Set & colors)
{
    bool         symbols    = options.IsOn (DisassemblyOptions::Option::Symbols);
    bool         colon      = options.IsOn (DisassemblyOptions::Option::AddressColon);
    std::string  annotation = DebuggerViewState::ComposeAnnotation (line.annotationParts,
                                                                    options.IsOn (DisassemblyOptions::Option::OperandValues),
                                                                    options.IsOn (DisassemblyOptions::Option::Pointers));



    cells[kAddressColumn].text     = std::format (L"{:04X}{}", line.address, colon ? L":" : L"");
    cells[kBytesColumn].text       = TextEncoding::NarrowToWide (line.bytes);
    cells[kLabelColumn].text       = TextEncoding::NarrowToWide (line.label);
    cells[kInstructionColumn].text = TextEncoding::NarrowToWide (DisassemblyOptions::GetInstructionText (line, symbols));
    cells[kOperandColumn]          = GetOperandAndResultCell (annotation, line.effect, colors.result);

    cells[kOperandColumn].argb     = colors.annotation;
    cells[kLabelColumn].argb       = colors.syntax.symbol;
    cells[kAddressColumn].argb     = colors.syntax.address;
    cells[kBytesColumn].argb       = colors.syntax.bytes;
    cells[kInstructionColumn].argb = colors.syntax.address;

    cells[kInstructionColumn].colorRanges = DebuggerTextColors::GetInstructionRanges (cells[kInstructionColumn].text, colors);

    return annotation;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::IsDimmed
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyCells::IsDimmed (const DisassemblyOptions & options, bool isSourceShown, bool isSourceRow, bool isPcRow)
{
    DisassemblyOptions::Dim  kind = isSourceRow ? DisassemblyOptions::Dim::Source : DisassemblyOptions::Dim::Code;



    return isSourceShown && !isPcRow && options.GetDim() == kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::DimRow
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyCells::DimRow (std::vector<DxuiListView::Cell> & cells, uint32_t foregroundArgb, uint32_t backgroundArgb)
{
    for (DxuiListView::Cell & cell : cells)
    {
        cell.argb = DxuiColor::Mix ((cell.argb != 0) ? cell.argb : foregroundArgb, backgroundArgb, kDimMix);

        for (auto & [first, end, argb] : cell.colorRanges)
        {
            argb = DxuiColor::Mix (argb, backgroundArgb, kDimMix);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::GetOperandAndResultCell
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::Cell DisassemblyCells::GetOperandAndResultCell (const std::string & annotation, const std::string & effect, uint32_t resultArgb)
{
    DxuiListView::Cell  cell;
    std::wstring        result = TextEncoding::NarrowToWide (effect);



    cell.text = TextEncoding::NarrowToWide (annotation);

    if (!result.empty())
    {
        result     = SourcePane::kpszResultPrefix + result;
        cell.text += cell.text.empty() ? L"" : L"  ";
        cell.colorRanges.emplace_back ((int) cell.text.size(), (int) (cell.text.size() + result.size()), resultArgb);
        cell.text += result;
    }

    return cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::GetPcMarkerArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DisassemblyCells::GetPcMarkerArgb (uint32_t background)
{
    return DebuggerTextColors::IsDark (background) ? 0xFFFFE34D : 0xFFA88300;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::TryGetBranchArrow
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyCells::TryGetBranchArrow (const DxuiListView & list, const std::vector<DebuggerViewSnapshot::CodeLine> & lines,
                                          const std::vector<DisassemblyOptions::Row> & rows, BranchArrow::Input & input, Word & goesTo, bool & isTaken)
{
    int    current  = -1;
    int    target   = -1;
    int    firstRow = 0;
    int    endRow   = 0;
    RECT   bounds   = {};
    RECT   column   = {};
    RECT   cell     = {};
    float  scale    = 1.0f;
    float  top      = 0.0f;
    float  bottom   = 0.0f;
    int    rowPx    = 0;



    for (size_t i = 0; i < lines.size(); i++)
    {
        current = lines[i].isCurrent ? (int) i : current;
    }

    firstRow = list.GetTopRow();

    if (current < 0 || !lines[(size_t) current].target.has_value() ||
        !list.GetCellTextRectPx (firstRow, kInstructionColumn, column))
    {
        return false;
    }

    goesTo  = *lines[(size_t) current].target;
    isTaken = lines[(size_t) current].isTargetTaken;

    for (size_t i = 0; i < lines.size(); i++)
    {
        target = (lines[i].address == goesTo) ? DisassemblyOptions::GetRowOfLine (rows, (int) i) : target;
    }

    //  From here on, indexes are rows: a source row above an instruction moves
    //  the instruction down a row.
    current = DisassemblyOptions::GetRowOfLine (rows, current);

    bounds = list.GetBounds();
    rowPx  = column.bottom - column.top;
    scale  = (float) rowPx / (float) (std::max) (1, list.GetRowHeightDip());
    endRow = firstRow + (std::min) (list.GetVisibleRowCapacity(), list.GetRowCount() - firstRow);
    top    = (float) (bounds.top + column.top);
    bottom = top + (float) ((endRow - firstRow) * rowPx);

    input.mnemonicX     = (float) (bounds.left + column.left);
    input.isTargetBelow = (target >= 0) ? target > current : goesTo > lines[(size_t) DisassemblyOptions::GetLineOfRow (rows, current)].address;
    input.edgeY         = input.isTargetBelow ? bottom : top;
    input.sourceEdgeY   = input.isTargetBelow ? top : bottom;
    input.marginPx     *= scale;
    input.stubPx       *= scale;
    input.radiusPx     *= scale;
    input.headPx       *= scale;
    input.slopPx       *= scale;

    //  A source out of view counts only while the line runs on across the
    //  rows: above them toward a target below, or below toward one above.
    if (current < firstRow || current >= endRow)
    {
        bool  above = current < firstRow;

        if (above != input.isTargetBelow ||
            (target >= 0 && (above ? target < firstRow : target >= endRow)))
        {
            return false;
        }
    }
    else if (list.GetCellTextRectPx (current, kInstructionColumn, cell))
    {
        input.sourceY = (float) (bounds.top + (cell.top + cell.bottom) / 2);
    }

    if (target >= 0 && list.GetCellTextRectPx (target, kInstructionColumn, cell))
    {
        input.targetY = (float) (bounds.top + (cell.top + cell.bottom) / 2);
    }

    //  Scrolled sideways past the mnemonics, there is no room for it.
    return input.mnemonicX - input.marginPx - input.stubPx >= (float) bounds.left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::PaintBranchArrow
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyCells::PaintBranchArrow (IDxuiPainter & painter, const BranchArrow::Input & input, bool isTaken, uint32_t takenArgb, uint32_t notTakenArgb)
{
    BranchArrow::Result  arrow = BranchArrow::Build (input);
    uint32_t             argb  = isTaken ? takenArgb : notTakenArgb;
    float                width = 1.5f * input.radiusPx / BranchArrow::Input().radiusPx;



    for (const BranchArrow::Segment & segment : arrow.segments)
    {
        painter.DrawLine (segment.x0, segment.y0, segment.x1, segment.y1, width, argb);
    }

    if (arrow.hasHead)
    {
        painter.FillConvexQuad (arrow.head[0], arrow.head[1], arrow.head[2], arrow.head[3],
                                arrow.head[4], arrow.head[5], arrow.head[4], arrow.head[5], argb);
    }
}
