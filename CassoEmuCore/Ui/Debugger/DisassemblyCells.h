#pragma once

#include "Pch.h"

#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/DisassemblyOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCells
//
//  The columns and row text of a disassembly list, and the branch arrow drawn
//  over it. The disassembly views and the Options sheet's preview both build
//  their rows here, so the same options produce the same rows in each.
//
////////////////////////////////////////////////////////////////////////////////

class DisassemblyCells
{
public:
    static constexpr float   kGlyphCenterDip    = 8.4f;
    static constexpr int     kGutterColumnDip   = 34;     // a breakpoint's dot, then the PC's arrow
    static constexpr size_t  kGutterColumn      = 0;
    static constexpr size_t  kHeatColumn        = 1;
    static constexpr size_t  kAddressColumn     = 2;
    static constexpr size_t  kBytesColumn       = 3;
    static constexpr size_t  kLabelColumn       = 4;
    static constexpr size_t  kInstructionColumn = 5;
    static constexpr size_t  kOperandColumn     = 6;
    static constexpr size_t  kColumnCount       = 7;
    static constexpr float   kDimMix            = 0.85f;
    static constexpr float   kArrowSplitDip     = 3.0f;
    static constexpr float   kArrowLaneDip      = 6.0f;

    //  Clear room at the right of a label cell for the branch arrow's
    //  lanes, the outer one up to 21 DIPs left of the mnemonics, less the
    //  list's 4 DIP padding on either side of the column edge, and a gap.
    static constexpr int     kLabelTrailDip     = 15;

    //  An arrow's ends by address: the branch, jump or call at `from`, where
    //  it goes, and whether the current flags take it.
    struct ArrowEnds
    {
        Word  from    = 0;
        Word  goesTo  = 0;
        bool  isTaken = true;
    };

    //  The columns, the glyph margin and the text view's background.
    static void  Configure (DxuiListView & list);

    //  The heat, address, bytes and label columns, as the options choose.
    static void  ShowColumns (DxuiListView & list, const DisassemblyOptions & options);

    //  A source line's row: its number when withNumber is set, then its text
    //  in the syntax colors. While the label column is visible, the line's label
    //  is in it and the rest starts in the instruction column, aligned with
    //  the disassembly's, and the number is in the first visible column before
    //  the label; otherwise the whole line starts in the first visible column
    //  after the glyph margin.
    //  With an operand cell, the row is also its instruction's: while the
    //  label column is visible, the code stops at the comment, the operand
    //  and result are in their column, and the comment follows them. With
    //  the label column hidden, or a dragged instruction column too narrow
    //  for the code, the operand and result follow the line.
    static void  FillSourceRow (std::vector<DxuiListView::Cell> & cells, const DxuiListView & list, int sourceLine,
                                const std::wstring & text, bool withNumber, const DebuggerTextColors::Set & colors, uint32_t numberArgb,
                                const DxuiListView::Cell * operand = nullptr);

    //  The bytes a source row's instructions occupy: from the first
    //  instruction under it to the end of the last one from the same line,
    //  so the row's heat is theirs. False for an instruction's row and for a
    //  source row with no instruction under it.
    static bool  TryGetSourceRowSpan (const std::vector<DisassemblyOptions::Row> & rows, size_t row,
                                      const std::vector<DebuggerViewSnapshot::CodeLine> & lines, Word & first, size_t & count);

    //  An instruction's row as the options choose, a data line's in italics.
    //  Returns the annotation placed in the operand cell.
    static std::string  FillCodeRow (std::vector<DxuiListView::Cell> & cells, const DebuggerViewSnapshot::CodeLine & line,
                                     const DisassemblyOptions & options, const DebuggerTextColors::Set & colors);

    //  Why a data line's bytes are data, and how to disassemble them as code
    //  again, for the tip over its instruction.
    static std::wstring  GetDataTip (DataOrigin origin);

    //  Whether a row is drawn dimmed: its kind matches the options' Dim and
    //  the view has source rows. The PC's row is never dimmed.
    static bool  IsDimmed (const DisassemblyOptions & options, bool isSourceShown, bool isSourceRow, bool isPcRow);

    //  A row's text, in every color it has, mixed three quarters of the way
    //  toward the background; text with no color of its own starts from
    //  foregroundArgb.
    static void  DimRow (std::vector<DxuiListView::Cell> & cells, uint32_t foregroundArgb, uint32_t backgroundArgb);

    //  The annotation, then the result in its own color two spaces after it.
    static DxuiListView::Cell  GetOperandAndResultCell (const std::string & annotation, const std::string & effect, uint32_t resultArgb);

    //  Visual Studio's yellow, which reads on either background.
    static uint32_t  GetPcMarkerArgb (uint32_t background);

    //  The ends of the arrow of the line marked as the PC's; false when that
    //  line has no target.
    static bool  TryGetPcArrowEnds (const std::vector<DebuggerViewSnapshot::CodeLine> & lines, ArrowEnds & ends);

    //  The arrow between two addresses over a list laid out and painted with
    //  these rows. An end not among the lines built for the view lies beyond
    //  the edge its address is past, and the line enters from that edge.
    //  False when no part of the arrow crosses the rows in view, or the list
    //  has no room for it.
    static bool  TryGetBranchArrow (const DxuiListView & list, const std::vector<DebuggerViewSnapshot::CodeLine> & lines,
                                    const std::vector<DisassemblyOptions::Row> & rows, const ArrowEnds & ends, BranchArrow::Input & input);

    //  The arrow of the branch a step took to the PC, which ends at the PC's
    //  row, and the PC's arrow, which starts there: each is drawn to the row a
    //  little to the side of its other end, and the PC's is moved a lane to the left when both run the
    //  same way from the row, so neither is drawn over the other.
    static void  SeparateArrows (BranchArrow::Input & stepped, BranchArrow::Input & pc);

    //  The arrow in takenArgb, or in notTakenArgb for a branch that the
    //  current flags do not take.
    //  Dotted for the branch that led to the PC: round dots as wide as the
    //  line, centered two widths apart, so a dot's width lies between each
    //  two. Solid for the PC's own. The head is solid either way.
    static void  PaintBranchArrow (IDxuiPainter & painter, const BranchArrow::Input & input, bool isTaken, uint32_t takenArgb, uint32_t notTakenArgb,
                                   bool isDotted = false);

private:
    //  The syntax runs of a source line that fall in [start, end), moved to
    //  begin at `offset` in a cell's text.
    static void  AddSourceRuns (DxuiListView::Cell & cell, const std::vector<SourceSyntax::Run> & runs, int start, int end, int offset,
                                const DebuggerTextColors::Set & colors);

    //  A cell's text and colors added at the end of another cell's, two
    //  spaces after any text there.
    static void  AppendCell (DxuiListView::Cell & cell, const DxuiListView::Cell & added);

    //  Where the first comment run at or after `from` starts, or the line's end.
    static size_t  GetCommentStart (const std::vector<SourceSyntax::Run> & runs, size_t from, size_t end);


    static void  PaintDots (IDxuiPainter & painter, const BranchArrow::Segment & segment, float width, uint32_t argb, float & along);

    //  The row of an address not among the lines: -1 for one before them,
    //  the row count for one after; false for one inside their span that
    //  isn't the start of any line.
    static bool  GetOutsideRow (const DxuiListView & list, const std::vector<DebuggerViewSnapshot::CodeLine> & lines, Word address, int & row);
};
