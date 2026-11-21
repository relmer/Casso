#include "Pch.h"
#include "ExplorerWindowPartsRig.h"
#include "../EmuTests/InfocomSaveBuilder.h"
#include "../FileTypes/WizardryScenarioBuilder.h"
#include "Machines/Apple2/Common/PascalVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SavedGamePaneTests
//
//  The switch between a disk's saved game and the disk in the preview pane,
//  run through ExplorerWindowParts with no window, over disks built in
//  memory on the rig's in-memory host. One rig is opened for every case, as
//  opening it is the slow part.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SavedGamePaneTests)
{
public:

    using Rig = ExplorerWindowPartsRig;

    static constexpr const wchar_t *  kDisks     = L"C:\\Disks";
    static constexpr size_t           kDiskBytes = 143360;


    //  A blank Pascal disk with one data file on it.
    static std::vector<Byte> MakePascalDisk (const char * name)
    {
        std::vector<Byte>  disk (kDiskBytes, 0);
        std::vector<Byte>  next;
        FilePayload        payload;



        PascalVolume::WriteBlank ("WIZ", PascalVolume::MakeDate (1981, 9, 1), disk);

        payload.bytes = WizardryScenarioBuilder::MakeScenario();
        payload.type  = PascalVolume::kKindData;

        AssertSucceeded (PascalVolume (disk).Write (FilePath::Parse (name), payload, next));

        return next;
    }


    static void Select (Rig & rig, const wchar_t * name)
    {
        Assert::IsTrue (rig.parts->listPane.SelectRowNamed (name), name);
        rig.parts->preview.FillPreview();
        rig.parts->layout.RecomputeLayout();
    }


    TEST_METHOD (SavedGameSwitch_ForEachKindOfDisk)
    {
        Rig    rig;
        RECT   disk = {};



        rig.prefs.previewVisible = true;
        rig.SeedBytes (MakePascalDisk ("SCENARIO.DATA"), L"C:\\Disks\\scenario.dsk");
        rig.SeedBytes (MakePascalDisk ("NOTES.DATA"),    L"C:\\Disks\\notes.dsk");
        rig.SeedBytes (InfocomSaveBuilder::MakeDisk (InfocomSaveBuilder::MakeMemory (0), 2), L"C:\\Disks\\zork-save.dsk");
        rig.browser.GetTreeModel().SetKnownFolders ({ kDisks });
        rig.browser.GetTreeModel().SetDrives ({});
        rig.browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring & path) { return _wcsicmp (path.c_str(), kDisks) == 0; });
        rig.Open();
        rig.browser.NavigateToLocation (Location::MakeHostFolder (kDisks));
        rig.parts->listPane.FillList();

        //  A Wizardry scenario disk: the switch on Saved game, the game under
        //  it, and no thumbnail.
        Select (rig, L"scenario.dsk");

        Assert::IsTrue   (rig.parts->preview.GetSections().IsVisible(), L"The switch is shown");
        Assert::AreEqual (0, rig.parts->preview.GetSections().GetSelected(), L"on Saved game");
        Assert::IsFalse  (rig.parts->preview.GetThumbnail().IsShown(), L"with no thumbnail");
        Assert::IsTrue   (rig.parts->preview.GetContentRect().top >= rig.parts->preview.GetSections().GetBounds().bottom, L"and the game below the switch");
        Assert::IsTrue   (rig.browser.GetPreview().kind == PreviewContent::Kind::Document);

        //  A click on Disk: the catalog, and the switch above it.
        disk      = rig.parts->preview.GetSections().GetBounds();
        disk.left = (disk.left + disk.right) / 2;
        rig.Click (disk);

        Assert::AreEqual (1, rig.parts->preview.GetSections().GetSelected(), L"Disk is selected");
        Assert::IsTrue   (rig.browser.GetPreview().kind == PreviewContent::Kind::Catalog, L"and the disk's catalog shown");
        Assert::IsTrue   (rig.parts->preview.GetSections().IsVisible(), L"and the switch above it");

        //  Disk is selected for the next disk with a saved game too: the Infocom
        //  save disk's details.
        Select (rig, L"zork-save.dsk");

        Assert::IsTrue   (rig.parts->preview.GetSections().IsVisible(), L"An Infocom save disk has the switch");
        Assert::AreEqual (1, rig.parts->preview.GetSections().GetSelected());
        Assert::IsTrue   (rig.browser.GetPreview().kind == PreviewContent::Kind::Details);

        //  A disk with no saved game has no switch.
        Select (rig, L"notes.dsk");

        Assert::IsFalse  (rig.parts->preview.GetSections().IsVisible(), L"No switch without a saved game");
    }
};
