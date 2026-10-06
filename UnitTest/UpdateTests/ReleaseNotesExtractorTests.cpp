#include "Pch.h"

#include "Update/ReleaseNotesExtractor.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesExtractorTests
//
//  CHANGELOG and README slicing between the running build and a newer
//  release, against documents laid out the way the real ones are.
//
////////////////////////////////////////////////////////////////////////////////

static const std::string s_kChangelog =
    "# Changelog\r\n"
    "\r\n"
    "## [Unreleased]\r\n"
    "\r\n"
    "- Not shipped yet\r\n"
    "\r\n"
    "## [1.31.0] - 2026-10-20: The one with updates\r\n"
    "\r\n"
    "### Added\r\n"
    "\r\n"
    "- Update notification\r\n"
    "\r\n"
    "## [1.30.1] - 2026-10-10: A fix\r\n"
    "\r\n"
    "- Fixed a thing\r\n"
    "\r\n"
    "## [1.30.0] - 2026-10-03: WOZ 2.1\r\n"
    "\r\n"
    "- Flux support\r\n"
    "\r\n"
    "## [1.29.0] - 2026-09-20: Older\r\n"
    "\r\n"
    "- Old news\r\n";

static const std::string s_kReadme =
    "# Casso\n"
    "\n"
    "## Release highlights\n"
    "\n"
    "### [2026-10-20 \xC2\xB7 1.31] Update notification\n"
    "\n"
    "Casso now tells you about new releases.\n"
    "\n"
    "### [2026-10-03 \xC2\xB7 1.30] WOZ 2.1 flux support\n"
    "\n"
    "Flux tracks load.\n"
    "#### Details\n"
    "More flux.\n"
    "\n"
    "### [2026-09-20 \xC2\xB7 1.29] Older\n"
    "\n"
    "Old news.\n"
    "\n"
    "## Building\n"
    "\n"
    "Run the build.\n";



TEST_CLASS (ReleaseNotesExtractorTests)
{
public:

    TEST_METHOD (Changes_RangeIsExclusiveInclusiveNewestFirst)
    {
        std::vector<NotesSection>  sections;
        HRESULT                    hr       = S_OK;



        hr = ReleaseNotesExtractor::ExtractChanges (s_kChangelog, { 1, 30, 0 }, { 1, 31, 0 }, sections);
        AssertSucceeded (hr);

        Assert::AreEqual ((size_t) 2, sections.size());
        Assert::IsTrue   (sections[0].version == ReleaseVersion { 1, 31, 0 });
        Assert::AreEqual (std::string ("[1.31.0] - 2026-10-20: The one with updates"), sections[0].heading);
        Assert::AreEqual (std::string ("### Added\n\n- Update notification"),          sections[0].body);
        Assert::IsTrue   (sections[1].version == ReleaseVersion { 1, 30, 1 });
        Assert::AreEqual (std::string ("- Fixed a thing"),                             sections[1].body);
    }



    TEST_METHOD (Changes_UnreleasedNeverShown)
    {
        std::vector<NotesSection>  sections;
        HRESULT                    hr       = S_OK;



        hr = ReleaseNotesExtractor::ExtractChanges (s_kChangelog, { 1, 0, 0 }, { 1, 31, 0 }, sections);
        AssertSucceeded (hr);

        Assert::AreEqual ((size_t) 4, sections.size());

        for (const NotesSection & section : sections)
        {
            Assert::IsTrue (section.body.find ("Not shipped") == std::string::npos);
        }
    }



    TEST_METHOD (Changes_NoSectionForNewer_Fails)
    {
        std::vector<NotesSection>  sections;
        HRESULT                    hr       = S_OK;



        hr = ReleaseNotesExtractor::ExtractChanges (s_kChangelog, { 1, 30, 0 }, { 1, 32, 0 }, sections);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), hr);
        Assert::IsTrue   (sections.empty());

        hr = ReleaseNotesExtractor::ExtractChanges ("<html>404</html>", { 1, 30, 0 }, { 1, 31, 0 }, sections);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), hr);
    }



    TEST_METHOD (Highlights_MinorLinesInRange)
    {
        std::vector<NotesSection>  sections;
        HRESULT                    hr       = S_OK;



        hr = ReleaseNotesExtractor::ExtractHighlights (s_kReadme, { 1, 29, 3 }, { 1, 31, 0 }, sections);
        AssertSucceeded (hr);

        Assert::AreEqual ((size_t) 2, sections.size());
        Assert::IsTrue   (sections[0].version == ReleaseVersion { 1, 31, 0 });
        Assert::AreEqual (std::string ("Update notification"),                    sections[0].heading);
        Assert::AreEqual (std::string ("Casso now tells you about new releases."), sections[0].body);
        Assert::AreEqual (std::string ("WOZ 2.1 flux support"),                   sections[1].heading);
        Assert::AreEqual (std::string ("Flux tracks load.\n#### Details\nMore flux."), sections[1].body,
                          L"a level-4 heading does not end the highlight");
    }



    TEST_METHOD (Highlights_LastOneEndsAtLevelTwoHeading)
    {
        std::vector<NotesSection>  sections;
        HRESULT                    hr       = S_OK;



        hr = ReleaseNotesExtractor::ExtractHighlights (s_kReadme, { 1, 28, 0 }, { 1, 29, 0 }, sections);
        AssertSucceeded (hr);

        Assert::AreEqual ((size_t) 1, sections.size());
        Assert::AreEqual (std::string ("Old news."), sections[0].body);
    }



    TEST_METHOD (Highlights_PatchReleaseHasNone)
    {
        std::vector<NotesSection>  sections;
        HRESULT                    hr       = S_OK;



        hr = ReleaseNotesExtractor::ExtractHighlights (s_kReadme, { 1, 31, 0 }, { 1, 31, 2 }, sections);
        AssertSucceeded (hr);
        Assert::IsTrue  (sections.empty());
    }



    TEST_METHOD (Highlights_NewMinorWithoutHighlight_Fails)
    {
        std::vector<NotesSection>  sections;
        HRESULT                    hr       = S_OK;



        hr = ReleaseNotesExtractor::ExtractHighlights (s_kReadme, { 1, 31, 0 }, { 1, 32, 0 }, sections);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), hr);
        Assert::IsTrue   (sections.empty());
    }



    TEST_METHOD (HighlightHeading_NeedsMiddleDot)
    {
        ReleaseVersion  version;
        std::string     title;



        Assert::IsFalse (ReleaseNotesExtractor::TryParseHighlightHeading ("### [2026-10-03 . 1.30] Title",    version, title));
        Assert::IsFalse (ReleaseNotesExtractor::TryParseHighlightHeading ("### [2026-10-03 \xC2\xB7 1.30.1] T", version, title));
        Assert::IsFalse (ReleaseNotesExtractor::TryParseHighlightHeading ("## [2026-10-03 \xC2\xB7 1.30] T",    version, title));
        Assert::IsTrue  (ReleaseNotesExtractor::TryParseHighlightHeading ("### [2026-10-03 \xC2\xB7 1.30] T",   version, title));
        Assert::IsTrue  (version == ReleaseVersion { 1, 30, 0 });
        Assert::AreEqual (std::string ("T"), title);
    }
};
