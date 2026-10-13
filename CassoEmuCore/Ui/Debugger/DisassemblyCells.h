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
    static constexpr int     kGutterColumnDip   = 17;
    static constexpr size_t  kGutterColumn      = 0;
    static constexpr size_t  kAddressColumn     = 1;
    static constexpr size_t  kBytesColumn       = 2;
    static constexpr size_t  kLabelColumn       = 3;
    static constexpr size_t  kInstructionColumn = 4;
    static constexpr size_t  kOperandColumn     = 5;
    static constexpr size_t  kColumnCount       = 6;
    static constexpr float   kDimMix            = 0.5f;

    //  The columns, the glyph margin and the text view's background.
    static void  Configure (DxuiListView & list);

    //  The address, bytes and label columns, as the options choose.
    static void  ShowColumns (DxuiListView & list, const DisassemblyOptions & options);

    //  A source line's row: its number when withNumber is set, then its text
    //  in the syntax colors, starting in the first visible column after the
    //  glyph margin.
    static void  FillSourceRow (std::vector<DxuiListView::Cell> & cells, const DxuiListView & list, int sourceLine,
                                const std::wstring & text, bool withNumber, const DebuggerTextColors::Set & colors, uint32_t numberArgb);

    //  An instruction's row as the options choose. Returns the annotation
    //  placed in the operand cell.
    static std::string  FillCodeRow (std::vector<DxuiListView::Cell> & cells, const DebuggerViewSnapshot::CodeLine & line,
                                     const DisassemblyOptions & options, const DebuggerTextColors::Set & colors);

    //  Whether a row is drawn dimmed: its kind matches the options' Dim and
    //  the view has source rows. The PC's row is never dimmed.
    static bool  IsDimmed (const DisassemblyOptions & options, bool isSourceShown, bool isSourceRow, bool isPcRow);

    //  A row's text, in every color it has, mixed halfway toward the
    //  background; text with no color of its own starts from foregroundArgb.
    static void  DimRow (std::vector<DxuiListView::Cell> & cells, uint32_t foregroundArgb, uint32_t backgroundArgb);

    //  The annotation, then the result in its own color two spaces after it.
    static DxuiListView::Cell  GetOperandAndResultCell (const std::string & annotation, const std::string & effect, uint32_t resultArgb);

    //  Visual Studio's yellow, which reads on either background.
    static uint32_t  GetPcMarkerArgb (uint32_t background);

    //  The PC's branch arrow over a list laid out and painted with these
    //  rows; false when the PC's line has no target or the list has no room.
    static bool  TryGetBranchArrow (const DxuiListView & list, const std::vector<DebuggerViewSnapshot::CodeLine> & lines,
                                    const std::vector<DisassemblyOptions::Row> & rows, BranchArrow::Input & input, Word & goesTo, bool & isTaken);

    //  The arrow in takenArgb, or in notTakenArgb for a branch that the
    //  current flags do not take.
    static void  PaintBranchArrow (IDxuiPainter & painter, const BranchArrow::Input & input, bool isTaken, uint32_t takenArgb, uint32_t notTakenArgb);
};
