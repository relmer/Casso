#include "Pch.h"

#include "FilePath.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FilePath::Parse
//
//  Splits on '/'. Empty components are dropped rather than preserved, so a
//  trailing separator or a doubled one is tolerated instead of producing a
//  nameless step that no volume could match.
//
////////////////////////////////////////////////////////////////////////////////

FilePath FilePath::Parse (const std::string & text)
{
    FilePath     path;
    size_t       start = 0;
    size_t       i     = 0;
    std::string  part;



    path.m_isRooted = !text.empty() && text[0] == '/';

    for (i = 0; i <= text.size(); i++)
    {
        bool  atEnd = i == text.size();

        if (!atEnd && text[i] != '/')
        {
            continue;
        }

        part = text.substr (start, i - start);

        if (!part.empty())
        {
            path.m_components.push_back (part);
        }

        start = i + 1;
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilePath::GetLeaf
//
//  An empty path has no leaf; returning a reference to an empty string keeps
//  callers from having to guard every message that quotes a name.
//
////////////////////////////////////////////////////////////////////////////////

const std::string & FilePath::GetLeaf() const
{
    if (m_components.empty())
    {
        return m_empty;
    }

    return m_components.back();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilePath::ToString
//
////////////////////////////////////////////////////////////////////////////////

std::string FilePath::ToString() const
{
    std::string  text;
    size_t       i    = 0;



    if (m_isRooted)
    {
        text = "/";
    }

    for (i = 0; i < m_components.size(); i++)
    {
        if (i > 0)
        {
            text += "/";
        }

        text += m_components[i];
    }

    return text;
}
