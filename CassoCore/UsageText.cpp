#include "Pch.h"

#include "UsageText.h"



//  A gutter is two spaces or more. One space separates words; two is someone
//  lining a column up.
static const size_t  s_kGutterWidth = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  UsageText::GetContinuationIndent
//
//  Where the rest of a wrapped line belongs.
//
//  Found from the text rather than passed in by every caller, so a help line
//  and its continuation cannot disagree about the column -- and so adding a row
//  to a table is one string rather than a string plus a number.
//
////////////////////////////////////////////////////////////////////////////////

size_t UsageText::GetContinuationIndent (const std::string & line)
{
    size_t  leading = line.find_first_not_of (' ');
    size_t  indent  = (leading == std::string::npos) ? 0 : leading;
    size_t  scan    = indent;



    // The LAST gutter, not the first: a description may itself contain a run of
    // spaces, and it is the rightmost column boundary that the reader sees as
    // the edge of the text.
    while (scan < line.size())
    {
        size_t  gap = line.find ("  ", scan);

        if (gap == std::string::npos)
        {
            break;
        }

        size_t  after = line.find_first_not_of (' ', gap);

        if (after == std::string::npos)
        {
            break;
        }

        indent = after;
        scan   = after;
    }

    return indent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UsageText::Wrap
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> UsageText::Wrap (const std::string & line, size_t width)
{
    std::vector<std::string>  rows;
    size_t                    indent = GetContinuationIndent (line);
    size_t                    taken  = 0;



    // A width that cannot hold the indent plus a character of text leaves
    // nothing to wrap INTO, and the loop below would never advance. Handing the
    // line back whole is the honest answer for a terminal that narrow.
    if (width == 0 || line.size() <= width || indent + s_kGutterWidth >= width)
    {
        rows.push_back (line);
        return rows;
    }

    while (taken < line.size())
    {
        size_t  room    = (taken == 0) ? width : width - indent;
        size_t  remains = line.size() - taken;
        size_t  chunk   = std::min (room, remains);
        size_t  brk     = std::string::npos;

        if (taken > 0)
        {
            // A continuation starts at text. The spaces at the break are the
            // break, not the first word of what follows.
            while (taken < line.size() && line[taken] == ' ')
            {
                taken++;
            }

            remains = line.size() - taken;
            chunk   = std::min (room, remains);
        }

        if (taken >= line.size())
        {
            break;
        }

        if (chunk < remains)
        {
            brk = line.rfind (' ', taken + chunk);
        }

        if (brk != std::string::npos && brk > taken)
        {
            chunk = brk - taken;
        }
        else if (chunk < remains)
        {
            // No space inside the room available, so the word is wider than the
            // column. It runs to its own end and overhangs: a path or a URL cut
            // across two rows leaves the reader unable to tell whether the
            // break is part of it.
            size_t  next = line.find (' ', taken);

            chunk = (next == std::string::npos) ? remains : next - taken;
        }

        rows.push_back ((taken == 0 ? std::string() : std::string (indent, ' '))
                        + line.substr (taken, chunk));

        taken += chunk;
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UsageText::ResolveWidth
//
//  How wide to fold help, given what the environment says and what the console
//  reported.
//
//  Three answers in order. COLUMNS when it is set and sane, because a reader
//  who overrides it means it and because a script can set it and then measure
//  what came back -- which is the only way the whole path gets tested at all.
//  Then the console, less one column: writing INTO the last one makes some
//  terminals wrap on their own, which puts a blank line between every row.
//  Then 80, for a stream that has no width to ask about.
//
//  A width at or under kNarrowestTerminal is ignored rather than honored. The
//  flag table's gutter alone is 27 columns, so folding to 20 would leave one
//  word per line and a page nobody can read; a terminal that narrow is better
//  served by lines that overhang it.
//
////////////////////////////////////////////////////////////////////////////////

size_t UsageText::ResolveWidth (const char * columnsEnv, bool hasConsole, int consoleColumns)
{
    size_t  width = kNoTerminal;
    int     asked = 0;



    if (columnsEnv != nullptr)
    {
        //  Read by hand rather than with atoi, so a value that is not a number
        //  at all is ignored instead of quietly reading as zero.
        for (const char * scan = columnsEnv; *scan != '\0'; scan++)
        {
            if (*scan < '0' || *scan > '9')
            {
                asked = 0;
                break;
            }

            asked = (asked * 10) + (*scan - '0');

            if (asked > 10000)
            {
                break;
            }
        }

        if (asked > (int) kNarrowestTerminal)
        {
            return (size_t) asked;
        }
    }

    if (hasConsole && consoleColumns > (int) kNarrowestTerminal)
    {
        width = (size_t) consoleColumns - 1;
    }

    return width;
}
