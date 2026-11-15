#include "Pch.h"

#include "Devices/Disk/Inspector/Repair/DiskRepair.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/RepairDialog.h"
#include "Ui/DiskInspector/Views/FilesView.h"
#include "Ui/DiskInspector/Views/IssuesView.h"
#include "../EmuTests/FakeInspectorHost.h"
#include "../EmuTests/FileMapTestImages.h"
#include "RepairFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RepairWindowPartsTests
//
//  The verdict and Repair in the inspector's window at 100% and 150%: the
//  verdict beside the File map's "Copy map", a link to its finding on the
//  Issues tab for a Bad or Suspect volume; the verdict chip in the status
//  band; a Repair button on each finding of the Issues tab that a change
//  sets right, and "Repair disk image..." above them; and Repair's
//  confirmation with a check box for each change.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RepairWindowPartsTests)
{
public:

    static constexpr RECT  kAreaDip = { 0, 0, 900, 600 };



    struct Map
    {
        InspectorSession   session;
        FakeInspectorHost  host;
        FilesView          files  { session, host };
        DxuiDpiScaler      scaler;
        RECT               area   = {};
    };



    static void Open (Map & map, const vector<Byte> & image, UINT dpi)
    {
        Images::Analyze (image, false, map.session.analysis);

        map.session.context.hasDisk = true;
        map.session.isDiskGroup     = true;
        map.session.diskTab         = kTabFileMap;
        map.session.repairPlan      = DiskRepair::Plan (map.session.analysis, {});
        map.scaler.SetDpi (dpi);
        map.area = { map.scaler.ToPx (kAreaDip.left), map.scaler.ToPx (kAreaDip.top), map.scaler.ToPx (kAreaDip.right), map.scaler.ToPx (kAreaDip.bottom) };

        map.files.Create();
        map.files.RefreshFileList();
        map.files.Layout (map.area, map.scaler);
    }



    static void Click (IDxuiControl & control, const RECT & bounds)
    {
        DxuiMouseEvent  ev;



        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = { (bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2 };

        for (DxuiMouseEventKind kind : { DxuiMouseEventKind::Move, DxuiMouseEventKind::Down, DxuiMouseEventKind::Up })
        {
            ev.kind = kind;
            (void) control.OnMouse (ev);
        }
    }



    static int GetVerdictFinding (const DiskAnalysis & analysis)
    {
        const Finding *  finding = FileSystemCheck::GetVerdictFinding (analysis);



        return (finding != nullptr) ? static_cast<int> (finding - analysis.findings.data()) : -1;
    }



    using Images = FileMapTestImages;



    TEST_METHOD (TheVerdictIsALinkBesideCopyMap_At100And150Percent)
    {
        for (UINT dpi : { 96u, 144u })
        {
            Map    map;
            RECT   verdict = {};
            RECT   copy    = {};

            Open (map, RepairFixtures::MakeDos33Bitmap().damaged, dpi);

            verdict = map.files.GetVerdictLink().GetBounds();
            copy    = map.files.GetCopyButton().GetBounds();

            Assert::AreEqual (std::wstring (L"File system: Bad"), map.files.GetVerdictLink().GetAccessibleName());
            Assert::IsTrue   (map.files.GetVerdictLink().IsVisible() && map.files.GetVerdictLink().IsEnabled());
            Assert::IsTrue   (verdict.right <= copy.left, std::format (L"the verdict, then Copy map, at {} dpi", dpi).c_str());
            Assert::IsTrue   (verdict.left >= map.area.left && copy.right <= map.area.right, L"inside the view");
            Assert::AreEqual (verdict.top, copy.top, L"on the row of Copy map");

            Click (map.host.panel, verdict);
            Assert::AreEqual (GetVerdictFinding (map.session.analysis), map.host.shownFinding, L"the link opens the verdict's finding");
            Assert::IsTrue   (map.host.shownFinding >= 0);
        }
    }



    TEST_METHOD (ASuspectVerdictIsALinkToPossibleInconsistencies)
    {
        Map  map;



        Open (map, RepairFixtures::MakeDos33NextTrackOff().damaged, 96);

        Assert::AreEqual (std::wstring (L"File system: Possible inconsistencies"), map.files.GetVerdictLink().GetAccessibleName());
        Assert::IsTrue   (map.files.GetVerdictLink().IsVisible() && map.files.GetVerdictLink().IsEnabled());
    }



    TEST_METHOD (AnOkVerdictIsntALink)
    {
        Map  map;



        Open (map, RepairFixtures::MakeDos33(), 96);

        Assert::AreEqual (std::wstring (L"File system: OK"), map.files.GetVerdictLink().GetAccessibleName());
        Assert::IsTrue   (map.files.GetVerdictLink().IsVisible());
        Assert::IsFalse  (map.files.GetVerdictLink().IsEnabled());
    }



    TEST_METHOD (TheStatusBandHasTheVerdictAfterTheFormat)
    {
        DiskSummary          summary;
        vector<SummaryChip>  chips;



        summary.tracksWithData = 35;

        chips = InspectorText::BuildChips (summary, FileSystemVerdict::Bad);
        Assert::IsTrue   (chips.size() >= 2);
        Assert::IsTrue   (chips[1].kind == SummaryChipKind::FileSystem);
        Assert::AreEqual (std::wstring (L"File system: Bad"), chips[1].text);
        Assert::IsTrue   (chips[1].isBad);

        chips = InspectorText::BuildChips (summary, FileSystemVerdict::Suspect);
        Assert::AreEqual (std::wstring (L"Inconsistencies"), chips[1].text);
        Assert::IsFalse  (chips[1].isBad);

        chips = InspectorText::BuildChips (summary, FileSystemVerdict::None);
        Assert::AreEqual (std::wstring (L"No standard file system"), chips[1].text);
        Assert::IsFalse  (chips[1].isBad);

        summary.tracksWithData = 0;
        chips = InspectorText::BuildChips (summary, FileSystemVerdict::None);
        Assert::IsFalse  (std::any_of (chips.begin(), chips.end(), [] (const SummaryChip & chip) { return chip.kind == SummaryChipKind::FileSystem; }),
                          L"nothing recorded, no verdict");
    }



    TEST_METHOD (TheIssuesTabHasRepairOnEachFindingAChangeSetsRight_At100And150Percent)
    {
        for (UINT dpi : { 96u, 144u })
        {
            InspectorSession        session;
            IssuesView              issues  { session.context };
            DxuiDpiScaler           scaler;
            RepairPlan              plan;
            std::set<std::wstring>  chosen  = { L"none" };
            int                     buttons = 0;
            RECT                    all     = {};

            Images::Analyze (RepairFixtures::MakeDos33Bitmap().damaged, false, session.analysis);
            session.context.hasDisk = true;
            plan = DiskRepair::Plan (session.analysis, {});
            scaler.SetDpi (dpi);

            issues.SetRepairPlan (&plan);
            issues.SetOnRepair   ([&chosen] (const std::set<std::wstring> & keys) { chosen = keys; });
            issues.Refresh();
            issues.Layout ({ 0, 0, scaler.ToPx (900), scaler.ToPx (400) }, scaler);

            for (int row = 0; row < static_cast<int> (issues.GetRows().size()); row++)
            {
                RECT  rect    = issues.GetRepairRect (row);
                bool  isShown = rect.right > rect.left;

                Assert::AreEqual (!issues.GetFixingKeys (issues.GetRows()[row].finding).empty(), isShown, std::format (L"row {} at {} dpi", row, dpi).c_str());

                if (isShown && buttons++ == 0)
                {
                    Click (issues, rect);
                    Assert::IsTrue (chosen == issues.GetFixingKeys (issues.GetRows()[row].finding), L"a row's Repair checks its changes");
                }
            }

            Assert::IsTrue (buttons >= 2, L"the HELLO and unowned findings each have Repair");

            all = issues.GetRepairAllRect();
            Assert::IsTrue (all.right > all.left && all.right <= scaler.ToPx (900) && all.bottom <= scaler.ToPx (IssuesView::kFilterDip));

            Click (issues, all);
            Assert::IsTrue (chosen.empty(), L"Repair disk image... checks every change");
        }
    }



    TEST_METHOD (ShowFindingSelectsItEvenWhenItsCategoryIsHidden)
    {
        InspectorSession  session;
        IssuesView        issues  { session.context };
        DxuiDpiScaler     scaler;
        int               chosen  = -1;
        int               finding = -1;
        RECT              toggle  = {};



        Images::Analyze (RepairFixtures::MakeDos33Bitmap().damaged, false, session.analysis);
        session.context.hasDisk = true;
        scaler.SetDpi (96);
        finding = GetVerdictFinding (session.analysis);

        issues.SetOnSelect ([&chosen] (const TableRow & row) { chosen = row.finding; });
        issues.Refresh();
        issues.Layout ({ 0, 0, 900, 400 }, scaler);

        //  The toggles are 104 DIPs wide with 4 between them; each is turned
        //  off until the finding's row is gone.
        for (int c = 0; c < InspectorTables::kCategoryCount && HasRow (issues, finding); c++)
        {
            toggle = { c * 108 + 50, 14, c * 108 + 51, 15 };
            Click (issues, toggle);
        }

        Assert::IsFalse (HasRow (issues, finding), L"the category is filtered out");

        issues.ShowFinding (finding);

        Assert::AreEqual (finding, chosen);
        Assert::IsTrue   (HasRow (issues, finding), L"and the row is back");
    }



    static bool HasRow (const IssuesView & issues, int finding)
    {
        return std::any_of (issues.GetRows().begin(), issues.GetRows().end(), [finding] (const TableRow & row) { return row.finding == finding; });
    }



    TEST_METHOD (ASuspectVolumesConfirmationCautionsThatItsFindingsCanBeNormal)
    {
        DiskAnalysis  analysis;
        RepairPlan    plan;
        std::wstring  intro;



        Images::Analyze (RepairFixtures::MakeDos33NextTrackOff().damaged, false, analysis);
        plan  = DiskRepair::Plan (analysis, {});
        intro = RepairDialog::FormatIntro (L"Game.dsk", plan);

        Assert::IsTrue   (plan.before == FileSystemVerdict::Suspect);
        Assert::IsTrue   (intro.starts_with (RepairDialog::FormatIntro (L"Game.dsk")));
        Assert::IsTrue   (intro.find (L"can be normal") != std::wstring::npos, intro.c_str());
        Assert::AreEqual (5, RepairDialog::GetIntroLines (plan));
    }



    TEST_METHOD (TheConfirmationHasACheckBoxForEachChange_At100And150Percent)
    {
        DiskAnalysis  analysis;
        RepairDialog  dialog;



        Images::Analyze (RepairFixtures::MakeDos33Bitmap().damaged, false, analysis);
        dialog.Configure (nullptr, L"Game.dsk", analysis, {});

        Assert::AreEqual (std::wstring (L"Writes a repaired copy of Game.dsk in the same folder, with the changes checked below. Game.dsk isn't changed."),
                          RepairDialog::FormatIntro (L"Game.dsk"));
        Assert::AreEqual (static_cast<size_t> (2), dialog.GetFullPlan().changes.size());
        Assert::IsTrue   (dialog.GetChosen() == DiskRepair::GetKeys (dialog.GetFullPlan()), L"none chosen checks every change");

        dialog.SetChosen (L"unowned", false);
        Assert::AreEqual (static_cast<size_t> (1), dialog.GetPlan().changes.size());
        Assert::IsTrue   (dialog.GetPlan().canRepair);
        Assert::AreEqual (std::wstring(), RepairDialog::FormatStatus (dialog.GetPlan()));

        dialog.SetChosen (L"used:HELLO", false);
        Assert::IsFalse  (dialog.GetPlan().canRepair);
        Assert::AreEqual (std::wstring (L"No changes are selected."), RepairDialog::FormatStatus (dialog.GetPlan()));

        Assert::AreEqual (2, RepairDialog::GetIntroLines (dialog.GetFullPlan()), L"a Bad volume has no caution");

        for (UINT dpi : { 96u, 144u })
        {
            DxuiDpiScaler                          scaler;
            RepairDialogBody                       body;
            DxuiLabel                              intro;
            DxuiLabel                              status;
            vector<std::unique_ptr<DxuiCheckbox>>  checks;
            RECT                                   bounds = {};

            for (const RepairChange & change : dialog.GetFullPlan().changes)
            {
                checks.push_back (std::make_unique<DxuiCheckbox> (change.text));
            }

            scaler.SetDpi (dpi);
            bounds = { 0, 0, scaler.ToPx (RepairDialog::kWidthDip), scaler.ToPx (RepairDialogBody::GetHeightDip (checks.size(), 2)) };
            body.Init   (intro, checks, status, 2);
            body.Layout (bounds, scaler);

            Assert::IsTrue (intro.GetBounds().bottom <= checks.front()->GetBounds().top, std::format (L"the intro, then the changes, at {} dpi", dpi).c_str());
            Assert::IsTrue (checks.front()->GetBounds().bottom <= checks.back()->GetBounds().top, L"a row for each change");
            Assert::IsTrue (checks.back()->GetBounds().bottom <= status.GetBounds().top && status.GetBounds().bottom <= bounds.bottom, L"and the status under them");
        }
    }
};
