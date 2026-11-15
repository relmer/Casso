#include "Pch.h"

#include "CassoExplorer/Picker/FileMaskSet.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileMaskSet::FileMaskSet
//
//  Masks are separated by semicolons; the space around one and an empty one
//  are dropped.
//
////////////////////////////////////////////////////////////////////////////////

FileMaskSet::FileMaskSet (std::wstring_view pattern)
{
    size_t  start = 0;



    while (start <= pattern.size())
    {
        size_t             end   = pattern.find (L';', start);
        std::wstring_view  mask  = pattern.substr (start, (end == std::wstring_view::npos) ? std::wstring_view::npos : end - start);
        size_t             first = mask.find_first_not_of (L" \t");
        size_t             last  = mask.find_last_not_of (L" \t");



        if (first != std::wstring_view::npos)
        {
            m_masks.emplace_back (mask.substr (first, last - first + 1));
        }

        if (end == std::wstring_view::npos)
        {
            break;
        }

        start = end + 1;
    }

    m_matchesAll = m_masks.empty();

    for (const std::wstring & mask : m_masks)
    {
        m_matchesAll = m_matchesAll || mask == L"*.*" || mask == L"*";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMaskSet::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool FileMaskSet::Matches (std::wstring_view name) const
{
    if (m_matchesAll)
    {
        return true;
    }

    for (const std::wstring & mask : m_masks)
    {
        if (MatchesMask (mask, name))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMaskSet::GetFirstExtension
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMaskSet::GetFirstExtension() const
{
    constexpr std::wstring_view  kPrefix = L"*.";



    for (const std::wstring & mask : m_masks)
    {
        std::wstring_view  extension = std::wstring_view (mask).substr (std::min (mask.size(), kPrefix.size()));
        bool               isPlain   = mask.starts_with (kPrefix) && !extension.empty() && extension.find_first_of (L"*?.") == std::wstring_view::npos;



        if (isPlain)
        {
            return std::wstring (extension);
        }
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMaskSet::MatchesMask
//
//  A `*` is tried at its shortest first; when the rest of the mask then
//  fails, the match returns to the last `*` and gives it one more character.
//
////////////////////////////////////////////////////////////////////////////////

bool FileMaskSet::MatchesMask (std::wstring_view mask, std::wstring_view name)
{
    constexpr size_t  kNone = std::wstring_view::npos;



    size_t  m        = 0;
    size_t  n        = 0;
    size_t  starMask = kNone;
    size_t  starName = 0;



    while (n < name.size())
    {
        if (m < mask.size() && (mask[m] == L'?' || towlower (mask[m]) == towlower (name[n])))
        {
            m++;
            n++;
        }
        else if (m < mask.size() && mask[m] == L'*')
        {
            starMask = m++;
            starName = n;
        }
        else if (starMask != kNone)
        {
            m = starMask + 1;
            n = ++starName;
        }
        else
        {
            return false;
        }
    }

    while (m < mask.size() && mask[m] == L'*')
    {
        m++;
    }

    return m == mask.size();
}
