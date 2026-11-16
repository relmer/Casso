#include "Pch.h"

#include "Devices/Disk/Inspector/Repair/DiskRepair.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
#include "Ui/DiskInspector/Components/HostLink.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/RepairDialog.h"
#include "Ui/DiskInspector/Views/FilesView.h"
#include "Ui/DiskInspector/Views/IssuesView.h"
#include "../EmuTests/FakeInspectorHost.h"
#include "../EmuTests/FileMapTestImages.h"
#include "../Dxui/MockDxuiTextRenderer.h"
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
//  sets right, and "Repair disk image..." above them; Repair's
//  confirmation with a check box for each change; and the report after a
//  repair, as wide as its widest line.
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
            Assert::IsTrue   (map.host.panel.GetCursorForPoint (POINT { (verdict.left + verdict.right) / 2, (verdict.top + verdict.bottom) / 2 }) == IDC_HAND,
                              L"the hand cursor over the link");

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



    TEST_METHOD (TheIssuesTabHasOneRepairButtonUnderTheListAndCenteredIcons_At100And150Percent)
    {
        for (UINT dpi : { 96u, 144u })
        {
            std::unique_ptr<InspectorSession>  session = std::make_unique<InspectorSession>();
            IssuesView                         issues  { session->context };
            DxuiDpiScaler                      scaler;
            RepairPlan                         plan;
            std::set<std::wstring>             chosen  = { L"none" };
            RECT                               button  = {};
            RECT                               icon    = {};
            RECT                               cell    = {};

            Images::Analyze (RepairFixtures::MakeDos33Bitmap().damaged, false, session->analysis);
            session->context.hasDisk = true;
            plan = DiskRepair::Plan (session->analysis, {});
            scaler.SetDpi (dpi);

            issues.SetRepairPlan (&plan);
            issues.SetOnRepair   ([&chosen] (const std::set<std::wstring> & keys) { chosen = keys; });
            issues.Refresh();
            issues.Layout ({ 0, 0, scaler.ToPx (900), scaler.ToPx (400) }, scaler);

            Assert::AreEqual (InspectorTables::GetFindingColumns().size(), static_cast<size_t> (6), L"no Repair column");

            button = issues.GetRepairButton().GetBounds();
            Assert::IsTrue (issues.GetRepairButton().IsVisible(), std::format (L"Repair disk image... at {} dpi", dpi).c_str());
            Assert::IsTrue (button.bottom <= scaler.ToPx (400) && button.top >= scaler.ToPx (400) - scaler.ToPx (IssuesView::kButtonBandDip), L"under the list");
            Assert::IsTrue (std::abs ((button.left + button.right) / 2 - scaler.ToPx (900) / 2) <= 1, L"centered");
            Assert::AreEqual (static_cast<long> (scaler.ToPx (IssuesView::kButtonWideDip)), button.right - button.left, L"a normal button's size");

            icon = issues.GetSeverityRect (0);
            Assert::IsTrue (icon.right > icon.left, L"the first row has its icon");
            Assert::IsTrue (issues.GetList().GetCellRectPx (0, 0, cell));
            Assert::IsTrue (std::abs ((icon.left + icon.right) / 2 - (cell.left + cell.right) / 2) <= 1, std::format (L"the icon centered in its column at {} dpi", dpi).c_str());

            Click (issues, button);
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
        Assert::AreEqual (6, RepairDialog::GetIntroLines (plan));
    }



    TEST_METHOD (TheConfirmationHasACheckBoxForEachChange_At100And150Percent)
    {
        DiskAnalysis  analysis;
        RepairDialog  dialog;



        Images::Analyze (RepairFixtures::MakeDos33Bitmap().damaged, false, analysis);
        dialog.Configure (nullptr, L"Game.dsk", analysis, {});

        Assert::AreEqual (std::wstring (L"Repairing an image writes a repaired copy of Game.dsk in the same folder. Game.dsk isn't changed. "
                                        L"Select the repairs below to apply to the repaired image."),
                          RepairDialog::FormatIntro (L"Game.dsk"));
        Assert::AreEqual (static_cast<size_t> (2), dialog.GetFullPlan().changes.size());

        //  Each change has the problem it sets right, and its repair under it.
        Assert::AreEqual (std::wstring (L"Track 18, sector $E is used by HELLO but marked free"), dialog.GetFullPlan().changes[0].problem);
        Assert::AreEqual (std::wstring (L"Repair: Mark 1 sector of HELLO as used in the bitmap"), RepairDialog::FormatRepair (dialog.GetFullPlan().changes[0]));
        Assert::IsFalse  (dialog.GetFullPlan().changes[1].problem.empty());
        Assert::IsTrue   (dialog.GetChosen() == DiskRepair::GetKeys (dialog.GetFullPlan()), L"none chosen checks every change");

        dialog.SetChosen (L"unowned", false);
        Assert::AreEqual (static_cast<size_t> (1), dialog.GetPlan().changes.size());
        Assert::IsTrue   (dialog.GetPlan().canRepair);
        Assert::AreEqual (std::wstring(), RepairDialog::FormatStatus (dialog.GetPlan()));

        dialog.SetChosen (L"used:HELLO", false);
        Assert::IsFalse  (dialog.GetPlan().canRepair);
        Assert::AreEqual (std::wstring (L"No changes are selected."), RepairDialog::FormatStatus (dialog.GetPlan()));

        Assert::AreEqual (3, RepairDialog::GetIntroLines (dialog.GetFullPlan()), L"a Bad volume has no caution");

        for (UINT dpi : { 96u, 144u })
        {
            DxuiDpiScaler                          scaler;
            RepairDialogBody                       body;
            DxuiLabel                              intro;
            DxuiLabel                              status;
            vector<std::unique_ptr<DxuiCheckbox>>  checks;
            vector<std::unique_ptr<DxuiLabel>>     repairs;
            RECT                                   bounds = {};

            for (const RepairChange & change : dialog.GetFullPlan().changes)
            {
                checks.push_back  (std::make_unique<DxuiCheckbox> (change.problem));
                repairs.push_back (std::make_unique<DxuiLabel> (RepairDialog::FormatRepair (change)));
            }

            scaler.SetDpi (dpi);
            bounds = { 0, 0, scaler.ToPx (RepairDialog::kWidthDip), scaler.ToPx (RepairDialogBody::GetHeightDip (checks.size(), 3)) };
            body.Init   (intro, checks, repairs, status, 3);
            body.Layout (bounds, scaler);

            Assert::IsTrue (intro.GetBounds().bottom <= checks.front()->GetBounds().top, std::format (L"the intro, then the changes, at {} dpi", dpi).c_str());
            Assert::IsTrue (checks.front()->GetBounds().bottom <= repairs.front()->GetBounds().top, L"the repair under its problem");
            Assert::IsTrue (repairs.front()->GetBounds().bottom < checks.back()->GetBounds().top, L"a gap before the next change");
            Assert::IsTrue (repairs.back()->GetBounds().bottom <= status.GetBounds().top && status.GetBounds().bottom <= bounds.bottom, L"and the status under them");
        }
    }



    TEST_METHOD (TheReportFitsItsWidestLineAndWrapsAFixAfterItsMark_At100And150Percent)
    {
        DiskAnalysis  analysis;
        RepairPlan    plan;



        Images::Analyze (RepairFixtures::MakeDos33Bitmap().damaged, false, analysis);
        plan = DiskRepair::Plan (analysis, {});

        Assert::AreEqual (std::wstring (L"Applied these fixes:"), RepairReportDialog::FormatHeading());
        Assert::AreEqual (std::wstring (L"Press OK to load the repaired disk image into Disk Inspector."), RepairReportDialog::FormatFooter (true, L"Game (repaired).dsk"));
        Assert::AreEqual (std::wstring (L"The repaired copy is \"Game (repaired).dsk\"."), RepairReportDialog::FormatFooter (false, L"Game (repaired).dsk"));

        for (UINT dpi : { 96u, 144u })
        {
            MockDxuiTextRenderer  text;
            DxuiTheme             theme   = DxuiTheme::Dark();
            DxuiDpiScaler         scaler;
            RepairReportBody      fitted;
            RepairReportBody      wrapped;
            vector<std::wstring>  fixes;
            std::wstring          footer  = RepairReportDialog::FormatFooter (true, L"Game.dsk");
            int                   maxPx   = 0;
            int                   widthPx = 0;
            int                   height  = 0;
            RECT                  bounds  = {};

            scaler.SetDpi (dpi);
            maxPx  = scaler.ToPx (RepairReportDialog::kMaxWidthDip);
            bounds = { 0, 0, maxPx, scaler.ToPx (400) };

            //  Short lines narrow the dialog to the widest of them.
            for (const RepairChange & change : plan.changes)
            {
                fixes.push_back (change.text);
            }

            fitted.Init   (RepairReportDialog::FormatHeading(), fixes, footer);
            fitted.Layout (bounds, scaler);

            Assert::IsTrue (fitted.TryMeasureWidthPx (text, theme, maxPx, widthPx));
            Assert::IsTrue (widthPx < maxPx && widthPx >= scaler.ToPx (RepairReportBody::kMinDip), std::format (L"{} px of {} at {} dpi", widthPx, maxPx, dpi).c_str());
            Assert::AreEqual (plan.changes.size() + 2, fitted.GetRowCount(), L"the heading, a row for each fix, and the bottom line");
            Assert::IsNull    (fitted.GetMark (0), L"the heading has no mark");
            Assert::IsNotNull (fitted.GetMark (1), L"a fix has its mark");

            //  A fix longer than the widest dialog wraps after its mark.
            fixes.push_back (std::wstring());

            for (int i = 0; i < 60; i++)
            {
                fixes.back() += L"word ";
            }

            wrapped.Init   (RepairReportDialog::FormatHeading(), fixes, footer);
            wrapped.Layout (bounds, scaler);

            Assert::IsTrue   (wrapped.TryMeasureWidthPx (text, theme, maxPx, widthPx));
            Assert::AreEqual (maxPx, widthPx, L"no wider than the most");
            Assert::IsTrue   (wrapped.TryMeasureHeightPx (text, theme, widthPx, height));

            wrapped.Layout (RECT { 0, 0, widthPx, height }, scaler);

            const DxuiLabel  & longFix = wrapped.GetLabel (fixes.size());
            const DxuiLabel  & mark    = *wrapped.GetMark (fixes.size());

            Assert::AreEqual (static_cast<long> (scaler.ToPx (RepairReportBody::kMarkDip)), longFix.GetBounds().left, L"the text starts after the marks' column");
            Assert::AreEqual (mark.GetBounds().right, longFix.GetBounds().left);
            Assert::AreEqual (mark.GetBounds().top, longFix.GetBounds().top, L"the mark on the first line");
            Assert::IsTrue   (longFix.GetBounds().bottom - longFix.GetBounds().top > scaler.ToPx (RepairReportBody::kLineDip), L"on more than one line");
            Assert::IsTrue   (longFix.GetBounds().bottom < wrapped.GetLabel (fixes.size() + 1).GetBounds().top, L"the bottom line under it");
            Assert::IsTrue   (wrapped.GetLabel (fixes.size() + 1).GetBounds().bottom <= height, L"inside the measured height");
        }
    }



    TEST_METHOD (OpeningADiskClearsTheSelectionFromAnErrorFinding)
    {
        InspectorSession   session;
        FakeInspectorHost  host;
        HostLink           link { session, host };



        session.context.errorQuarterTrack = 72;
        session.context.errorSector       = 14;
        session.context.errorFirstNibble  = 300;
        session.context.errorNibbleCount  = 400;

        link.StartDisk (nullptr, L"");

        Assert::AreEqual (-1, session.context.errorQuarterTrack, L"a repaired copy opened in the image's place isn't drawn in the error color");
        Assert::AreEqual (-1, session.context.errorSector);
        Assert::IsFalse  (session.context.IsErrorSelected());
    }



    TEST_METHOD (TheIssuesColumnsResizeByTheirHeadersDividers_At100And150Percent)
    {
        for (UINT dpi : { 96u, 144u })
        {
            InspectorSession  session;
            IssuesView        issues  { session.context };
            DxuiDpiScaler     scaler;
            DxuiMouseEvent    ev;
            RECT              before  = {};
            RECT              after   = {};
            POINT             divider = {};

            Images::Analyze (RepairFixtures::MakeDos33Bitmap().damaged, false, session.analysis);
            session.context.hasDisk = true;
            scaler.SetDpi (dpi);

            issues.Refresh();
            issues.Layout ({ 0, 0, scaler.ToPx (900), scaler.ToPx (400) }, scaler);

            //  The divider at the Category column's right edge, in the header.
            Assert::IsTrue (issues.GetList().GetCellRectPx (0, 2, before));
            divider = { issues.GetList().GetBounds().left + before.right - 1, issues.GetList().GetBounds().top + issues.GetList().GetHeaderHeightPx() / 2 };

            Assert::IsTrue (issues.GetCursorForPoint (divider) == IDC_SIZEWE, std::format (L"the resize arrows over the divider at {} dpi", dpi).c_str());

            ev.button = DxuiMouseButton::Left;

            for (std::pair<DxuiMouseEventKind, int> step : { std::pair { DxuiMouseEventKind::Move, 0 }, { DxuiMouseEventKind::Down, 0 }, { DxuiMouseEventKind::Move, 40 }, { DxuiMouseEventKind::Up, 40 } })
            {
                ev.kind        = step.first;
                ev.positionDip = { divider.x + scaler.ToPx (step.second), divider.y };
                (void) issues.OnMouse (ev);
            }

            Assert::IsTrue   (issues.GetList().GetCellRectPx (0, 2, after));
            Assert::AreEqual (static_cast<long> (scaler.ToPx (40)), (after.right - after.left) - (before.right - before.left), L"the column is as much wider as the drag");
        }
    }
};
