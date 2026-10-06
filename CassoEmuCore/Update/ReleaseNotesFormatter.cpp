#include "Pch.h"

#include "Update/ReleaseNotesFormatter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::Format
//
//  One FormattedLine per heading, bullet or paragraph. A non-blank line that
//  follows a bullet or paragraph line directly continues it, which is how a
//  wrapped CHANGELOG bullet reads. Runs of blank lines collapse to one
//  Blank, and none is kept at the end.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesFormatter::Format (
    const std::string           & markdown,
    std::vector<FormattedLine>  & outLines)
{
    std::string_view  rest    = markdown;
    std::string_view  line;
    std::string_view  trimmed;
    size_t            lf      = 0;
    size_t            first   = 0;
    bool              isOpen  = false;
    FormattedLine     parsed;



    outLines.clear();

    while (!rest.empty())
    {
        lf   = rest.find ('\n');
        line = rest.substr (0, lf);
        rest = (lf == std::string_view::npos) ? std::string_view() : rest.substr (lf + 1);

        if (line.ends_with ('\r'))
        {
            line.remove_suffix (1);
        }

        first   = line.find_first_not_of (" \t");
        trimmed = (first == std::string_view::npos) ? std::string_view() : line.substr (first);

        if (trimmed.empty())
        {
            if (!outLines.empty() && outLines.back().kind != FormattedLineKind::Blank)
            {
                outLines.push_back (FormattedLine { FormattedLineKind::Blank });
            }

            isOpen = false;
            continue;
        }

        if (TryParseHeading (line, parsed))
        {
            outLines.push_back (std::move (parsed));
            isOpen = false;
            continue;
        }

        if (TryParseBullet (line, parsed))
        {
            outLines.push_back (std::move (parsed));
            isOpen = true;
            continue;
        }

        if (isOpen)
        {
            AppendContinuation (outLines.back(), trimmed);
            continue;
        }

        parsed      = {};
        parsed.kind = FormattedLineKind::Paragraph;
        FormatInline (trimmed, parsed.runs);
        outLines.push_back (std::move (parsed));
        isOpen = true;
    }

    while (!outLines.empty() && outLines.back().kind == FormattedLineKind::Blank)
    {
        outLines.pop_back();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::TryParseHeading
//
//  "#" to "####" at the start of the line, then a space. Deeper headings are
//  not in the subset and stay paragraphs.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesFormatter::TryParseHeading (std::string_view text, FormattedLine & outLine)
{
    static constexpr int  kMaxHeadingLevel = 4;
    size_t                hashes           = text.find_first_not_of ('#');
    int                   level            = (int) hashes;
    bool                  isHeading        = false;



    outLine   = {};
    isHeading = hashes != std::string_view::npos &&
                level >= 1                       &&
                level <= kMaxHeadingLevel        &&
                text[hashes] == ' ';

    if (isHeading)
    {
        outLine.kind         = FormattedLineKind::Heading;
        outLine.headingLevel = level;
        FormatInline (text.substr (hashes + 1), outLine.runs);
    }

    return isHeading;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::TryParseBullet
//
//  "- " or "* " after any indent. Every two columns of indent is one level
//  of nesting.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesFormatter::TryParseBullet (std::string_view text, FormattedLine & outLine)
{
    static constexpr int  kColumnsPerLevel = 2;
    size_t                indent           = text.find_first_not_of (' ');
    bool                  isBullet         = false;



    outLine  = {};
    isBullet = indent != std::string_view::npos              &&
               indent + 1 < text.size()                      &&
               (text[indent] == '-' || text[indent] == '*')  &&
               text[indent + 1] == ' ';

    if (isBullet)
    {
        outLine.kind        = FormattedLineKind::Bullet;
        outLine.indentLevel = (int) indent / kColumnsPerLevel;
        FormatInline (text.substr (indent + 2), outLine.runs);
    }

    return isBullet;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::AppendContinuation
//
//  Joins a wrapped line onto the block it continues, with one space.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesFormatter::AppendContinuation (FormattedLine & line, std::string_view text)
{
    std::vector<FormattedRun>  runs;



    FormatInline (text, runs);
    AppendRun (line.runs, " ", false, false, "");

    for (const FormattedRun & run : runs)
    {
        AppendRun (line.runs, run.text, run.bold, run.code, run.linkUrl);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::FormatInline
//
//  **bold**, `code` and [text](url). A marker without its partner is plain
//  text, so an unmatched ** or ` shows as typed.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesFormatter::FormatInline (std::string_view text, std::vector<FormattedRun> & outRuns)
{
    static constexpr std::string_view  kBold = "**";
    std::string       plain;
    std::string_view  label;
    std::string_view  url;
    size_t            i     = 0;
    size_t            close = 0;
    size_t            end   = 0;
    bool              bold  = false;



    outRuns.clear();

    while (i < text.size())
    {
        if (text.substr (i).starts_with (kBold) && (bold || text.find (kBold, i + kBold.size()) != std::string_view::npos))
        {
            AppendRun (outRuns, plain, bold, false, "");
            plain.clear();
            bold = !bold;
            i   += kBold.size();
            continue;
        }

        close = (text[i] == '`') ? text.find ('`', i + 1) : std::string_view::npos;

        if (close != std::string_view::npos)
        {
            AppendRun (outRuns, plain, bold, false, "");
            plain.clear();
            AppendRun (outRuns, text.substr (i + 1, close - i - 1), bold, true, "");
            i = close + 1;
            continue;
        }

        if (text[i] == '[' && TryParseLink (text, i, label, url, end))
        {
            AppendRun (outRuns, plain, bold, false, "");
            plain.clear();
            AppendRun (outRuns, label, bold, false, url);
            i = end;
            continue;
        }

        plain += text[i];
        i++;
    }

    AppendRun (outRuns, plain, bold, false, "");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::TryParseLink
//
//  "[label](url)" starting at `start`; `outEnd` is just past the ')'.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesFormatter::TryParseLink (
    std::string_view    text,
    size_t              start,
    std::string_view  & outLabel,
    std::string_view  & outUrl,
    size_t            & outEnd)
{
    size_t  close  = text.find (']', start + 1);
    size_t  paren  = std::string_view::npos;
    bool    isLink = false;



    if (close != std::string_view::npos && close + 1 < text.size() && text[close + 1] == '(')
    {
        paren = text.find (')', close + 2);
    }

    isLink = paren != std::string_view::npos && paren > close + 2;

    if (isLink)
    {
        outLabel = text.substr (start + 1, close - start - 1);
        outUrl   = text.substr (close + 2, paren - close - 2);
        outEnd   = paren + 1;
    }

    return isLink;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::AppendRun
//
//  Adds a run, merging it into the last one when the style matches. Empty
//  text adds nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesFormatter::AppendRun (
    std::vector<FormattedRun>  & runs,
    std::string_view             text,
    bool                         bold,
    bool                         code,
    std::string_view             linkUrl)
{
    bool  isMergeable = false;



    if (text.empty())
    {
        return;
    }

    isMergeable = !runs.empty()              &&
                  runs.back().bold == bold   &&
                  runs.back().code == code   &&
                  runs.back().linkUrl == linkUrl;

    if (isMergeable)
    {
        runs.back().text += text;
    }
    else
    {
        runs.push_back (FormattedRun { std::string (text), bold, code, std::string (linkUrl) });
    }
}
