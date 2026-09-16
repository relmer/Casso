#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchItem
//
////////////////////////////////////////////////////////////////////////////////

struct WatchItem
{
    int   id      = 0;
    Word  address = 0;
    bool  enabled = true;
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
    int   AddAt         (int id, Word address);
    bool  TryClear      (int id);
    void  ClearAll      ();
    bool  TrySetEnabled (int id, bool enabled);
    bool  TryFind       (int id, WatchItem & item) const;

    const std::vector<WatchItem> &  GetAll () const { return m_items; }

private:
    std::vector<WatchItem>  m_items;
    int                     m_nextId = 0;
};
