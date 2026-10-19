#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchItem
//
////////////////////////////////////////////////////////////////////////////////

//  basicName is a BASIC watch's Applesoft variable, as Applesoft writes it;
//  empty for an address. A watch typed in the watch pane is resolved once
//  to its kind, with the text typed as its expression: a register's
//  name, a symbol's, whose address is looked up again at each read, or an
//  expression evaluated at each read.
struct WatchItem
{
    enum class Kind
    {
        Address,
        Basic,
        Register,
        Symbol,
        Expression,
    };

    int          id      = 0;
    Word         address = 0;
    bool         enabled = true;
    std::string  basicName;
    Kind         kind    = Kind::Address;
    std::string  expression;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WatchTable
//
//  A numbered address list: AppleWin's display watches, zero-page pointers
//  and bookmarks each use one. Ids count up from 0 within the list and are
//  not reused after a clear, except that clearing the whole list starts
//  numbering over. AddAt fills a given slot, as ZP0-ZP7 do, replacing what
//  was there.
//
////////////////////////////////////////////////////////////////////////////////

class WatchTable
{
public:
    int   Add           (Word address);
    int   AddBasic      (const std::string & name);
    int   AddTyped      (WatchItem item);
    int   AddAt         (int id, Word address);
    bool  TryClear      (int id);
    void  ClearAll      ();
    bool  TrySetEnabled (int id, bool enabled);
    bool  TryFind       (int id, WatchItem & item) const;

    //  An entry's id changed, and the id the next Add takes, as a debug
    //  project restores them. TryChangeId fails if the id is taken.
    bool  TryChangeId   (int from, int to);
    int   GetNextId     () const     { return m_nextId; }
    void  SetNextId     (int nextId) { m_nextId = nextId; }

    const std::vector<WatchItem> &  GetAll () const { return m_items; }

private:
    std::vector<WatchItem>  m_items;
    int                     m_nextId = 0;
};
