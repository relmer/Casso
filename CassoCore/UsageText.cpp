#include "Pch.h"

#include "UsageText.h"



//  A gutter is two spaces or more. One space separates words; two is someone
//  lining a column up.
static const size_t  s_kGutterWidth = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  UsageText::ContinuationIndent
//
//  Where the rest of a wrapped line belongs.
//
//  Found from the text rather than passed in by every caller, so a help line
//  and its continuation cannot disagree about the column -- and so adding a row
//  to a table is one string rather than a string plus a number.
//
////////////////////////////////////////////////////////////////////////////////

size_t UsageText::ContinuationIndent (const std::string & line)
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
    size_t                    indent = ContinuationIndent (line);
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
