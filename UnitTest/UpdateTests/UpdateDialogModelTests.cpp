#include "Pch.h"

#include "Update/UpdateDialogModel.h"
#include "Core/UnicodeSymbols.h"

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
        Assert::AreEqual (std::wstring (L"Update to 1.30.0"), UpdateDialogModel::MakeUpdateLabel ({ 1, 30, 0 }));
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
};
