#include "Pch.h"

#include "DiskCheck/BootTitle.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BootTitle::Find
//
//  The line over a copyright line is the first choice, as a page of prose can
//  have one line in its first four. A line that reads as a sentence, or that
//  runs on from the line above it, isn't a title, as the lines over Apple
//  Pascal's copyright aren't.
//
////////////////////////////////////////////////////////////////////////////////

std::string BootTitle::Find (const std::vector<std::string> & rows, bool isTextMode)
{
    std::string  title;
    size_t       filled = 0;
    size_t       only   = 0;
    bool         isDone = false;



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

        isDone = false;

        for (size_t back = 1; back <= s_kCopyrightReach && back <= at && !isDone; back++)
        {
            size_t  line = at - back;

            if (IsBlank (rows[line]) || HasCopyright (rows[line]))
            {
                continue;
            }

            isDone = true;
            title  = (!IsSentence (rows[line]) && IsSetOff (rows, line)) ? Trim (rows[line]) : std::string();
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

    if (title.empty() && filled == 1 && !HasCopyright (rows[only]) && !IsSentence (rows[only]))
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
//  BootTitle::IsSentence
//
//  A line that ends with a period, or that has a verb a title rarely has.
//
////////////////////////////////////////////////////////////////////////////////

bool BootTitle::IsSentence (const std::string & row)
{
    static constexpr std::array<std::string_view, 8>  s_kVerbs = { "IS", "ARE", "WAS", "WERE", "HAS", "HAVE", "WILL", "CAN" };
    std::string                                       text     = Trim (row);
    std::string                                       word;
    bool                                              isVerb   = false;



    for (size_t at = 0; at <= text.size() && !isVerb; at++)
    {
        char  each = at < text.size() ? text[at] : ' ';

        if (isalpha ((unsigned char) each))
        {
            word.push_back ((char) toupper ((unsigned char) each));
            continue;
        }

        isVerb = std::find (s_kVerbs.begin(), s_kVerbs.end(), word) != s_kVerbs.end();
        word.clear();
    }

    return isVerb || (!row.empty() && row.find_last_not_of (' ') != std::string::npos && row[row.find_last_not_of (' ')] == '.');
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootTitle::IsSetOff
//
//  On the top row, under a blank line, or centered within a character of the
//  screen's width, 80 columns when a row is longer than 40.
//
////////////////////////////////////////////////////////////////////////////////

bool BootTitle::IsSetOff (const std::vector<std::string> & rows, size_t line)
{
    size_t  width = s_kNarrowColumns;
    size_t  first = rows[line].find_first_not_of (' ');
    size_t  last  = rows[line].find_last_not_of (' ');
    size_t  right = 0;



    for (const std::string & row : rows)
    {
        width = row.find_last_not_of (' ') != std::string::npos && row.find_last_not_of (' ') >= s_kNarrowColumns ? s_kWideColumns : width;
    }

    right = (last == std::string::npos) ? 0 : width - 1 - last;

    return line == 0 || IsBlank (rows[line - 1]) || (first != std::string::npos && (first > right ? first - right : right - first) <= 1);
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
