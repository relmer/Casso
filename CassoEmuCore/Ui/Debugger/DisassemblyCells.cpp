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
    std::vector<DxuiListView::Column>  columns = { { L"",                   kGutterColumnDip, false, DxuiTextHAlign::Left },
                                                   { L"Heat",               0,                false, DxuiTextHAlign::Left },
                                                   { L"Address",            0,                false, DxuiTextHAlign::Left },
                                                   { L"Bytes",              0,                false, DxuiTextHAlign::Left },
                                                   { L"Label",              0,                false, DxuiTextHAlign::Left },
                                                   { L"Instruction",        0,                false, DxuiTextHAlign::Left },
                                                   { L"Operand and result", 0,                false, DxuiTextHAlign::Left } };



    columns.back().tip         = L"The values each instruction reads, then on the PC's line the registers, memory and flags it changes";
    columns[kHeatColumn].tip   = L"How hot the line's bytes are, in the heat map's colors: the code color for bytes run, the read or write color for bytes read or written as data";

    list.SetColumns         (std::move (columns));
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
    list.SetColumnVisible (kHeatColumn,    options.IsOn (DisassemblyOptions::Option::Heat));
    list.SetColumnVisible (kAddressColumn, options.IsOn (DisassemblyOptions::Option::Addresses));
    list.SetColumnVisible (kBytesColumn,   options.IsOn (DisassemblyOptions::Option::CodeBytes));
    list.SetColumnVisible (kLabelColumn,   options.IsOn (DisassemblyOptions::Option::Symbols));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::FillSourceRow
//
//  A line that starts with a space has no label; a comment line, which starts
//  with ; or *, is all in the label column, as a label is.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyCells::FillSourceRow (std::vector<DxuiListView::Cell> & cells, const DxuiListView & list, int sourceLine,
                                      const std::wstring & text, bool withNumber, const DebuggerTextColors::Set & colors, uint32_t numberArgb)
{
    std::wstring                    prefix   = withNumber ? std::format (L"{:>5}  ", sourceLine) : std::wstring();
    std::wstring                    number   = withNumber ? std::format (L"{}", sourceLine) : std::wstring();
    std::vector<SourceSyntax::Run>  runs     = SourceSyntax::GetSourceRuns (text);
    bool                            isAll    = !text.empty() && (text[0] == L';' || text[0] == L'*');
    size_t                          labelEnd = (text.empty() || iswspace (text[0])) ? 0 : (isAll ? text.size() : text.find_first_of (L" \t"));
    size_t                          restAt   = 0;
    size_t                          column   = kAddressColumn;



    labelEnd = (labelEnd == std::wstring::npos) ? text.size() : labelEnd;
    restAt   = std::min (text.size(), text.find_first_not_of (L" \t", labelEnd));

    if (!list.IsColumnVisible (kLabelColumn))
    {
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

        AddSourceRuns (cells[column], runs, 0, (int) text.size(), (int) prefix.size(), colors);
        return;
    }

    //  The number in the address column, or the bytes column while the
    //  address is hidden, and else before the label.
    column = list.IsColumnVisible (kAddressColumn) ? kAddressColumn : list.IsColumnVisible (kBytesColumn) ? kBytesColumn : kLabelColumn;

    if (withNumber && column != kLabelColumn)
    {
        cells[column].text = number;
        cells[column].colorRanges.push_back ({ 0, (int) number.size(), numberArgb });
        prefix.clear();
    }

    cells[kLabelColumn].text = prefix + text.substr (0, labelEnd);

    if (!prefix.empty())
    {
        cells[kLabelColumn].colorRanges.push_back ({ 0, (int) prefix.size(), numberArgb });
    }

    AddSourceRuns (cells[kLabelColumn], runs, 0, (int) labelEnd, (int) prefix.size(), colors);

    cells[kInstructionColumn].text     = text.substr (restAt);
    cells[kInstructionColumn].spansRow = true;
    AddSourceRuns (cells[kInstructionColumn], runs, (int) restAt, (int) text.size(), 0, colors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::AddSourceRuns
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyCells::AddSourceRuns (DxuiListView::Cell & cell, const std::vector<SourceSyntax::Run> & runs, int start, int end, int offset,
                                      const DebuggerTextColors::Set & colors)
{
    for (const SourceSyntax::Run & run : runs)
    {
        int  first = std::max (run.start, start);
        int  last  = std::min (run.start + run.length, end);

        if (first < last)
        {
            cell.colorRanges.push_back ({ offset + first - start, offset + last - start, colors.syntax.Get (run.token) });
        }
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
                                          const std::vector<DisassemblyOptions::Row> & rows, BranchArrow::Input & input, Word & goesTo, bool & isTaken,
                                          std::optional<Word> branchedFrom)
{
    int    current  = -1;
    int    pcLine   = -1;
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
        pcLine  = lines[i].isCurrent ? (int) i : pcLine;
        current = (branchedFrom.has_value() ? lines[i].address == *branchedFrom : lines[i].isCurrent) ? (int) i : current;
    }

    firstRow = list.GetTopRow();

    if (lines.empty() || !list.GetCellTextRectPx (firstRow, kInstructionColumn, column))
    {
        return false;
    }

    //  The stepped branch went to the PC, which its snapshot made sure of;
    //  the PC's own goes to its target.
    if (branchedFrom.has_value())
    {
        if (pcLine < 0)
        {
            return false;
        }

        goesTo  = lines[(size_t) pcLine].address;
        isTaken = true;
    }
    else
    {
        if (current < 0 || !lines[(size_t) current].target.has_value())
        {
            return false;
        }

        goesTo  = *lines[(size_t) current].target;
        isTaken = lines[(size_t) current].isTargetTaken;
    }

    for (size_t i = 0; i < lines.size(); i++)
    {
        target = (lines[i].address == goesTo) ? DisassemblyOptions::GetRowOfLine (rows, (int) i) : target;
    }

    //  From here on, indexes are rows: a source row above an instruction moves
    //  the instruction down a row. A stepped source not among the lines lies
    //  above them or below, by its address.
    if (current >= 0)
    {
        current = DisassemblyOptions::GetRowOfLine (rows, current);
    }
    else
    {
        current = (*branchedFrom < lines.front().address) ? -1 : list.GetRowCount();
    }

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

void DisassemblyCells::PaintBranchArrow (IDxuiPainter & painter, const BranchArrow::Input & input, bool isTaken, uint32_t takenArgb, uint32_t notTakenArgb,
                                         bool isDotted)
{
    BranchArrow::Result  arrow = BranchArrow::Build (input);
    uint32_t             argb  = isTaken ? takenArgb : notTakenArgb;
    float                width = 1.5f * input.radiusPx / BranchArrow::Input().radiusPx;
    float                along = 0.0f;



    for (const BranchArrow::Segment & segment : arrow.segments)
    {
        if (isDotted)
        {
            PaintDots (painter, segment, width, argb, along);
        }
        else
        {
            painter.DrawLine (segment.x0, segment.y0, segment.x1, segment.y1, width, argb);
        }
    }

    if (arrow.hasHead)
    {
        painter.FillConvexQuad (arrow.head[0], arrow.head[1], arrow.head[2], arrow.head[3],
                                arrow.head[4], arrow.head[5], arrow.head[4], arrow.head[5], argb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::PaintDots
//
//  A segment's dots, `along` the distance drawn so far, so the spacing runs
//  on unbroken through the rounded corners, which are short segments: a dot
//  is centered at every multiple of two widths along the whole arrow.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyCells::PaintDots (IDxuiPainter & painter, const BranchArrow::Segment & segment, float width, uint32_t argb, float & along)
{
    float  dx     = segment.x1 - segment.x0;
    float  dy     = segment.y1 - segment.y0;
    float  length = std::sqrt (dx * dx + dy * dy);
    float  period = 2.0f * width;
    float  at     = 0.0f;



    if (length > 0.0f && period > 0.0f)
    {
        for (at = std::ceil (along / period) * period - along; at <= length; at += period)
        {
            painter.FillCircle (segment.x0 + dx * at / length, segment.y0 + dy * at / length, width / 2.0f, argb);
        }
    }

    along += length;
}