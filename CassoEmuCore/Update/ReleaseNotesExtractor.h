#pragma once

#include "Pch.h"

#include "Update/ReleaseVersion.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NotesSection
//
//  One release's slice of CHANGELOG.md or README.md: its version (patch 0
//  for a README highlight, which covers a minor line), the heading text, and
//  the markdown body under the heading.
//
////////////////////////////////////////////////////////////////////////////////

struct NotesSection
{
    ReleaseVersion  version;
    std::string     heading;
    std::string     body;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotes
//
//  What the update dialog shows: README highlights and CHANGELOG sections,
//  each newest first.
//
////////////////////////////////////////////////////////////////////////////////

struct ReleaseNotes
{
    std::vector<NotesSection>  highlights;
    std::vector<NotesSection>  changes;
    std::string                runningReleaseDate;   // "YYYY-MM-DD", empty when the CHANGELOG has none
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesExtractor
//
//  Slices the sections between the running build and a newer release out of
//  CHANGELOG.md and README.md as the release tag has them.
//
////////////////////////////////////////////////////////////////////////////////

class ReleaseNotesExtractor
{
public:
    static HRESULT           ExtractChanges           (const std::string          & changelog,
                                                       const ReleaseVersion       & running,
                                                       const ReleaseVersion       & newer,
                                                       std::vector<NotesSection>  & outSections);
    static HRESULT           ExtractHighlights        (const std::string          & readme,
                                                       const ReleaseVersion       & running,
                                                       const ReleaseVersion       & newer,
                                                       std::vector<NotesSection>  & outSections);
    static bool              TryParseChangelogHeading (std::string_view   line,
                                                       ReleaseVersion   & outVersion);
    static bool              TryGetReleaseDate        (const std::string     & changelog,
                                                       const ReleaseVersion  & version,
                                                       std::string           & outDate);
    static bool              IsDateText               (std::string_view text);
    static bool              TryParseHighlightHeading (std::string_view   line,
                                                       ReleaseVersion   & outVersion,
                                                       std::string      & outTitle);

private:
    static void              SplitLines               (const std::string              & text,
                                                       std::vector<std::string_view>  & outLines);
    static bool              IsHeadingAtLevel         (std::string_view line, int minLevel, int maxLevel);
    static std::string       JoinBody                 (const std::vector<std::string_view> & lines,
                                                       size_t                                first,
                                                       size_t                                end);
    static void              SortNewestFirst          (std::vector<NotesSection> & sections);
    static std::string_view  TrimSpaces               (std::string_view text);
};
