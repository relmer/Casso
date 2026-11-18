#pragma once

#include "Debugger/IDebugCommandHandler.h"

class DebugSession;
struct DebugFile;





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers
//
//  SYM and the SYM<table> commands: a lookup by name or address, CLEAR,
//  LOAD "file"[,offset], SAVE "file", ON, OFF, name = addr, ! name and
//  ~ name; SYMINFO and SYMLIST.
//
//  SYM alone reports each table's count. A lookup through SYM searches the
//  enabled tables; through SYM<table> it searches that table only. A name
//  with `*` or `?` in it lists every symbol it matches. SYM adds
//  and removes in the User table; SYM<table> in its own.
//
////////////////////////////////////////////////////////////////////////////////

class SymbolHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    //  An Applesoft listing rather than a symbol file: a .bas file, or three
    //  lines or more, each a rising line number and a statement starting
    //  with a keyword. Read as symbols, `10 HOME` was HOME at $0010.
    static bool  IsBasicListing (const std::string & path, const std::string & content);

private:
    static void  Lookup         (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  LookupPattern  (DebugSession & session, const DebugCommand & command, const std::string & pattern, Reply & reply);
    static bool  IsPatternMatch (const std::string & pattern, const std::string & name);
    static void  Info           (DebugSession & session, Reply & reply);
    static void  List           (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Load           (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  LoadDebugFile (DebugSession & session, SymbolTableId table, const std::string & name,
                                const std::string & content, int offset, bool isListing, Reply & reply);
    static bool                                     IsListingWithLines  (const std::string & content);
    static std::map<int, std::vector<std::string>>  ReadSources         (DebugSession & session, const DebugFile & file, const std::wstring & debugFilePath);
    static void                                     AddRegisterNameNote (DebugSession & session, SymbolTableId table, std::vector<std::string> & lines);
    static void                                     Save                (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                                     Clear               (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                                     Enable              (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                                     Add                 (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                                     Remove              (DebugSession & session, const DebugCommand & command, Reply & reply);

    static bool         TryGetTable    (const DebugCommand & command, SymbolTableId & table);
    static bool         TryParseHex    (const std::string & text, Word & value);
    static bool         TryGetFileName (const std::string & text, std::string & name, int & offset, std::string & error);
    static bool         HasOffset      (const std::string & text);
    static std::string  Trim           (const std::string & text);
};
