#include "Pch.h"

#include "Devices/Disk/Inspector/Repair/DiskRepair.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/RepairDialog.h"
#include "Ui/DiskInspector/Views/FilesView.h"
#include "../EmuTests/FakeInspectorHost.h"
#include "../EmuTests/FileMapTestImages.h"
#include "RepairFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RepairWindowPartsTests
//
//  The verdict and Repair in the inspector's window at 100% and 150%: the
//  verdict and Repair beside the File map's "Copy map", Repair enabled only
//  for a volume that can be repaired and its tip with the reason when it
//  can't, the verdict chip in the status band, and Repair's confirmation
//  with a line for each change and its check box inside the dialog.
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



    static void Click (Map & map, const RECT & bounds)
    {
        DxuiMouseEvent  ev;



        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = { (bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2 };

        for (DxuiMouseEventKind kind : { DxuiMouseEventKind::Move, DxuiMouseEventKind::Down, DxuiMouseEventKind::Up })
        {
            ev.kind = kind;
            (void) map.host.panel.OnMouse (ev);
        }
    }



    using Images = FileMapTestImages;



    TEST_METHOD (TheVerdictAndRepairAreBesideCopyMap_At100And150Percent)
    {
        for (UINT dpi : { 96u, 144u })
        {
            Map    map;
            RECT   verdict = {};
            RECT   repair  = {};
            RECT   copy    = {};

            Open (map, RepairFixtures::MakeDos33Bitmap().damaged, dpi);

            verdict = map.files.GetVerdictLabel().GetBounds();
            repair  = map.files.GetRepairButton().GetBounds();
            copy    = map.files.GetCopyButton().GetBounds();

            Assert::AreEqual (std::wstring (L"File system: Bad"), map.files.GetVerdictLabel().GetText());
            Assert::IsTrue   (map.files.GetVerdictLabel().IsVisible());
            Assert::IsTrue   (map.files.GetRepairButton().IsVisible() && map.files.GetRepairButton().IsEnabled());
            Assert::IsTrue   (verdict.right <= repair.left && repair.right <= copy.left, std::format (L"verdict, Repair, Copy map in order at {} dpi", dpi).c_str());
            Assert::IsTrue   (verdict.left >= map.area.left && copy.right <= map.area.right, L"inside the view");
            Assert::AreEqual (static_cast<long> (map.scaler.ToPx (90)), repair.right - repair.left, L"Repair's width scales");
            Assert::AreEqual (repair.top, copy.top, L"on the row of Copy map");

            Click (map, repair);
            Assert::AreEqual (1, map.host.repairs, L"Repair opens the confirmation");
        }
    }



    TEST_METHOD (RepairIsDisabledWithTheReasonForAVolumeThatCantBeRepaired)
    {
        for (UINT dpi : { 96u, 144u })
        {
            Map           map;
            RECT          repair = {};
            RECT          anchor = {};
            std::wstring  tip;

            Open (map, RepairFixtures::MakeDos33ChainOffDisk().damaged, dpi);

            repair = map.files.GetRepairButton().GetBounds();
            tip    = map.files.GetTip ({ (repair.left + repair.right) / 2, (repair.top + repair.bottom) / 2 }, anchor);

            Assert::IsTrue  (map.files.GetRepairButton().IsVisible());
            Assert::IsFalse (map.files.GetRepairButton().IsEnabled());
            Assert::IsTrue  (tip.find (L"Repair isn't available. Repairing this would change a file's data: ") != std::wstring::npos, tip.c_str());
        }
    }



    TEST_METHOD (RepairIsntShownForAnOkVolume)
    {
        Map  map;



        Open (map, RepairFixtures::MakeDos33(), 96);

        Assert::AreEqual (std::wstring (L"File system: OK"), map.files.GetVerdictLabel().GetText());
        Assert::IsTrue   (map.files.GetVerdictLabel().IsVisible());
        Assert::IsFalse  (map.files.GetRepairButton().IsVisible());
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

        chips = InspectorText::BuildChips (summary, FileSystemVerdict::None);
        Assert::AreEqual (std::wstring (L"No standard file system"), chips[1].text);
        Assert::IsFalse  (chips[1].isBad);

        summary.tracksWithData = 0;
        chips = InspectorText::BuildChips (summary, FileSystemVerdict::None);
        Assert::IsFalse  (std::any_of (chips.begin(), chips.end(), [] (const SummaryChip & chip) { return chip.kind == SummaryChipKind::FileSystem; }),
                          L"nothing recorded, no verdict");
    }



    TEST_METHOD (TheConfirmationHasALineForEachChangeAndItsCheckBox_At100And150Percent)
    {
        DiskAnalysis  analysis;
        RepairPlan    plan;



        Images::Analyze (RepairFixtures::MakeDos33Bitmap().damaged, false, analysis);
        plan = DiskRepair::Plan (analysis, {});

        Assert::AreEqual (std::wstring (L"\x2022 Mark 1 sector of HELLO as used in the bitmap\n\x2022 Mark 1 sector not in any file as free in the bitmap"),
                          RepairDialog::FormatChanges (plan));
        Assert::AreEqual (std::wstring (L"Writes a repaired copy of Game.dsk in the same folder, with these changes. Game.dsk isn't changed."), RepairDialog::FormatIntro (L"Game.dsk"));

        for (UINT dpi : { 96u, 144u })
        {
            DxuiDpiScaler     scaler;
            RepairDialogBody  body;
            DxuiLabel         intro;
            DxuiLabel         changes;
            DxuiCheckbox      noFree { L"Keep sectors not in any file marked as used" };
            RECT              bounds = {};

            scaler.SetDpi (dpi);
            bounds = { 0, 0, scaler.ToPx (RepairDialog::kWidthDip), scaler.ToPx (RepairDialogBody::GetHeightDip (plan.changes.size())) };
            body.Init   (intro, changes, noFree);
            body.Layout (bounds, scaler);

            Assert::IsTrue (noFree.GetBounds().bottom <= bounds.bottom && noFree.GetBounds().top >= changes.GetBounds().bottom, L"the check box is under the changes");
            Assert::IsTrue (changes.GetBounds().bottom - changes.GetBounds().top >= static_cast<long> (plan.changes.size()) * scaler.ToPx (RepairDialogBody::kLineDip),
                            std::format (L"a line for each change at {} dpi", dpi).c_str());
            Assert::IsTrue (intro.GetBounds().bottom <= changes.GetBounds().top);
        }
    }
};
