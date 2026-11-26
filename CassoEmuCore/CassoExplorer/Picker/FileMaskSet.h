#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileMaskSet
//
//  A file dialog filter's masks, such as "*.po;*.2mg", matched against file
//  names as the Windows dialogs match them: without regard to case, `*` for
//  any run of characters and `?` for one, and "*.*" for every name, with or
//  without an extension. A pattern with no masks matches every name.
//
////////////////////////////////////////////////////////////////////////////////

class FileMaskSet
{
public:
    FileMaskSet () = default;
    explicit FileMaskSet (std::wstring_view pattern);

    bool  Matches (std::wstring_view name) const;

    const std::vector<std::wstring> &  GetMasks () const { return m_masks; }

    //  The extension of the first mask of the form "*.ext", without the dot,
    //  or empty: what a save form adds to a name typed without one.
    std::wstring  GetFirstExtension () const;

    //  The plain "*.ext" extensions, without the dots.
    std::vector<std::wstring>  GetPlainExtensions () const;

    //  `name` for the file type `chosen`: a type with one extension replaces
    //  a known extension or adds its own, and a type with several removes a
    //  known extension. An extension is known when one of `all` has it.
    static std::wstring  ApplyToName (std::wstring_view name, const FileMaskSet & chosen, const std::vector<FileMaskSet> & all);

    static bool  MatchesMask (std::wstring_view mask, std::wstring_view name);

private:
    std::vector<std::wstring>  m_masks;
    bool                       m_matchesAll = true;
};
