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

    //  Explorer's own list, newest first, with this list's entries it lacks --
    //  paths into disk images, which Explorer cannot open -- placed just after
    //  the nearest newer entry the two share, or first when none is newer. At
    //  most kMaxEntries, matched without regard to case.
    static std::vector<std::wstring>  Merge (const std::vector<std::wstring> & explorer, const std::vector<std::wstring> & ours);

    //  Explorer's own TypedPaths list keeps as many.
    static constexpr size_t  kMaxEntries = 25;

private:

    static bool  IsBlank (const std::wstring & text);

    std::vector<std::wstring>  m_entries;
};
