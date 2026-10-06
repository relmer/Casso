#include "Pch.h"

#include "Update/ReleaseNotesExtractor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesExtractor::ExtractChanges
//
//  Every CHANGELOG section whose version is after `running` and at most
//  `newer`, newest first. A section starts at a "## [x.y.z]" line and runs
//  to the next "## " line, so "## [Unreleased]" never matches and ends the
//  section above it. No section for `newer` itself means the document is not
//  the one expected: ERROR_NOT_FOUND rather than an empty, successful list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReleaseNotesExtractor::ExtractChanges (
    const std::string          & changelog,
    const ReleaseVersion       & running,
    const ReleaseVersion       & newer,
    std::vector<NotesSection>  & outSections)
{
    static constexpr size_t  kHeadingPrefix = 3;     // "## "
    static constexpr int           kSectionLevel = 2;
    HRESULT                        hr            = S_OK;
    std::vector<std::string_view>  lines;
    ReleaseVersion                 version;
    size_t                         end           = 0;
    bool                           isInRange     = false;
    bool                           foundNewer    = false;



    outSections.clear();
    SplitLines (changelog, lines);

    for (size_t i = 0; i < lines.size(); i++)
    {
        if (!TryParseChangelogHeading (lines[i], version))
        {
            continue;
        }

        end = i + 1;
        while (end < lines.size() && !IsHeadingAtLevel (lines[end], kSectionLevel, kSectionLevel))
        {
            end++;
        }

        isInRange   = version > running && version <= newer;
        foundNewer |= version == newer;

        if (isInRange)
        {
            outSections.push_back (NotesSection { version,
                                                  std::string (TrimSpaces (lines[i].substr (kHeadingPrefix))),
                                                  JoinBody (lines, i + 1, end) });
        }

        i = end - 1;
    }

    CBREx (foundNewer, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    SortNewestFirst (outSections);

Error:
    if (FAILED (hr))
    {
        outSections.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesExtractor::ExtractHighlights
//
//  Every README release highlight for a minor line after the running
//  build's and at most the newer release's, newest first. A highlight starts
//  at a "### [<date> . x.y] <title>" line (a middle dot, not a period) and
//  runs to the next heading of level 1 to 3. A patch release within the
//  running minor line has no highlights, and an empty list is correct then;
//  a newer minor line with no highlight of its own is ERROR_NOT_FOUND.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReleaseNotesExtractor::ExtractHighlights (
    const std::string          & readme,
    const ReleaseVersion       & running,
    const ReleaseVersion       & newer,
    std::vector<NotesSection>  & outSections)
{
    static constexpr int           kMinEndLevel = 1;
    static constexpr int           kMaxEndLevel = 3;
    HRESULT                        hr           = S_OK;
    std::vector<std::string_view>  lines;
    ReleaseVersion                 version;
    std::string                    title;
    size_t                         end          = 0;
    bool                           isInRange    = false;
    bool                           needsNewer   = newer.CompareMinorLine (running) > 0;
    bool                           foundNewer   = false;



    outSections.clear();
    SplitLines (readme, lines);

    for (size_t i = 0; i < lines.size(); i++)
    {
        if (!TryParseHighlightHeading (lines[i], version, title))
        {
            continue;
        }

        end = i + 1;
        while (end < lines.size() && !IsHeadingAtLevel (lines[end], kMinEndLevel, kMaxEndLevel))
        {
            end++;
        }

        isInRange   = version.CompareMinorLine (running) > 0 && version.CompareMinorLine (newer) <= 0;
        foundNewer |= version.CompareMinorLine (newer) == 0;

        if (isInRange)
        {
            outSections.push_back (NotesSection { version, title, JoinBody (lines, i + 1, end) });
        }

        i = end - 1;
    }

    CBREx (foundNewer || !needsNewer, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    SortNewestFirst (outSections);

Error:
    if (FAILED (hr))
    {
        outSections.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesExtractor::TryParseChangelogHeading
//
//  "## [1.30.0] - 2026-10-03: ..." gives 1.30.0. Anything else, including
//  "## [Unreleased]", fails.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesExtractor::TryParseChangelogHeading (
    std::string_view   line,
    ReleaseVersion   & outVersion)
{
    static constexpr std::string_view  kPrefix = "## [";
    size_t  close    = std::string_view::npos;
    bool    isParsed = false;



    outVersion = {};

    if (line.starts_with (kPrefix))
    {
        close = line.find (']', kPrefix.size());
    }

    if (close != std::string_view::npos)
    {
        isParsed = ReleaseVersion::TryParse (line.substr (kPrefix.size(), close - kPrefix.size()), outVersion);
    }

    return isParsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesExtractor::TryParseHighlightHeading
//
//  "### [2026-10-03 <middle dot> 1.30] WOZ 2.1 flux support" gives 1.30.0
//  and the title after the bracket. The files are UTF-8 and this source is
//  not, so the middle dot is matched as its two UTF-8 bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesExtractor::TryParseHighlightHeading (
    std::string_view   line,
    ReleaseVersion   & outVersion,
    std::string      & outTitle)
{
    static constexpr std::string_view  kPrefix    = "### [";
    static constexpr std::string_view  kMiddleDot = "\xC2\xB7";
    size_t            close    = std::string_view::npos;
    size_t            dot      = std::string_view::npos;
    std::string_view  inner;
    bool              isParsed = false;



    outVersion = {};
    outTitle.clear();

    if (line.starts_with (kPrefix))
    {
        close = line.find (']', kPrefix.size());
    }

    if (close != std::string_view::npos)
    {
        inner = line.substr (kPrefix.size(), close - kPrefix.size());
        dot   = inner.rfind (kMiddleDot);
    }

    if (dot != std::string_view::npos)
    {
        // A minor line is "x.y"; completing it to "x.y.0" lets the full
        // version parser check it, and rejects "x.y.z" in the same step.
        isParsed = ReleaseVersion::TryParse (std::string (TrimSpaces (inner.substr (dot + kMiddleDot.size()))) + ".0",
                                             outVersion);
    }

    if (isParsed)
    {
        outTitle = std::string (TrimSpaces (line.substr (close + 1)));
    }

    return isParsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesExtractor::SplitLines
//
//  Splits on LF and drops a trailing CR, so CRLF and LF files read alike.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesExtractor::SplitLines (
    const std::string              & text,
    std::vector<std::string_view>  & outLines)
{
    std::string_view  rest = text;
    std::string_view  line;
    size_t            lf   = 0;



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

        outLines.push_back (line);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesExtractor::IsHeadingAtLevel
//
//  Whether `line` is a markdown heading of a level from `minLevel` to
//  `maxLevel`: that many '#' characters followed by a space.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesExtractor::IsHeadingAtLevel (std::string_view line, int minLevel, int maxLevel)
{
    size_t  hashes = line.find_first_not_of ('#');
    int     level  = (int) hashes;



    return hashes != std::string_view::npos &&
           line[hashes] == ' '              &&
           level >= minLevel                &&
           level <= maxLevel;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesExtractor::JoinBody
//
//  Lines [first, end) joined with LF, without the blank lines at either end.
//
////////////////////////////////////////////////////////////////////////////////

std::string ReleaseNotesExtractor::JoinBody (
    const std::vector<std::string_view>  & lines,
    size_t                                 first,
    size_t                                 end)
{
    std::string  body;



    while (first < end && TrimSpaces (lines[first]).empty())
    {
        first++;
    }

    while (end > first && TrimSpaces (lines[end - 1]).empty())
    {
        end--;
    }

    for (size_t i = first; i < end; i++)
    {
        if (i > first)
        {
            body += '\n';
        }

        body += lines[i];
    }

    return body;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesExtractor::SortNewestFirst
//
//  Stable, so two sections for one version keep their document order.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesExtractor::SortNewestFirst (std::vector<NotesSection> & sections)
{
    std::stable_sort (sections.begin(),
                      sections.end(),
                      [] (const NotesSection & a, const NotesSection & b) { return a.version > b.version; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesExtractor::TrimSpaces
//
////////////////////////////////////////////////////////////////////////////////

std::string_view ReleaseNotesExtractor::TrimSpaces (std::string_view text)
{
    static constexpr std::string_view  kSpaces = " \t";
    size_t  first = text.find_first_not_of (kSpaces);
    size_t  last  = text.find_last_not_of (kSpaces);



    return (first == std::string_view::npos) ? std::string_view() : text.substr (first, last - first + 1);
}
