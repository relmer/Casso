#pragma once

#include "Ui/Debugger/DebuggerActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolActions
//
//  The actions on symbols and the debug file: loading, adding, enabling,
//  clearing, saving and removing symbols, marking ranges as data, and
//  unloading the debug file.
//
////////////////////////////////////////////////////////////////////////////////

class SymbolActions
{
public:
    //  The code pane's menu: a range marked as data of a directive's kind
    //  (DB, DW, DA, ASC or DF) or as code again, a symbol, a routine never
    //  stepped into, a bookmark added or removed, the stopwatch between two
    //  addresses, and a routine run as JSR runs it.
    static DebuggerAction  GetMarkData       (const std::string & directive, Word first, Word last, CommandMode mode, const std::string & name = {});
    static DebuggerAction  GetClearData      (Word first, Word last, CommandMode mode);
    static DebuggerAction  GetAddSymbol      (const std::string & name, Word address, CommandMode mode);

    //  The Symbols, Profiler and step filter views: a table on or off,
    //  cleared, saved, or one of its symbols removed, where the table is its
    //  name as SYM writes it after SYM (USER, MAIN); PROFILE with its words;
    //  and a step filter removed by its name or range.
    static DebuggerAction  GetEnableSymbols  (const std::string & table, bool enable, CommandMode mode);
    static DebuggerAction  GetClearSymbols   (const std::string & table, CommandMode mode);
    static DebuggerAction  GetSaveSymbols    (const std::string & table, const std::string & path, CommandMode mode);
    static DebuggerAction  GetRemoveSymbol   (const std::string & table, const std::string & name, CommandMode mode);

    //  SRC CLEAR: the debug file and the symbols it loaded, unloaded.
    static DebuggerAction  GetUnloadDebugFile  (CommandMode mode);
    static DebuggerAction  GetLoadSymbols      (const std::string & path, CommandMode mode);
};
