#pragma once

#include "Debugger/IDebugCommandHandler.h"

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
//  A watch shows the word at its address; a zero-page pointer shows the
//  address the pointer holds. BMG lists code at the bookmark, since batch
//  mode has no cursor to move.
//
////////////////////////////////////////////////////////////////////////////////

class WatchHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    // The script that recreates a list, as the *SAVE commands write it.
    static std::string    MakeScript (DebugSession & session, WatchListKind kind);
    static WatchListData  MakeList   (DebugSession & session, WatchListKind kind);

private:
    static constexpr int  kListLines = 20;

    static void  Add    (DebugSession & session, const DebugCommand & command, WatchListKind kind, Reply & reply);
    static void  Clear  (DebugSession & session, const DebugCommand & command, WatchListKind kind, Reply & reply);
    static void  Enable (DebugSession & session, const DebugCommand & command, WatchListKind kind, bool enabled, Reply & reply);
    static void  GoTo   (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Save   (DebugSession & session, const DebugCommand & command, WatchListKind kind, Reply & reply);

    static WatchTable  & GetTable   (DebugSession & session, WatchListKind kind);
    static const char  * GetNoun    (WatchListKind kind);
    static const char  * GetPlural  (WatchListKind kind);
    static bool          TryGetSlot (const std::string & sourceName, int & slot);
    static Word          PeekWord   (DebugSession & session, Word address);
    static void          SetNoSuch  (Reply & reply, WatchListKind kind, int id);
};
