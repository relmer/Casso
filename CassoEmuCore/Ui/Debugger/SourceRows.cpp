#include "Pch.h"

#include "Ui/Debugger/SourceRows.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/GutterGlyph.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Ui/Debugger/SourceSyntax.h"
#include "Ui/Debugger/SourceLineText.h"

#include "Debugger/Source/SourcePathList.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceRows::AddHeatStrips
//
//  One fill of the hottest byte's color in the gutter's heat band, as the
//  disassembly fills its heat cell. A line without code, whose bytes nothing
//  touched, or whose bytes in memory are other code's now, has no fill.
//
////////////////////////////////////////////////////////////////////////////////

void SourceRows::AddHeatStrips (std::vector<DxuiTextView::Row> & rows, int fileId,
                                const std::map<std::pair<int, int>, std::pair<Word, Word>> & lineRanges,
                                const DebuggerViewSnapshot::HeatColumnLevels & levels, const HeatMapView::Palette & palette,
                                const std::set<std::pair<int, int>> * unmatched)
{
    for (size_t i = 0; i < rows.size(); i++)
    {
        auto  found = lineRanges.find ({ fileId, (int) i + 1 });

        if (found != lineRanges.end() && (unmatched == nullptr || !unmatched->contains ({ fileId, (int) i + 1 })))
        {
            rows[i].gutterFill = HeatStrip::Get (levels, found->second.first, (size_t) (found->second.second - found->second.first) + 1, palette, false).hottest;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceRows::SplitLines
//
//  Lines ending in LF, CR LF or CR, with tabs expanded to every fourth
//  column: the view reads a tab as the break between cells.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> SourceRows::SplitLines (const std::string & text)
{
    std::vector<std::wstring>  lines;
    std::string                line;
    auto                       flush = [&] ()
                                       {
                                           std::wstring  wide = SourcePathList::Utf8ToWide (line);
                                           std::wstring  expanded;

                                           for (wchar_t c : wide)
                                           {
                                               if (c != L'\t')
                                               {
                                                   expanded += c;
                                                   continue;
                                               }

                                               expanded.append ((size_t) (kTabWidth - (int) expanded.size() % kTabWidth), L' ');
                                           }

                                           lines.push_back (std::move (expanded));
                                           line.clear();
                                       };



    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] == '\r' || text[i] == '\n')
        {
            flush();

            if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
            {
                i++;
            }

            continue;
        }

        line += text[i];
    }

    if (!line.empty())
    {
        flush();
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceRows::BuildRows
//
//  A marker, the line number and the text. The marked line takes the PC's
//  row fill. With the style's icons, as the disassembly pane draws them, a
//  breakpoint is its icon in the view's glyph margin, and with its PC arrow
//  the marked line's arrow is there too, drawn over the line's breakpoint
//  when it has one, so both sit where Visual Studio puts them. Without the
//  icons a breakpoint is a bullet in the marker, and without the arrow the
//  marked line is a triangle there in the marker color.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTextView::Row> SourceRows::BuildRows (const std::vector<std::wstring> & lines, int markedLine,
                                                      const std::set<int> & breakpointLines,
                                                      const std::set<int> & disabledLines,
                                                      const SourceRowStyle & style,
                                                      const std::map<int, std::pair<std::wstring, std::wstring>> & lineOperands,
                                                      SourceSyntax::Assembler assembler,
                                                      SourceSyntax::Listing   listing,
                                                      bool                    isC,
                                                      int                     hoverLine)
{
    std::vector<DxuiTextView::Row>  rows;
    int                             width       = (int) std::to_wstring (lines.size()).size();
    bool                            icons       = style.enabledIcon != nullptr;
    bool                            arrow       = style.pcIcon != nullptr;
    bool                            isInComment = false;



    for (size_t i = 0; i < lines.size(); i++)
    {
        int                number   = (int) i + 1;
        bool               isMarked = number == markedLine;
        std::wstring       marker;
        DxuiTextView::Row  row;

        if (!icons)
        {
            marker = breakpointLines.contains (number) ? std::wstring (1, s_kchBullet) : std::wstring (L" ");
        }

        if (!arrow)
        {
            marker += isMarked ? std::wstring (s_kpszTriangleRight) : std::wstring (L" ");
        }

        row.cells = { marker, std::format (L"{:>{}}", number, width), lines[i] };

        if (icons && breakpointLines.contains (number))
        {
            row.icon = disabledLines.contains (number) ? style.disabledIcon : style.enabledIcon;
        }
        else if (number == hoverLine)
        {
            row.icon = style.hoverIcon;
        }

        if (isMarked)
        {
            row.background = style.pcRowArgb;
            row.icon       = arrow ? GutterGlyph::MakeMarked (row.icon, style.pcIcon) : row.icon;

            if (!arrow && style.pcMarkerArgb != 0)
            {
                row.spans.push_back ({ 0, (int) marker.size() - 1, 1, style.pcMarkerArgb });
            }
        }

        //  What the line's instruction reads in the operand color, then the
        //  result in the result color, as in the disassembly pane's operand
        //  column.
        //  Only on a line whose opcode is a mnemonic: a directive or a macro
        //  produced its bytes some other way than one instruction.
        if (!lineOperands.empty())
        {
            auto  found  = lineOperands.find (number);
            int   column = (int) row.cells.size();

            row.cells.emplace_back();

            if (found != lineOperands.end() && SourceLineText::IsInstructionLine (lines[i], assembler))
            {
                row.cells.back() = found->second.first;

                if (style.operandArgb != 0 && !row.cells.back().empty())
                {
                    row.spans.push_back ({ column, 0, (int) row.cells.back().size(), style.operandArgb });
                }

                if (!found->second.second.empty())
                {
                    std::wstring  result = kpszResultPrefix + found->second.second;

                    row.cells.back() += row.cells.back().empty() ? L"" : L"  ";

                    if (style.resultArgb != 0)
                    {
                        row.spans.push_back ({ column, (int) row.cells.back().size(), (int) result.size(), style.resultArgb });
                    }

                    row.cells.back() += result;
                }
            }
        }

        //  A C block comment can run over lines, so every C line is read in
        //  order, colored or not.
        if (isC)
        {
            std::vector<SourceSyntax::Run>  runs = SourceSyntax::GetCRuns (lines[i], isInComment);

            for (const SourceSyntax::Run & run : runs)
            {
                if (style.syntax.mnemonic != 0)
                {
                    row.spans.push_back ({ 2, run.start, run.length, style.syntax.Get (run.token) });
                }
            }
        }
        else if (style.syntax.mnemonic != 0)
        {
            for (const SourceSyntax::Run & run : SourceSyntax::GetLineRuns (lines[i], assembler, listing))
            {
                row.spans.push_back ({ 2, run.start, run.length, style.syntax.Get (run.token) });
            }
        }

        rows.push_back (std::move (row));
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceRows::GetRowLines
//
//  A row with a line number is the next source line; one without is a row of
//  the instructions under it.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> SourceRows::GetRowLines (const std::vector<DxuiTextView::Row> & rows)
{
    std::vector<int>  lines;
    int               line  = 0;



    for (const DxuiTextView::Row & row : rows)
    {
        bool  isLine = row.cells.size() > 1 && row.cells[1].find_first_not_of (L' ') != std::wstring::npos;

        line += isLine ? 1 : 0;
        lines.push_back (isLine ? line : 0);
    }

    return lines;
}
