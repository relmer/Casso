#include "Pch.h"

#include "DiskCheck/BootTitle.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BootTitle::Find
//
//  The line over a copyright line is the first choice, as a page of prose can
//  have one line in its first four.
//
////////////////////////////////////////////////////////////////////////////////

std::string BootTitle::Find (const std::vector<std::string> & rows, bool isTextMode)
{
    std::string  title;
    size_t       filled = 0;
    size_t       only   = 0;



    if (!isTextMode)
    {
        return title;
    }

    for (size_t at = 0; at < rows.size() && title.empty(); at++)
    {
        if (!HasCopyright (rows[at]))
        {
            continue;
        }

        for (size_t back = 1; back <= s_kCopyrightReach && back <= at && title.empty(); back++)
        {
            if (!IsBlank (rows[at - back]) && !HasCopyright (rows[at - back]))
            {
                title = Trim (rows[at - back]);
            }
        }
    }

    for (size_t at = 0; at < s_kTopRows && at < rows.size(); at++)
    {
        if (!IsBlank (rows[at]))
        {
            filled++;
            only = at;
        }
    }

    if (title.empty() && filled == 1 && !HasCopyright (rows[only]))
    {
        title = Trim (rows[only]);
    }

    return title;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootTitle::ToName
//
////////////////////////////////////////////////////////////////////////////////

std::string BootTitle::ToName (const std::string & title)
{
    bool         hasSmall = std::any_of (title.begin(), title.end(), [] (char each) { return islower ((unsigned char) each) != 0; });
    std::string  name;
    size_t       from     = 0;



    //  Each part between colons, with the spaces at its ends dropped.
    while (from <= title.size())
    {
        size_t       colon = title.find (':', from);
        size_t       end   = (colon == std::string::npos) ? title.size() : colon;
        std::string  part  = Trim (title.substr (from, end - from));
        size_t       at    = 0;

        from = end + 1;

        if (part.empty())
        {
            continue;
        }

        name += name.empty() ? "" : " - ";

        while (at < part.size())
        {
            size_t  space = part.find (' ', at);

            space = (space == std::string::npos) ? part.size() : space;

            if (space > at)
            {
                name += (at == 0) ? "" : " ";
                name += hasSmall ? part.substr (at, space - at) : ToTitleCase (part.substr (at, space - at), at == 0);
            }

            at = space + 1;
        }
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootTitle::ToTitleCase
//
//  A short joining word after the first is in small letters.
//
////////////////////////////////////////////////////////////////////////////////

std::string BootTitle::ToTitleCase (const std::string & word, bool isFirst)
{
    static constexpr std::array<std::string_view, 9>  s_kSmall   = { "A", "AN", "AND", "IN", "OF", "ON", "OR", "THE", "TO" };
    bool                                              isNumeral  = !word.empty() && word.find_first_not_of ("IVX") == std::string::npos;
    bool                                              isSmall    = !isFirst && std::find (s_kSmall.begin(), s_kSmall.end(), word) != s_kSmall.end();
    std::string                                       cased      = word;
    bool                                              isStart    = true;



    if (isNumeral || word == "DOS")
    {
        return word;
    }

    for (char & each : cased)
    {
        each    = (isStart && !isSmall) ? each : (char) tolower ((unsigned char) each);
        isStart = isalnum ((unsigned char) each) == 0;
    }

    return cased;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootTitle::IsBlank
//
////////////////////////////////////////////////////////////////////////////////

bool BootTitle::IsBlank (const std::string & row)
{
    return std::all_of (row.begin(), row.end(), [] (char each) { return each == ' '; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootTitle::HasCopyright
//
////////////////////////////////////////////////////////////////////////////////

bool BootTitle::HasCopyright (const std::string & row)
{
    std::string  upper = row;



    std::transform (upper.begin(), upper.end(), upper.begin(), [] (char each) { return (char) toupper ((unsigned char) each); });

    return upper.find ("COPYRIGHT") != std::string::npos || upper.find ("(C)") != std::string::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootTitle::Trim
//
//  Empty for a line with fewer letters and digits than a title has, as a
//  prompt's bracket is.
//
////////////////////////////////////////////////////////////////////////////////

std::string BootTitle::Trim (const std::string & row)
{
    auto    isKept  = [] (char each) { return isalnum ((unsigned char) each) != 0; };
    auto    first   = std::find_if (row.begin(), row.end(), isKept);
    auto    last    = std::find_if (row.rbegin(), row.rend(), isKept).base();
    size_t  letters = (size_t) std::count_if (row.begin(), row.end(), isKept);



    return (first < last && letters >= s_kMinLetters) ? std::string (first, last) : std::string();
}
