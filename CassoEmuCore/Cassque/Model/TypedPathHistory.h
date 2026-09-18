#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TypedPathHistory
//
//  The paths the user has typed into the address bar, newest first.
//
//  ONLY WHAT WAS TYPED. Clicking a folder, a tree node or an address segment
//  moves the browser without going through here, so the list stays what the
//  user wrote rather than a second Back stack. A path is added after the
//  navigation it asked for succeeded, so a path that goes nowhere is never
//  offered back.
//
////////////////////////////////////////////////////////////////////////////////

class TypedPathHistory
{
public:

    //  Puts `text` at the top, dropping the oldest entry past the limit. A
    //  path already held moves to the top instead of appearing twice, matched
    //  without regard to case because neither Windows nor ProDOS distinguishes
    //  it. Blank text is ignored.
    void  Add (const std::wstring & text);

    void  Clear ();

    //  Replaces the whole list, for the stored copy read at startup. Takes the
    //  same limit and the same duplicate rule as Add, oldest first.
    void  Reset (const std::vector<std::wstring> & entries);

    const std::vector<std::wstring> &  GetEntries () const { return m_entries; }

    static constexpr size_t  kMaxEntries = 10;

private:

    static bool  IsBlank (const std::wstring & text);

    std::vector<std::wstring>  m_entries;
};
