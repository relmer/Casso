#pragma once

#include "Debugger/IDebugCommandHandler.h"

class ApplesoftProgram;
class DebugSession;
class WatchTable;





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers
//
//  The three numbered address lists: watches (W, WA, WC, WD, WE, WL,
//  WSAVE), zero-page pointers (ZP, ZPA, ZP0-ZP7, P0-P4, ZPC, ZPD, ZPE, ZPL,
//  ZPSAVE) and bookmarks (BM, BMA, BMC, BML, BMG, BMSAVE).
//
//  A watch's value is the word at its address, or an Applesoft variable's;
//  a zero-page pointer's is the address its two bytes form. BMG lists code
//  at the bookmark, since batch mode has no cursor to move.
//
////////////////////////////////////////////////////////////////////////////////

class WatchHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    // The script that recreates a list, as the *SAVE commands write it.
    static std::string    MakeScript (DebugSession & session, WatchListKind kind);
    //  A BASIC watch's value is read only with isReadingBasic, since a real
    //  is written by running the ROM's FOUT.
    static WatchListData  MakeList   (DebugSession & session, WatchListKind kind, bool isReadingBasic = true);

    //  The BASIC variable a watch's text refers to, following FR-241: BASIC and
    //  the name always; otherwise not when a symbol has the name, and a name
    //  with $, % or subscripts, or a plain name the stopped program has.
    static bool  TryGetBasicName (DebugSession & session, const std::string & text, std::string & name);

    //  The value as PRINT writes it, a string in quotes; none unless the
    //  machine is in a running Applesoft program.
    static std::optional<std::string>  ReadBasicValue (DebugSession & session, const std::string & name);

private:
    static constexpr int  kListLines = 20;

    static void  Add    (DebugSession & session, const DebugCommand & command, WatchListKind kind, Reply & reply);
    static void  Clear  (DebugSession & session, const DebugCommand & command, WatchListKind kind, Reply & reply);
    static void  Enable (DebugSession & session, const DebugCommand & command, WatchListKind kind, bool enabled, Reply & reply);
    static void  GoTo   (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Save   (DebugSession & session, const DebugCommand & command, WatchListKind kind, Reply & reply);

    static WatchTable          & GetTable       (DebugSession & session, WatchListKind kind);
    static const char          * GetNoun        (WatchListKind kind);
    static const char          * GetPlural      (WatchListKind kind);
    static bool                  TryGetSlot     (const std::string & sourceName, int & slot);
    static bool                  TryGetFreeSlot (const WatchTable & table, int & slot);
    static std::optional<Word>   PeekWord       (DebugSession & session, Word address, WatchListKind kind);
    static void                  SetNoSuch      (Reply & reply, WatchListKind kind, uint32_t id);
    static bool                  IsProgramRunning (DebugSession & session, const ApplesoftProgram & program);
};
