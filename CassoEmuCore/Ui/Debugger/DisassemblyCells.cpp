#include "Pch.h"

#include "Ui/Debugger/DisassemblyCells.h"
#include "Core/TextEncoding.h"
#include "Ui/Debugger/CodeMargins.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Ui/Debugger/Panes/SourcePane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::Configure
//
//  Every column but the last is fitted to its contents. The operand and
//  result fill the width after the other columns, never less than
//  kOperandFloorDip, and an annotation wider than that is cut off at the
//  pane's edge, so the pane scrolls sideways only when the columns ahead of
//  it don't fit. The glyph margin and the heat strip have fixed widths, the
//  same as a source document's (CodeMargins), since neither is text a fit
//  could measure; the glyph margin contains the breakpoints and the PC's
//  arrow. The rows are drawn on a text view's background.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyCells::Configure (DxuiListView & list)
{
    std::vector<DxuiListView::Column>  columns = { { L"",                   CodeMargins::kGlyphDip, false, DxuiTextHAlign::Left },
                                                   { L"Heat",               CodeMargins::kHeatDip,  false, DxuiTextHAlign::Left },
                                                   { L"Address",            0,                      false, DxuiTextHAlign::Left },
                                                   { L"Bytes",              0,                      false, DxuiTextHAlign::Left },
                                                   { L"Label",              0,                      false, DxuiTextHAlign::Left },
                                                   { L"Instruction",        0,                      false, DxuiTextHAlign::Left },
                                                   { L"Operand and result", kOperandFloorDip,       true,  DxuiTextHAlign::Left } };



    columns.back().tip                 = L"The values each instruction reads, then on the PC's line the registers, memory and flags it changes";
    columns[kHeatColumn].tip           = HeatStrip::GetHeadingTip (false).text;
    columns[kHeatColumn].tipLineScales = HeatStrip::GetHeadingTip (false).lineScales;
    columns[kHeatColumn].isText        = false;

    list.SetColumns         (std::move (columns));
    list.SetGlyphCenterDip  (CodeMargins::kGlyphCenterDip);
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
                                      const std::wstring & text, bool withNumber, const DebuggerTextColors::Set & colors, uint32_t numberArgb,
                                      const DxuiListView::Cell * operand)
{
    std::wstring                    prefix   = withNumber ? std::format (L"{:>5}  ", sourceLine) : std::wstring();
    std::wstring                    number   = withNumber ? std::format (L"{}", sourceLine) : std::wstring();
    std::vector<SourceSyntax::Run>  runs     = SourceSyntax::GetSourceRuns (text);
    bool                            isAll    = !text.empty() && (text[0] == L';' || text[0] == L'*');
    size_t                          labelEnd = (text.empty() || iswspace (text[0])) ? 0 : (isAll ? text.size() : text.find_first_of (L" \t"));
    size_t                          restAt   = 0;
    size_t                          column   = kAddressColumn;
    size_t                          comment  = 0;
    size_t                          codeEnd  = 0;
    int                             dragged  = -1;
    bool                            isNarrow = false;



    labelEnd = (labelEnd == std::wstring::npos) ? text.size() : labelEnd;
    restAt   = std::min (text.size(), text.find_first_not_of (L" \t", labelEnd));
    comment  = GetCommentStart (runs, restAt, text.size());
    codeEnd  = comment;

    while (codeEnd > restAt && iswspace (text[codeEnd - 1]))
    {
        codeEnd--;
    }

    //  A column the user dragged has a fixed width, so code too long for it
    //  runs on across the row, and the operand and result follow it.
    dragged  = list.GetColumnOverrideWidthPx (kInstructionColumn);
    isNarrow = dragged >= 0 && list.GetFittedWidthPx (codeEnd - restAt) > dragged;

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

        if (operand != nullptr)
        {
            AppendCell (cells[column], *operand);
        }

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

    cells[kLabelColumn].text     = prefix + text.substr (0, labelEnd);
    cells[kLabelColumn].trailDip = kLabelTrailDip;

    if (!prefix.empty())
    {
        cells[kLabelColumn].colorRanges.push_back ({ 0, (int) prefix.size(), numberArgb });
    }

    AddSourceRuns (cells[kLabelColumn], runs, 0, (int) labelEnd, (int) prefix.size(), colors);

    if (operand == nullptr || isNarrow)
    {
        cells[kInstructionColumn].text     = text.substr (restAt);
        cells[kInstructionColumn].spansRow = true;
        AddSourceRuns (cells[kInstructionColumn], runs, (int) restAt, (int) text.size(), 0, colors);

        if (operand != nullptr)
        {
            AppendCell (cells[kInstructionColumn], *operand);
        }

        return;
    }

    cells[kInstructionColumn].text = text.substr (restAt, codeEnd - restAt);
    AddSourceRuns (cells[kInstructionColumn], runs, (int) restAt, (int) codeEnd, 0, colors);

    cells[kOperandColumn] = *operand;

    if (comment < text.size())
    {
        DxuiListView::Cell  remark;

        remark.text = text.substr (comment);
        AddSourceRuns (remark, runs, (int) comment, (int) text.size(), 0, colors);
        AppendCell (cells[kOperandColumn], remark);
        cells[kOperandColumn].spansRow = true;
    }
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
//  DisassemblyCells::AppendCell
//
//  The added cell's color covers its text up to its first range, so its
//  plain text is in that color in a cell colored differently.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyCells::AppendCell (DxuiListView::Cell & cell, const DxuiListView::Cell & added)
{
    int  at    = (int) cell.text.size() + (cell.text.empty() ? 0 : 2);
    int  plain = added.colorRanges.empty() ? (int) added.text.size() : std::get<0> (added.colorRanges.front());



    if (added.text.empty())
    {
        return;
    }

    cell.text += cell.text.empty() ? L"" : L"  ";
    cell.text += added.text;

    if (added.argb != 0 && plain > 0)
    {
        cell.colorRanges.push_back ({ at, at + plain, added.argb });
    }

    for (const auto & [first, end, argb] : added.colorRanges)
    {
        cell.colorRanges.push_back ({ at + first, at + end, argb });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::GetCommentStart
//
////////////////////////////////////////////////////////////////////////////////

size_t DisassemblyCells::GetCommentStart (const std::vector<SourceSyntax::Run> & runs, size_t from, size_t end)
{
    for (const SourceSyntax::Run & run : runs)
    {
        if (run.token == SourceSyntax::Token::Comment && (size_t) run.start >= from)
        {
            return (std::min) ((size_t) run.start, end);
        }
    }

    return end;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::TryGetSourceRowSpan
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyCells::TryGetSourceRowSpan (const std::vector<DisassemblyOptions::Row> & rows, size_t row,
                                            const std::vector<DebuggerViewSnapshot::CodeLine> & lines, Word & first, size_t & count)
{
    bool  isSpan = false;



    count = 0;

    if (row >= rows.size() || rows[row].codeLine >= 0)
    {
        return false;
    }

    for (size_t i = row + 1; i < rows.size(); i++)
    {
        const DisassemblyOptions::Row  & each = rows[i];

        if (each.codeLine < 0 || each.fileId != rows[row].fileId || each.sourceLine != rows[row].sourceLine)
        {
            break;
        }

        const DebuggerViewSnapshot::CodeLine  & line = lines[(size_t) each.codeLine];

        first  = isSpan ? first : line.address;
        isSpan = true;
        count  = (size_t) (Word) (line.address - first) + std::max<size_t> (1, HeatStrip::CountBytes (line.bytes));
    }

    return isSpan;
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
    cells[kLabelColumn].trailDip   = kLabelTrailDip;
    cells[kInstructionColumn].text = TextEncoding::NarrowToWide (DisassemblyOptions::GetInstructionText (line, symbols));
    cells[kOperandColumn]          = GetOperandAndResultCell (annotation, line.effect, colors.result);

    cells[kOperandColumn].argb     = colors.annotation;
    cells[kLabelColumn].argb       = colors.syntax.symbol;
    cells[kAddressColumn].argb     = colors.syntax.address;
    cells[kBytesColumn].argb       = colors.syntax.bytes;
    cells[kInstructionColumn].argb = colors.syntax.address;

    cells[kInstructionColumn].colorRanges = DebuggerTextColors::GetInstructionRanges (cells[kInstructionColumn].text, colors);
    cells[kInstructionColumn].weight      = (line.dataOrigin != DataOrigin::None) ? DxuiFontWeight::Italic : DxuiFontWeight::Normal;

    //  A data line has no operand or result, and its text, often a string,
    //  runs on across the operand column rather than being cut off.
    cells[kInstructionColumn].spansRow    = line.dataOrigin != DataOrigin::None;

    return annotation;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::GetDataTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DisassemblyCells::GetDataTip (DataOrigin origin)
{
    switch (origin)
    {
    case DataOrigin::Marked:    return L"Marked as data. To disassemble these bytes as code, choose Clear data marking on the row's menu.";
    case DataOrigin::DebugFile: return L"Data: the debug file maps these bytes to a data directive. To disassemble them as code, choose Clear data marking on the row's menu.";
    default:                    return std::wstring();
    }
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
//  DisassemblyCells::TryGetPcArrowEnds
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyCells::TryGetPcArrowEnds (const std::vector<DebuggerViewSnapshot::CodeLine> & lines, ArrowEnds & ends)
{
    for (const DebuggerViewSnapshot::CodeLine & line : lines)
    {
        if (line.isCurrent && line.target.has_value())
        {
            ends = { line.address, *line.target, line.isTargetTaken };
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::TryGetBranchArrow
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyCells::TryGetBranchArrow (const DxuiListView & list, const std::vector<DebuggerViewSnapshot::CodeLine> & lines,
                                          const std::vector<DisassemblyOptions::Row> & rows, const ArrowEnds & ends, BranchArrow::Input & input)
{
    int    current  = -1;
    int    target   = -1;
    int    beyond   = -1;
    int    firstRow = 0;
    int    endRow   = 0;
    RECT   bounds   = {};
    RECT   column   = {};
    RECT   cell     = {};
    float  scale    = 1.0f;
    float  top      = 0.0f;
    float  bottom   = 0.0f;
    int    rowPx    = 0;
    bool   isAbove  = false;



    firstRow = list.GetTopRow();

    if (lines.empty() || !list.GetCellTextRectPx (firstRow, kInstructionColumn, column))
    {
        return false;
    }

    //  Indexes are rows: a source row above an instruction moves the
    //  instruction down a row, and an instruction under the source row
    //  representing it has its arrow there.
    for (size_t i = 0; i < lines.size(); i++)
    {
        current = (lines[i].address == ends.from)   ? DisassemblyOptions::GetFrontRowOfLine (rows, (int) i) : current;
        target  = (lines[i].address == ends.goesTo) ? DisassemblyOptions::GetFrontRowOfLine (rows, (int) i) : target;
    }

    if (current < 0 && !GetOutsideRow (list, lines, ends.from, current))
    {
        return false;
    }

    bounds = list.GetBounds();
    rowPx  = column.bottom - column.top;
    scale  = (float) rowPx / (float) (std::max) (1, list.GetRowHeightDip());
    endRow = firstRow + (std::min) (list.GetVisibleRowCapacity(), list.GetRowCount() - firstRow);
    top    = (float) (bounds.top + column.top);
    bottom = top + (float) ((endRow - firstRow) * rowPx);

    input.mnemonicX     = (float) (bounds.left + column.left);
    input.isTargetBelow = (target >= 0) ? target > current : ends.goesTo > ends.from;
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
        isAbove = current < firstRow;
        beyond  = target;

        if (beyond < 0 && !GetOutsideRow (list, lines, ends.goesTo, beyond))
        {
            return false;
        }

        if (isAbove != input.isTargetBelow || (isAbove ? beyond < firstRow : beyond >= endRow))
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
//  DisassemblyCells::GetOutsideRow
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyCells::GetOutsideRow (const DxuiListView & list, const std::vector<DebuggerViewSnapshot::CodeLine> & lines, Word address, int & row)
{
    if (address < lines.front().address)
    {
        row = -1;
        return true;
    }

    if (address > lines.back().address)
    {
        row = list.GetRowCount();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells::SeparateArrows
//
//  The stepped arrow ends at the PC's row and the PC's starts there. The
//  stepped one ends a little to the side of the row toward its source, and
//  the PC's starts a little to the other side. Run the same way, as from a
//  branch below to a PC whose target is below too, the two would share a
//  lane; the PC's is moved out a lane, and since it starts on the side away
//  from the stepped one's end, the two lines don't cross.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyCells::SeparateArrows (BranchArrow::Input & stepped, BranchArrow::Input & pc)
{
    float  scale  = pc.radiusPx / BranchArrow::Input().radiusPx;
    float  offset = (stepped.isTargetBelow ? -kArrowSplitDip : kArrowSplitDip) * scale;



    if (stepped.targetY.has_value())
    {
        *stepped.targetY += offset;
    }

    if (pc.sourceY.has_value())
    {
        *pc.sourceY -= offset;
    }

    if (stepped.isTargetBelow != pc.isTargetBelow)
    {
        pc.stubPx += kArrowLaneDip * scale;
    }
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