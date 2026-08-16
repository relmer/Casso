#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FilePath
//
//  A file's location on a volume, as one or more name components.
//
//  Paths rather than bare names from the outset, even though DOS 3.3 has no
//  subdirectories and the ProDOS reader currently only walks the volume
//  directory. Adding traversal later then becomes filling in a capability
//  instead of changing every signature that names a file -- and a caller who
//  hands over a multi-component path today gets a clear refusal rather than a
//  silently truncated name that opens the wrong file.
//
//  A volume with no subdirectories simply has paths one component long.
//
////////////////////////////////////////////////////////////////////////////////

class FilePath
{
public:
    //  Splits on '/' -- ProDOS's separator, and unambiguous on a platform whose
    //  own separator is '\\'. Empty components are dropped, so a trailing or
    //  doubled separator is tolerated rather than producing a nameless step.
    static FilePath  Parse (const std::string & text);

    const vector<std::string> &  GetComponents () const { return m_components; }

    bool  IsRooted () const { return m_isRooted; }
    bool  IsEmpty  () const { return m_components.empty(); }

    size_t  GetDepth () const { return m_components.size(); }

    //  True when the path names a file directly in the volume directory, which
    //  is all that DOS 3.3 can express and all the ProDOS reader walks today.
    bool  IsSingleComponent () const { return m_components.size() == 1; }

    //  The last component -- the file's own name.
    const std::string &  GetLeaf () const;

    //  Rejoined with '/', for messages that quote what the caller asked for.
    std::string  ToString () const;

private:
    vector<std::string>  m_components;
    bool                 m_isRooted = false;
    std::string          m_empty;
};
