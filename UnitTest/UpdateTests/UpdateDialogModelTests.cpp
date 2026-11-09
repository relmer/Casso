#include "Pch.h"

#include "Update/UpdateDialogModel.h"
#include "Ui/Dialogs/UpdateDialogContent.h"
#include "Core/UnicodeSymbols.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModelTests
//
//  Which actions the update dialog offers for each kind of copy, and the
//  text it shows.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (UpdateDialogModelTests)
{
public:

    TEST_METHOD (Buttons_OfficialCopyWithAsset_GetsUpdateNow)
    {
        Assert::IsTrue (UpdateDialogModel::SelectButtons (InstallType::Zip,  true) == UpdateButtonSet::UpdateNow);
        Assert::IsTrue (UpdateDialogModel::SelectButtons (InstallType::Msix, true) == UpdateButtonSet::UpdateNow);
    }



    TEST_METHOD (Buttons_NoAsset_GetsTheReleasePage)
    {
        Assert::IsTrue (UpdateDialogModel::SelectButtons (InstallType::Zip,  false) == UpdateButtonSet::ReleasePage);
        Assert::IsTrue (UpdateDialogModel::SelectButtons (InstallType::Msix, false) == UpdateButtonSet::ReleasePage);
    }



    TEST_METHOD (Buttons_DeveloperBuild_NeverGetsUpdateNow)
    {
        Assert::IsTrue (UpdateDialogModel::SelectButtons (InstallType::Developer, true)  == UpdateButtonSet::Developer);
        Assert::IsTrue (UpdateDialogModel::SelectButtons (InstallType::Developer, false) == UpdateButtonSet::Developer);
    }



    TEST_METHOD (Buttons_UnknownInstallType_GetsTheReleasePage)
    {
        Assert::IsTrue (UpdateDialogModel::SelectButtons (InstallType::Unknown, true) == UpdateButtonSet::ReleasePage);
    }



    TEST_METHOD (AfterFailure_OnlyUnfixableFailuresDropUpdateNow)
    {
        Assert::IsTrue (UpdateDialogModel::SelectAfterFailure (UpdateButtonSet::UpdateNow, UpdateFailure::FolderNotWritable) == UpdateButtonSet::ReleasePage);
        Assert::IsTrue (UpdateDialogModel::SelectAfterFailure (UpdateButtonSet::UpdateNow, UpdateFailure::NoAsset)           == UpdateButtonSet::ReleasePage);
        Assert::IsTrue (UpdateDialogModel::SelectAfterFailure (UpdateButtonSet::UpdateNow, UpdateFailure::Network)           == UpdateButtonSet::UpdateNow);
        Assert::IsTrue (UpdateDialogModel::SelectAfterFailure (UpdateButtonSet::UpdateNow, UpdateFailure::OtherInstanceRunning) == UpdateButtonSet::UpdateNow);
    }



    TEST_METHOD (HasAsset_MatchesInstallTypeAndArch)
    {
        ReleaseInfo   release;
        ReleaseAsset  zip;
        ReleaseAsset  bundle;



        release.version = { 1, 31, 0 };
        zip.name        = "Casso-1.31.0-x64.zip";
        bundle.name     = "Casso-1.31.0.msixbundle";
        release.assets  = { zip };

        Assert::IsTrue  (UpdateDialogModel::HasAsset (release, InstallType::Zip,  ReleaseArch::X64));
        Assert::IsFalse (UpdateDialogModel::HasAsset (release, InstallType::Zip,  ReleaseArch::Arm64));
        Assert::IsFalse (UpdateDialogModel::HasAsset (release, InstallType::Msix, ReleaseArch::X64));

        release.assets.push_back (bundle);
        Assert::IsTrue  (UpdateDialogModel::HasAsset (release, InstallType::Msix,      ReleaseArch::X64));
        Assert::IsFalse (UpdateDialogModel::HasAsset (release, InstallType::Developer, ReleaseArch::X64));
    }



    TEST_METHOD (Text_HeaderDateAndProgress)
    {
        Assert::AreEqual (std::wstring (L"Casso 1.31.0 (released 2026-10-20) is available. Sadly, you're still using 1.30.0") + s_kchEmDash + L"how gauche.",
                          UpdateDialogModel::MakeHeader ({ 1, 31, 0 }, "2026-10-20", { 1, 30, 0 }, L"how gauche"));
        Assert::AreEqual (std::wstring (L"Casso 1.31.0 is available. Sadly, you're still using 1.30.0") + s_kchEmDash + L"how quaint.",
                          UpdateDialogModel::MakeHeader ({ 1, 31, 0 }, "", { 1, 30, 0 }, L"how quaint"),
                          L"no date, no parenthesis");
        Assert::AreEqual (std::wstring (L"Casso was updated to version 1.30.0."), UpdateDialogModel::MakeUpdatedNotice ("1.30.0"));
        Assert::AreEqual (std::wstring (L"Downloading: 1.0 of 4.0 MB"),
                          UpdateDialogModel::MakeProgressText (1024 * 1024, 4 * 1024 * 1024));
        Assert::AreEqual (std::wstring (L"Downloading: 0.5 MB"), UpdateDialogModel::MakeProgressText (512 * 1024, 0));
    }



    TEST_METHOD (Text_EveryFailureHasACause)
    {
        for (UpdateFailure failure : { UpdateFailure::Network,         UpdateFailure::RateLimited,
                                       UpdateFailure::BadData,         UpdateFailure::NoAsset,
                                       UpdateFailure::DigestMismatch,  UpdateFailure::NotOfficial,
                                       UpdateFailure::FolderNotWritable, UpdateFailure::OtherInstanceRunning,
                                       UpdateFailure::InstallFailed,   UpdateFailure::RestoreFailed })
        {
            Assert::IsFalse (UpdateDialogModel::DescribeFailure (failure).empty());
        }

        Assert::IsTrue (UpdateDialogModel::MakeCheckFailedText (UpdateFailure::Network).starts_with (L"Error: update check failed\n"));
        Assert::IsTrue (UpdateDialogModel::DescribeFailure (UpdateFailure::None).empty());
    }



    //  Every remark, dropped into the header, makes one well-formed sentence:
    //  lower-case start (it follows the dash), no punctuation of its own at
    //  the end, no stray spaces, and exactly one closing period.
    TEST_METHOD (Judgements_EveryEntryMakesAWellFormedHeader)
    {
        UpdateDialogModel::JudgementList  list  = UpdateDialogModel::GetJudgements();
        std::wstring                      header;



        Assert::IsTrue (list.size() >= 10, L"a list to rotate through, not one remark");

        for (LPCWSTR judgement : list)
        {
            std::wstring_view  text = judgement;

            Assert::IsFalse (text.empty());
            Assert::IsTrue  (text.front() != L' ' && text.back() != L' ');
            Assert::IsTrue  (std::wstring_view (L".!?,;:").find (text.back()) == std::wstring_view::npos);
            Assert::IsTrue  (iswlower (text.front()) || text.starts_with (L"Woz") || text.starts_with (L"the Apple"),
                             judgement);

            header = UpdateDialogModel::MakeHeader ({ 1, 30, 0 }, "2026-10-03", { 1, 29, 0 }, text);
            Assert::IsTrue  (header.ends_with (std::wstring (L"1.29.0") + s_kchEmDash + judgement + L"."));
            Assert::IsTrue  (header.find (L"  ") == std::wstring::npos);
            Assert::IsTrue  (header.find (L"..") == std::wstring::npos);
        }
    }



    TEST_METHOD (PickJudgement_UsesTheInjectedRandomSource)
    {
        UpdateDialogModel::JudgementList  list  = UpdateDialogModel::GetJudgements();
        size_t                            asked = 0;



        Assert::AreEqual (std::wstring (list[0]), UpdateDialogModel::PickJudgement ([] (size_t) { return (size_t) 0; }));
        Assert::AreEqual (std::wstring (list[2]), UpdateDialogModel::PickJudgement ([&asked] (size_t count) { asked = count; return (size_t) 2; }));
        Assert::AreEqual (list.size(), asked, L"the source is told how many there are");
        Assert::AreEqual (std::wstring (list.back()), UpdateDialogModel::PickJudgement ([] (size_t) { return (size_t) 9999; }),
                          L"an index out of range is clamped");
    }



    TEST_METHOD (DaysSince_CountsCalendarDays)
    {
        constexpr std::int64_t  kOct3Noon = 1759492800;   // 2025-10-03 12:00 UTC
        constexpr std::int64_t  kDay      = 86400;
        int                     days      = -1;



        Assert::IsTrue   (UpdateDialogModel::TryGetDaysSince ("2025-10-03", kOct3Noon, days));
        Assert::AreEqual (0, days, L"the same day");
        Assert::IsTrue   (UpdateDialogModel::TryGetDaysSince ("2025-10-02", kOct3Noon, days));
        Assert::AreEqual (1, days);
        Assert::IsTrue   (UpdateDialogModel::TryGetDaysSince ("2025-08-17", kOct3Noon, days));
        Assert::AreEqual (47, days);
        Assert::IsTrue   (UpdateDialogModel::TryGetDaysSince ("2025-10-03", kOct3Noon + kDay / 2 - 1, days));
        Assert::AreEqual (0, days, L"still the same UTC day just before midnight");
    }



    TEST_METHOD (AgeRemarks_MalSaysDaysAgo)
    {
        size_t  count = UpdateDialogModel::GetAgeRemarkCount();



        Assert::AreEqual (std::wstring (L"Mal would've updated 1 day ago."),   UpdateDialogModel::MakeAgeRemark (count - 1, 1,  { 1, 29, 0 }));
        Assert::AreEqual (std::wstring (L"Mal would've updated 47 days ago."), UpdateDialogModel::MakeAgeRemark (count - 1, 47, { 1, 29, 0 }));
    }



    TEST_METHOD (DaysSince_FutureOrBadDate_HasNoAge)
    {
        constexpr std::int64_t  kOct3Noon = 1759492800;
        int                     days      = -1;



        Assert::IsFalse (UpdateDialogModel::TryGetDaysSince ("2025-10-04", kOct3Noon, days), L"a date in the future");
        Assert::IsFalse (UpdateDialogModel::TryGetDaysSince ("2025-02-30", kOct3Noon, days), L"no such day");
        Assert::IsFalse (UpdateDialogModel::TryGetDaysSince ("2025-13-01", kOct3Noon, days));
        Assert::IsFalse (UpdateDialogModel::TryGetDaysSince ("",           kOct3Noon, days));
        Assert::IsFalse (UpdateDialogModel::TryGetDaysSince ("10/03/2025", kOct3Noon, days));
    }



    //  Every age remark, at one day and at many, is complete text: the count
    //  is filled in with the right noun, no placeholder survives, and the
    //  header puts it on its own line after a plain sentence.
    TEST_METHOD (AgeRemarks_EveryEntryIsWellFormed)
    {
        size_t        count  = UpdateDialogModel::GetAgeRemarkCount();
        std::wstring  remark;
        std::wstring  header;
        size_t        i      = 0;



        Assert::IsTrue (count >= 5);

        for (i = 0; i < count; i++)
        {
            for (int days : { 1, 47 })
            {
                remark = UpdateDialogModel::MakeAgeRemark (i, days, { 1, 29, 0 });

                Assert::IsTrue (remark.find (L'{') == std::wstring::npos && remark.find (L'}') == std::wstring::npos, remark.c_str());
                Assert::IsTrue (remark.find (days == 1 ? L"1 day" : L"47 days") != std::wstring::npos, remark.c_str());
                Assert::IsTrue (remark.find (L"1 days") == std::wstring::npos);
                Assert::IsTrue (remark.find (std::wstring (L" ") + s_kchEmDash) == std::wstring::npos && remark.find (std::wstring (1, s_kchEmDash) + L" ") == std::wstring::npos,
                                L"em dashes abut");
                Assert::IsTrue (iswupper (remark.front()) || iswdigit (remark.front()), remark.c_str());
                Assert::IsTrue (std::wstring_view (L".?!").find (remark.back()) != std::wstring_view::npos, remark.c_str());

                header = UpdateDialogModel::MakeAgeHeader ({ 1, 30, 0 }, "2025-10-03", { 1, 29, 0 }, remark);
                Assert::IsTrue (header.ends_with (L"still using 1.29.0.\n" + remark), header.c_str());
            }
        }
    }



    TEST_METHOD (FinalHeader_PoolsDependOnTheKnownAge)
    {
        size_t        shortCount = UpdateDialogModel::GetJudgements().size();
        size_t        seen       = 0;
        std::wstring  header;



        header = UpdateDialogModel::MakeFinalHeader ({ 1, 30, 0 }, "", { 1, 29, 0 }, 47, [] (size_t) { return (size_t) 0; });
        Assert::IsTrue (header.ends_with (std::wstring (L"1.29.0") + s_kchEmDash + L"how gauche."), L"a short remark ends the sentence");

        header = UpdateDialogModel::MakeFinalHeader ({ 1, 30, 0 }, "", { 1, 29, 0 }, 47,
                                                     [&seen, shortCount] (size_t count) { seen = count; return shortCount; });
        Assert::AreEqual (shortCount + UpdateDialogModel::GetAgeRemarkCount(), seen, L"both pools when the age is known");
        Assert::IsTrue   (header.find (L"1.29.0.\n") != std::wstring::npos && header.find (L"47 days") != std::wstring::npos);

        header = UpdateDialogModel::MakeFinalHeader ({ 1, 30, 0 }, "", { 1, 29, 0 }, std::nullopt,
                                                     [&seen] (size_t count) { seen = count; return count + 5; });
        Assert::AreEqual (shortCount, seen, L"only the short pool without an age");
        Assert::IsTrue   (header.find (L'\n') == std::wstring::npos);
    }



    TEST_METHOD (Openers_IncludeTheOwnersLinesAndAreWellFormed)
    {
        std::vector<std::wstring>         list  = UpdateDialogModel::GetOpeners();
        std::set<std::wstring>            texts;



        Assert::IsTrue (list.size() >= 6);

        for (const std::wstring & text : list)
        {
            Assert::IsTrue (iswupper (text.front()), text.c_str());
            Assert::IsTrue (std::wstring_view (L".!").find (text.back()) != std::wstring_view::npos, text.c_str());
            Assert::IsTrue (text.find (L"  ") == std::wstring::npos && text.find (L'\n') == std::wstring::npos, text.c_str());
            Assert::IsTrue (text.find_first_of (L"{}") == std::wstring::npos, L"every placeholder is filled");
            Assert::IsTrue (text.find (std::wstring (L" ") + s_kchEmDash) == std::wstring::npos &&
                            text.find (std::wstring (1, s_kchEmDash) + L" ") == std::wstring::npos, L"em dashes abut");
            texts.insert (text);
        }

        Assert::IsTrue (texts.contains (L"Ooh ooh, new toys, new toys!!"));
        Assert::IsTrue (texts.contains (L"ZOMG! Fresh Casso available!!"));
        Assert::IsTrue (texts.contains (L"I love it when a plan comes together."));
        Assert::IsTrue (texts.contains (std::wstring (L"New Casso just dropped") + s_kchEmDash + L"shiny!"));
        Assert::IsTrue (texts.contains (L"Curse your sudden but inevitable update!"));
        Assert::IsTrue (texts.contains (L"Gorram it, there's a new Casso."));
        Assert::IsTrue (texts.contains (std::wstring (L"Can't stop the signal") + s_kchEmDash + L"or the updates."));
        Assert::IsTrue (texts.contains (L"Everything's shiny, Cap'n. A new Casso's in the black."));
        Assert::AreEqual (list[2], UpdateDialogModel::PickOpener ([] (size_t) { return (size_t) 2; }));
    }



    TEST_METHOD (DeveloperNudges_EveryEntryIsASentence)
    {
        UpdateDialogModel::JudgementList  list = UpdateDialogModel::GetDeveloperNudges();



        Assert::IsTrue (list.size() >= 4);

        for (LPCWSTR nudge : list)
        {
            std::wstring_view  text = nudge;

            Assert::IsTrue (iswupper (text.front()), nudge);
            Assert::IsTrue (text.back() == L'.', nudge);
            Assert::IsTrue (text.find (L"  ") == std::wstring_view::npos, nudge);
            Assert::IsTrue (text.find (L"rebuild") != std::wstring_view::npos, nudge);
        }

        Assert::AreEqual (std::wstring (list[1]), UpdateDialogModel::PickDeveloperNudge ([] (size_t) { return (size_t) 1; }));
    }



    TEST_METHOD (StripVersionBrackets_KeepsTheRestAndTheColon)
    {
        Assert::AreEqual (std::string ("1.30.0 - 2026-10-03: The one with flux"),
                          UpdateDialogModel::StripVersionBrackets ("[1.30.0] - 2026-10-03: The one with flux"));
        Assert::AreEqual (std::string ("No brackets"), UpdateDialogModel::StripVersionBrackets ("No brackets"));
        Assert::AreEqual (std::string ("[open"),       UpdateDialogModel::StripVersionBrackets ("[open"));
    }



    TEST_METHOD (FormatNotes_HighlightsThenChangesUnderHeadings)
    {
        ReleaseNotes                notes;
        std::vector<FormattedLine>  lines;



        notes.highlights.push_back ({ { 1, 31, 0 }, "Updates", "Casso can update itself." });
        notes.changes.push_back    ({ { 1, 31, 0 }, "[1.31.0] - 2026-10-20", "- New" });
        notes.changes.push_back    ({ { 1, 30, 1 }, "[1.30.1] - 2026-10-10", "- Fix" });

        UpdateDialogModel::FormatNotes (notes, lines);

        Assert::IsTrue   (lines.size() >= 8);
        Assert::IsTrue   (lines[0].kind == FormattedLineKind::Heading);
        Assert::AreEqual (2, lines[0].headingLevel);
        Assert::AreEqual (std::string ("Updates"), lines[0].runs[0].text);
        Assert::IsTrue   (lines[1].kind == FormattedLineKind::Paragraph);
        Assert::IsTrue   (lines[2].kind == FormattedLineKind::Blank);
        Assert::AreEqual (3, lines[3].headingLevel);
        Assert::AreEqual (std::string ("1.31.0 - 2026-10-20"), lines[3].runs[0].text, L"the link brackets are dropped");
        Assert::IsTrue   (lines[4].kind == FormattedLineKind::Bullet);
        Assert::AreEqual (std::string ("1.30.1 - 2026-10-10"), lines[6].runs[0].text);
    }


    TEST_METHOD (NotesTabs_WhatsNewFirstWhenThereAreHighlights)
    {
        ReleaseNotes           notes;
        std::vector<NotesTab>  tabs;



        notes.changes.push_back ({ { 1, 31, 0 }, "[1.31.0] - 2026-10-20", "- New" });

        tabs = UpdateDialogModel::GetNotesTabs (notes);
        Assert::AreEqual ((size_t) 1, tabs.size(), L"no highlights: the changelog alone");
        Assert::IsTrue   (tabs[0] == NotesTab::Changelog);
        Assert::IsFalse  (UpdateDialogModel::ShowsTabStrip (tabs), L"and no strip for a lone tab");

        notes.highlights.push_back ({ { 1, 31, 0 }, "Updates", "Casso can update itself." });

        tabs = UpdateDialogModel::GetNotesTabs (notes);
        Assert::AreEqual ((size_t) 2, tabs.size());
        Assert::IsTrue   (tabs[0] == NotesTab::WhatsNew, L"the default, first");
        Assert::IsTrue   (tabs[1] == NotesTab::Changelog);
        Assert::IsTrue   (UpdateDialogModel::ShowsTabStrip (tabs));
        Assert::AreEqual (std::wstring (L"What's new"), std::wstring (UpdateDialogModel::GetTabLabel (NotesTab::WhatsNew)));
        Assert::AreEqual (std::wstring (L"Changelog"),  std::wstring (UpdateDialogModel::GetTabLabel (NotesTab::Changelog)));
    }



    TEST_METHOD (FormatNotesTab_EachTabHasOnlyItsOwnSections)
    {
        ReleaseNotes                notes;
        std::vector<FormattedLine>  lines;



        notes.highlights.push_back ({ { 1, 31, 0 }, "Updates", "Casso can update itself." });
        notes.changes.push_back    ({ { 1, 31, 0 }, "[1.31.0] - 2026-10-20", "- New" });

        UpdateDialogModel::FormatNotesTab (notes, NotesTab::WhatsNew, lines);
        Assert::AreEqual ((size_t) 2, lines.size());
        Assert::AreEqual (std::string ("Updates"), lines[0].runs[0].text);

        UpdateDialogModel::FormatNotesTab (notes, NotesTab::Changelog, lines);
        Assert::AreEqual ((size_t) 2, lines.size());
        Assert::AreEqual (std::string ("1.31.0 - 2026-10-20"), lines[0].runs[0].text);
        Assert::IsTrue   (lines[1].kind == FormattedLineKind::Bullet);
    }



    TEST_METHOD (DialogContent_ShowsTheStripOnlyWithBothTabs)
    {
        UpdateDialogContent  content;
        ReleaseNotes         notes;
        DxuiDpiScaler        scaler;



        content.Layout (RECT { 0, 0, 600, 500 }, scaler);
        Assert::IsFalse (content.IsTabStripShown(), L"loading: the message alone");

        notes.changes.push_back ({ { 1, 31, 0 }, "[1.31.0] - 2026-10-20", "- New" });
        content.SetNotes (notes);
        Assert::IsFalse (content.IsTabStripShown(), L"no highlights: the changelog alone");
        Assert::IsTrue  (content.GetSelectedTab() == NotesTab::Changelog);

        notes.highlights.push_back ({ { 1, 31, 0 }, "Updates", "Casso can update itself." });
        content.SetNotes (notes);
        Assert::IsTrue  (content.IsTabStripShown());
        Assert::IsTrue  (content.GetSelectedTab() == NotesTab::WhatsNew, L"What's new by default");

        content.SelectTab (NotesTab::Changelog);
        Assert::IsTrue  (content.GetSelectedTab() == NotesTab::Changelog);

        content.SetNotesMessage (UpdateDialogModel::kpszNotesMissing);
        Assert::IsFalse (content.IsTabStripShown(), L"a notice stands alone");
    }



    //  Each tab is a fixed width, so its label has to fit in what the strip
    //  leaves of it once the label is inset.
    TEST_METHOD (DialogContent_TabLabelsFitTheirTabs)
    {
        UpdateDialogContent   content;
        ReleaseNotes          notes;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        NotesTab              tabs[2] = { NotesTab::WhatsNew, NotesTab::Changelog };



        notes.highlights.push_back ({ { 1, 31, 0 }, "Updates", "Casso can update itself." });
        notes.changes.push_back    ({ { 1, 31, 0 }, "[1.31.0] - 2026-10-20", "- New" });

        content.Layout   (RECT { 0, 0, 600, 500 }, scaler);
        content.SetNotes (notes);
        content.Paint    (painter, text, theme);

        for (NotesTab tab : tabs)
        {
            std::wstring              label  = UpdateDialogModel::GetTabLabel (tab);
            float                     width  = 0.0f;
            float                     height = 0.0f;
            const RecordedTextCall  * drawn  = nullptr;
            HRESULT                   hr     = text.MeasureString (label.c_str(), DxuiTabStrip::kLabelFontDip, DxuiTabStrip::GetLabelFace(), width, height);

            Assert::AreEqual (S_OK, hr);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.kind == RecordedTextKind::DrawString && call.text == label)
                {
                    drawn = &call;
                }
            }

            Assert::IsNotNull (drawn, label.c_str());
            Assert::IsTrue    (drawn->width >= width, label.c_str());
        }
    }


    TEST_METHOD (CountWrappedLines_WrapsByWordAndBreaksAtNewlines)
    {
        auto  tenPerChar = [] (const std::wstring & text) { return 10.0f * (float) text.size(); };



        Assert::AreEqual (0, UpdateDialogModel::CountWrappedLines (L"", 100.0f, tenPerChar));
        Assert::AreEqual (1, UpdateDialogModel::CountWrappedLines (L"one two", 100.0f, tenPerChar), L"fits on one line");
        Assert::AreEqual (2, UpdateDialogModel::CountWrappedLines (L"one two three", 100.0f, tenPerChar), L"wraps before the word that overflows");
        Assert::AreEqual (2, UpdateDialogModel::CountWrappedLines (L"one\ntwo", 100.0f, tenPerChar), L"a newline starts a line");
        Assert::AreEqual (1, UpdateDialogModel::CountWrappedLines (L"supercalifragilistic", 100.0f, tenPerChar), L"a word wider than the line takes one line");
    }



    //  The notes start right under the header's own lines, plus the gap
    //  above the body: one line more when the age remark wraps it to two.
    TEST_METHOD (DialogContent_HeaderTakesOnlyItsWrappedLines)
    {
        constexpr int        kLinePx = 20;
        UpdateDialogContent  content;
        DxuiDpiScaler        scaler;
        int                  oneLine = 0;



        content.Layout (RECT { 0, 0, 600, 500 }, scaler);
        Assert::AreEqual (1, content.GetHeaderLines(), L"no reserved line for a remark that may never come");
        oneLine = content.GetNotesViewport().top;

        content.SetMeasuredHeaderLines (2);
        Assert::IsTrue   (content.SyncNotesHeight(), L"a remark that wraps the header lays it out again");
        Assert::AreEqual (2, content.GetHeaderLines());
        Assert::AreEqual (oneLine + kLinePx, (int) content.GetNotesViewport().top);

        content.SetMeasuredHeaderLines (1);
        Assert::IsTrue   (content.SyncNotesHeight());
        Assert::AreEqual (oneLine, (int) content.GetNotesViewport().top);
        Assert::IsFalse  (content.SyncNotesHeight(), L"nothing more to do");
    }
};
