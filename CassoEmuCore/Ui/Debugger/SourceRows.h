#pragma once

#include "Ui/Debugger/DebuggerViewSnapshot.h"
#include "Ui/Debugger/Panes/HeatMapView.h"
#include "Ui/Debugger/SourceRowStyle.h"
#include "Ui/Debugger/SourceSyntax.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceRows
//
//  A source file's text as a text view's rows: its lines, each row's marker,
//  line number, text and annotation cells, and the heat strips in the
//  gutter. Apart from any view.
//
////////////////////////////////////////////////////////////////////////////////

class SourceRows
{
public:
    //  What an operand's result is shown after, here and in the disassembly.
    static constexpr const wchar_t * kpszResultPrefix = L"Result: ";

    //  A row's cells: the marker, the line number, the source text, and the
    //  annotation. The text's column is no wider than kAnnotationColumn, so
    //  the annotations start near the code, and only a longer line pushes
    //  that line's annotation along. An annotation is cut off at the view's
    //  right edge, with its tip over it, rather than scrolled to.
    static constexpr int    kTextCell         = 2;
    static constexpr int    kAnnotationCell   = 3;
    static constexpr int    kAnnotationColumn = 40;

    //  The line numbers are followed by room for the branch arrow, which
    //  stops short of the text.
    static constexpr int    kNumberCell       = 1;
    static constexpr int    kArrowGapCells    = 5;
    static constexpr float  kArrowMarginDip   = 6.0f;

    static std::vector<std::wstring>       SplitLines (const std::string & text);
    static std::vector<DxuiTextView::Row>  BuildRows  (const std::vector<std::wstring> & lines, int markedLine,
                                                       const std::set<int> & breakpointLines,
                                                       const std::set<int> & disabledLines = {},
                                                       const SourceRowStyle & style        = {},
                                                       const std::map<int, std::pair<std::wstring, std::wstring>> & lineOperands = {},
                                                       SourceSyntax::Assembler assembler = SourceSyntax::Assembler::Any,
                                                       SourceSyntax::Listing   listing   = SourceSyntax::Listing::None,
                                                       bool                    isC       = false,
                                                       int                     hoverLine = -1);

    //  The source line each row shows.
    static std::vector<int>  GetRowLines (const std::vector<DxuiTextView::Row> & rows);

    //  The heat strips of the rows, one a line from line 1, as the gutter
    //  band's fills.
    static void  AddHeatStrips (std::vector<DxuiTextView::Row> & rows, int fileId,
                                const std::map<std::pair<int, int>, std::pair<Word, Word>> & lineRanges,
                                const DebuggerViewSnapshot::HeatColumnLevels & levels, const HeatMapView::Palette & palette,
                                const std::set<std::pair<int, int>> * unmatched = nullptr);

private:
    static constexpr int  kTabWidth = 4;
};
