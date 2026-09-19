#pragma once

#include "Debugger/IDebugCommandHandler.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers
//
//  SYM and the SYM<table> commands: a lookup by name or address, CLEAR,
//  LOAD "file"[,offset], SAVE "file", ON, OFF, name = addr, ! name and
//  ~ name; SYMINFO and SYMLIST.
//
//  SYM alone reports each table's count. A lookup through SYM searches the
//  enabled tables; through SYM<table> it searches that table only. SYM adds
//  and removes in the User table; SYM<table> in its own.
//
////////////////////////////////////////////////////////////////////////////////

class SymbolHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

private:
    static void  Lookup   (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Info     (DebugSession & session, Reply & reply);
    static void  List     (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Load     (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  LoadDebugFile (DebugSession & session, SymbolTableId table, const std::string & name,
                                const std::string & content, int offset, bool isListing, Reply & reply);
    static bool  IsListingWithLines (const std::string & content);
    static void  Save     (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Clear    (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Enable   (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Add      (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Remove   (DebugSession & session, const DebugCommand & command, Reply & reply);

    static bool  TryGetTable    (const DebugCommand & command, SymbolTableId & table);
    static bool  TryParseHex    (const std::string & text, Word & value);
    static bool  TryGetFileName (const std::string & text, std::string & name, int & offset, std::string & error);
    static std::string  Trim    (const std::string & text);
};
