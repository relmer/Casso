#pragma once

#include "Ui/Debugger/Panes/ICodeViews.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CodeEditCommands
//
//  The disassembly views' menu entries that change a line: data marking
//  (FR-182), a symbol (FR-183), a routine never stepped into (FR-184), a
//  bookmark (FR-189), the stopwatch and a call (FR-197), and the edits of
//  the code itself (FR-185, FR-186), with their own undo and redo.
//
////////////////////////////////////////////////////////////////////////////////

class CodeEditCommands
{
public:
    CodeEditCommands (IDebuggerPaneHost & host, const ICodeViews & views);

    CodeEditCommands (const CodeEditCommands &)             = delete;
    CodeEditCommands & operator= (const CodeEditCommands &) = delete;

    //  The first address of the first selected line through the last byte
    //  of the last, or the clicked line's bytes when it is not among the
    //  selected; none when no row is a line.
    std::optional<std::pair<Word, Word>>  GetSelection (int view, int row) const;

    //  A line's entries, for the row of the line clicked.
    void                 AddItems     (int view, const DebuggerViewSnapshot::CodeLine & line, int row, PaneMenuItems & items);

    //  The typed instruction assembled over the line's; every byte of a
    //  range made a NOP; and the last edit undone or redone.
    void                 EditInstruction (int view, const DebuggerViewSnapshot::CodeLine & line);
    void                 ReplaceWithNops (int view, std::pair<Word, Word> range);
    void                 Undo         (bool redo);

    //  The bytes from first through last, as the view's lines have them;
    //  false where the lines do not cover the range without a gap.
    bool                 TryGetBytes  (int view, Word first, Word last, std::vector<Byte> & bytes) const;

private:
    struct Edit
    {
        Word               address = 0;
        std::vector<Byte>  before;
        std::vector<Byte>  after;
        std::wstring       text;
    };

    IDebuggerPaneHost    & m_host;
    const ICodeViews     & m_views;
    std::vector<Edit>      m_undo;
    std::vector<Edit>      m_redo;

    //  Where Start stopwatch here was chosen, until Stop stopwatch here.
    std::optional<Word>    m_stopwatchStart;
};
