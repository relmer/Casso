#pragma once

#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/SourceSyntax.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramPane
//
//  The Applesoft program as LIST prints it (FR-173), read from memory, with
//  the line being run on the PC's row color and its statement in full color
//  and the rest of the line muted, the PC's arrow on the row the statement
//  starts on, and a breakpoint's dot on each line with a BPBASIC
//  breakpoint. Activating a row, by a double-click or Enter, sets a line
//  breakpoint there or clears the one there.
//
//  The window owns the list; the pane has a pointer to it.
//
////////////////////////////////////////////////////////////////////////////////

class BasicProgramPane
{
public:
    using ToggleFn = std::function<void (Word line)>;

    //  The icons and colors the disassembly marks a line with, so the two
    //  panes mark one alike.
    struct Style
    {
        std::shared_ptr<const DxuiIconImage>  pcIcon;
        std::shared_ptr<const DxuiIconImage>  enabledIcon;
        std::shared_ptr<const DxuiIconImage>  disabledIcon;
        std::shared_ptr<const DxuiIconImage>  pcOnEnabledIcon;
        std::shared_ptr<const DxuiIconImage>  pcOnDisabledIcon;
        uint32_t                              pcRowArgb = 0;
        SourceSyntax::Colors                  syntax;

        bool operator== (const Style & other) const = default;
    };

    //  One row as LIST prints it: its line, whether it is the line's first,
    //  and where each of the line's pieces on it begins, by piece.
    struct Row
    {
        std::wstring                      text;
        size_t                            lineIndex = 0;
        bool                              isFirst   = true;
        std::vector<std::pair<int, int>>  pieces;   // piece index, offset in text
    };

    BasicProgramPane (DxuiListView * list, ToggleFn toggle);

    DxuiListView *  GetList () const { return m_list; }

    void  Configure (int gutterDip);
    void  SetStyle  (const Style & style) { m_style = style; }
    void  Apply     (const std::optional<DebuggerViewSnapshot::BasicState> & basic, const std::vector<DebuggerViewSnapshot::BreakpointLine> & breakpoints);

    //  Scrolls to a line's first row and selects it.
    void  ShowLine  (Word line);

    //  The rows LIST prints, with each piece's place.
    static std::vector<Row>  GetRows (const std::vector<BasicLine> & lines);

    //  The pieces of the statement that the piece at `piece` is in, first
    //  and one past the last: a statement ends at a colon outside quotes,
    //  and a REM runs to the end of the line.
    static std::pair<int, int>  GetStatement (const BasicLine & line, int piece);

    //  Each row's cells: the gutter's icon and the text, colored.
    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const DebuggerViewSnapshot::BasicState                & basic,
                                                                    const std::vector<Row>                                 & rows,
                                                                    const std::vector<DebuggerViewSnapshot::BreakpointLine> & breakpoints,
                                                                    const Style                                            & style);

    //  The note in place of the listing, empty when there is a listing.
    static std::wstring  GetNote (const std::optional<DebuggerViewSnapshot::BasicState> & basic);

private:
    static void  AddSyntaxColors (const BasicLine & line, const Row & row, const SourceSyntax::Colors & syntax, DxuiListView::Cell & cell);

    DxuiListView      * m_list = nullptr;
    ToggleFn            m_toggle;
    Style               m_style;
    std::vector<Row>    m_rows;
    std::vector<Word>   m_rowLines;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BasicVariablesPane
//
//  The program's simple variables and every array element, each value as
//  PRINT writes it and a string in quotes (FR-176). The values are read only
//  while the machine is paused; while it runs, the pane has a note to
//  pause it.
//
////////////////////////////////////////////////////////////////////////////////

class BasicVariablesPane
{
public:
    explicit BasicVariablesPane (DxuiListView * list) : m_list (list) {}

    DxuiListView *  GetList () const { return m_list; }

    void  Configure ();
    void  Apply     (const std::optional<DebuggerViewSnapshot::BasicState> & basic);

    //  Name and value, or one note across the row.
    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const std::optional<DebuggerViewSnapshot::BasicState> & basic);

    //  An element's name: the array's, then its subscripts, the first varying
    //  fastest.
    static std::wstring  GetElementName (const BasicArray & array, size_t index);

private:
    DxuiListView  * m_list = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStackPane
//
//  The active GOSUB returns and FOR loops, newest first, each with the line
//  it was made on (FR-177). Activating a row brings that line into view in
//  the BASIC program pane.
//
////////////////////////////////////////////////////////////////////////////////

class BasicStackPane
{
public:
    using ShowLineFn = std::function<void (Word line)>;

    BasicStackPane (DxuiListView * list, ShowLineFn showLine) : m_list (list), m_showLine (std::move (showLine)) {}

    DxuiListView *  GetList () const { return m_list; }

    void  Configure ();
    void  Apply     (const std::optional<DebuggerViewSnapshot::BasicState> & basic);

    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const std::optional<DebuggerViewSnapshot::BasicState> & basic);

private:
    DxuiListView          * m_list = nullptr;
    ShowLineFn              m_showLine;
    std::vector<Word>       m_lines;
};
