#pragma once

#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions
//
//  What a disassembly view shows, as Visual Studio's viewing options have it:
//  addresses, code bytes, the source lines the code came from, symbol names
//  and the source lines' numbers. One set holds for every view and is kept in
//  a setting as text.
//
//  SOURCE ROWS SIT ABOVE THEIR CODE. A view's rows are its instructions with,
//  when source is shown, each source line the debug file maps to an
//  instruction placed on a row of its own above the first instruction it
//  produced. A row is therefore not an instruction index; GetLineOfRow and
//  GetRowOfLine convert between the two.
//
////////////////////////////////////////////////////////////////////////////////

class DisassemblyOptions
{
public:
    enum class Option
    {
        Addresses,
        CodeBytes,
        Source,
        Symbols,
        LineNumbers,
    };

    static constexpr int  kOptionCount = 5;

    //  One row of a view: an instruction, by its index in the view's lines,
    //  or a source line, with -1 for the instruction.
    struct Row
    {
        int  codeLine   = -1;
        int  fileId     = -1;
        int  sourceLine = 0;
    };

    using HasLineFn = std::function<bool (int fileId, int line)>;

    bool  IsOn   (Option option) const { return m_on[(size_t) option]; }
    void  Set    (Option option, bool on) { m_on[(size_t) option] = on; }
    void  Toggle (Option option) { Set (option, !IsOn (option)); }

    bool  operator== (const DisassemblyOptions & other) const = default;

    //  The setting's text: the keys of the options turned on, separated by
    //  spaces. Empty text gives the defaults; an unknown key is skipped.
    std::string                 ToText   () const;
    static DisassemblyOptions   FromText (const std::string & text);

    static const wchar_t      * GetLabel (Option option);
    static const char         * GetKey   (Option option);

    //  The tips of the source and symbol names check boxes: how to get source
    //  while there is none, and each source of the symbols, one to a line.
    static std::wstring  GetSourceTip  (bool hasDebugFile);
    static std::wstring  GetSymbolsTip (const std::vector<std::string> & sources);

    //  Source and line numbers need a debug file that maps the code.
    static bool  NeedsDebugFile (Option option) { return option == Option::Source || option == Option::LineNumbers; }

    //  A view's rows. A source row goes above an instruction whose source line
    //  differs from the instruction before it, when the line's text is known.
    static std::vector<Row>  BuildRows (const std::vector<DebuggerViewSnapshot::CodeLine> & lines, bool showSource,
                                        const HasLineFn & hasLine);

    static int   GetLineOfRow (const std::vector<Row> & rows, int row);
    static int   GetRowOfLine (const std::vector<Row> & rows, int codeLine);

    //  The instruction as the view shows it: with symbol names off, the
    //  operand's address in place of the symbol shown for it.
    static std::string  GetInstructionText (const DebuggerViewSnapshot::CodeLine & line, bool showSymbols);

    //  Whether a line can take a breakpoint: an instruction, not data.
    static bool  CanTakeBreakpoint (const DebuggerViewSnapshot::CodeLine & line);

private:
    std::array<bool, kOptionCount>  m_on = { true, true, true, true, true };
};
