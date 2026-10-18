#pragma once

#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions
//
//  What a disassembly view shows, as Visual Studio's viewing options have it:
//  addresses, code bytes, the source lines the code came from, symbol names
//  and the source lines' numbers; and, from AppleWin's DISASM settings, the
//  operand values and pointers in the annotation, the PC's branch arrow and a
//  colon after each address; and which kind of row is dimmed while the view
//  has source rows. One set applies to every view and is saved in a setting
//  as text.
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
        OperandValues,
        Pointers,
        BranchArrow,
        AddressColon,
        Heat,
    };

    static constexpr int  kOptionCount = 10;

    //  The rows drawn dimmed while the view has source rows, to set the other
    //  kind apart. A view with no source rows dims nothing.
    enum class Dim
    {
        Source,
        Code,
        None,
    };

    static constexpr int  kDimCount = 3;

    //  The options a text saved before the last four existed can list.
    static constexpr int  kFirstTextOptionCount = 5;

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
    Dim   GetDim () const { return m_dim; }
    void  SetDim (Dim dim) { m_dim = dim; }

    bool  operator== (const DisassemblyOptions & other) const = default;

    //  The setting's text: each option's key, with a leading - when it is
    //  off, then dim= and the Dim key, separated by spaces. FromText returns
    //  the defaults for empty text and skips an unknown key. A text with no -
    //  is the earlier form, which listed the options turned on: an option it
    //  can list and does not is off, and the later options take their
    //  defaults.
    std::string                 ToText   () const;
    static DisassemblyOptions   FromText (const std::string & text);

    static const wchar_t      * GetLabel (Option option);
    static const char         * GetKey   (Option option);
    static const wchar_t      * GetDimLabel (Dim dim);
    static const char         * GetDimKey   (Dim dim);

    //  The tips of the source, line number and symbol names check boxes: how
    //  to get source or line numbers while there are none, and each source of
    //  the symbols, one to a line.
    static std::wstring  GetSourceTip      (bool hasDebugFile);
    static std::wstring  GetLineNumbersTip (bool hasDebugFile, bool sourceShown);
    static std::wstring  GetSymbolsTip     (const std::vector<std::string> & sources);

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
    static constexpr const char *   kDimPrefix = "dim=";

    std::array<bool, kOptionCount>  m_on  = { true, true, true, true, true, true, true, true, false, false };
    Dim                             m_dim = Dim::Code;
};
