#include "Pch.h"

#include "ExplorerWindowPartsRig.h"
#include "CassoExplorer/Window/Panes/ExplorerPickerBar.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Managers/ExplorerPickerMode.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerWindowTests
//
//  The window's parts in picker mode: the commands a picker has, what it
//  lists and confirms, its bar, its stock disks and its save form.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExplorerPickerWindowTests)
{
public:

    using Rig = ExplorerWindowPartsRig;




    //  A picker's window runs only the commands on the picker's list, by
    //  whatever route: a file command is disabled with a file selected, and
    //  dispatching one does nothing.
    TEST_METHOD (Picker_BlocksCommandsOffItsList)
    {
        Rig            rig;
        PickerRequest  request;
        size_t         tabs = 0;



        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        rig.Open();
        rig.browser.NavigateToAddress (L"C:\\Disks");
        rig.browser.SetSelectedRows ({ 0 });
        tabs = rig.browser.GetBrowserModel().GetTabCount();

        Assert::IsTrue (rig.parts->commandManager.IsEnabled (CassoExplorerCommands::kDeleteItems), L"outside a picker, Delete is enabled for the selection");

        rig.app.picker = &request;

        for (int id : CassoExplorerCommands::GetAllIds())
        {
            if (!CassoExplorerCommands::IsAllowedInPicker (id, false))
            {
                Assert::IsFalse (rig.parts->commandManager.IsEnabled (id), std::format (L"command {}", id).c_str());
            }
        }

        rig.parts->commandManager.Dispatch (CassoExplorerCommands::kNewTab);
        Assert::AreEqual (tabs, rig.browser.GetBrowserModel().GetTabCount(), L"New tab did nothing");

        rig.parts->commandManager.Dispatch (CassoExplorerCommands::kUp);
        Assert::IsFalse (rig.browser.GetLocation() == Location::MakeHostFolder (L"C:\\Disks"), L"Up is on the list and ran");
    }

    //  A picker for a Disk II: only folders and the images the drive takes
    //  are listed, the confirm button follows the selection, and a confirm
    //  puts the file's path in the outcome and closes the window.
    TEST_METHOD (Picker_ListsItsScope_AndConfirmsTheSelectedFile)
    {
        Rig            rig;
        PickerRequest  request;
        PickerOutcome  outcome;
        int            image = -1;



        request.form           = PickerForm::ModalOpen;
        request.filters        = { { L"Disk images", L"*.dsk;*.po" }, { L"All files", L"*.*" } };
        request.receivingDrive = ReceivingDrive { ReceivingDriveKind::Floppy525, 0 };
        request.driveLabel     = L"Disk II, slot 6 drive 1";

        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        AssertSucceeded (rig.fs.WriteAllText (L"C:\\Disks\\readme.txt", "hello"));
        AssertSucceeded (rig.fs.WriteAllText (L"C:\\Disks\\Games\\note.txt", "x"));

        rig.app.picker        = &request;
        rig.app.outcome = &outcome;
        rig.Open();
        rig.browser.NavigateToAddress (L"C:\\Disks");
        rig.parts->listPane.FillList();

        Assert::AreEqual ((size_t) 2, rig.browser.GetRows().size(), L"the folder and the image, without the text file");
        Assert::AreEqual (std::wstring (L"Disk II, slot 6 drive 1: 5.25-inch disk images"), rig.parts->pickerMode.GetHeaderText());
        Assert::AreEqual (std::wstring (L"Open"), rig.parts->pickerMode.GetConfirmLabel());
        Assert::IsFalse  (rig.parts->pickerBar.IsConfirmEnabled(), L"nothing is selected");

        for (int row = 0; row < (int) rig.browser.GetRows().size(); row++)
        {
            image = rig.browser.GetRows()[(size_t) row].isDirectory ? image : row;
        }

        rig.browser.SetSelectedRows ({ 1 - image });
        rig.parts->statusBar.Fill();
        Assert::IsFalse (rig.parts->pickerBar.IsConfirmEnabled(), L"a folder is selected");

        rig.browser.SetSelectedRows ({ image });
        rig.parts->statusBar.Fill();
        Assert::IsTrue (rig.parts->pickerBar.IsConfirmEnabled(), L"the image is selected");

        rig.parts->pickerMode.Confirm();

        Assert::IsTrue   (outcome.isConfirmed);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33.dsk"), outcome.path);
        Assert::IsTrue   (rig.window.closed);
    }


    //  Esc closes a picker with nothing confirmed, and a double-click on a
    //  file confirms it without opening the image.
    TEST_METHOD (Picker_EscCancels_AndOpeningAFileConfirmsIt)
    {
        Rig            rig;
        PickerRequest  request;
        PickerOutcome  outcome;
        DxuiKeyEvent   escape;
        std::wstring   inserted;



        request.form      = PickerForm::DriveWindow;
        request.onConfirm = [&inserted] (const std::wstring & path) { inserted = path; return true; };

        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        rig.app.picker        = &request;
        rig.app.outcome = &outcome;
        rig.Open();
        rig.browser.NavigateToAddress (L"C:\\Disks");

        escape.kind = DxuiKeyEventKind::Down;
        escape.vk   = VK_ESCAPE;

        Assert::IsTrue  (rig.parts->pickerMode.HandleKey (escape));
        Assert::IsTrue  (rig.window.closed);
        Assert::IsFalse (outcome.isConfirmed);

        rig.window.closed = false;

        Assert::IsTrue   (rig.parts->pickerMode.HandleActivateRow (0));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33.dsk"), inserted, L"the request inserted the image");
        Assert::IsTrue   (rig.browser.GetLocation() == Location::MakeHostFolder (L"C:\\Disks"), L"the image was not opened as a folder");
        Assert::IsTrue   (outcome.isConfirmed);
        Assert::IsTrue   (rig.window.closed);
        Assert::AreEqual (std::wstring (L"Insert"), rig.parts->pickerMode.GetConfirmLabel());
    }


    //  A drive's picker stays open when the request does not take the image.
    TEST_METHOD (Picker_StaysOpenWhenTheInsertIsNotTaken)
    {
        Rig            rig;
        PickerRequest  request;
        PickerOutcome  outcome;



        request.form      = PickerForm::DriveWindow;
        request.onConfirm = [] (const std::wstring &) { return false; };

        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        rig.app.picker        = &request;
        rig.app.outcome = &outcome;
        rig.Open();
        rig.browser.NavigateToAddress (L"C:\\Disks");

        Assert::IsTrue  (rig.parts->pickerMode.HandleActivateRow (0));
        Assert::IsFalse (outcome.isConfirmed);
        Assert::IsFalse (rig.window.closed);
    }


    //  A picker has no tab strip, and its bar above the status bar; a window
    //  that is not a picker has no such bar.
    TEST_METHOD (Picker_HasItsBarAndNoTabStrip)
    {
        Rig            plain;
        Rig            rig;
        PickerRequest  request;
        RECT           band = {};



        plain.Open();
        band = plain.parts->layout.GetPickerBand().GetBounds();
        Assert::AreEqual (band.top, band.bottom, L"no picker bar in a file manager window");

        rig.app.picker = &request;
        rig.Open();

        band = rig.parts->layout.GetPickerBand().GetBounds();
        Assert::AreEqual ((LONG) rig.scaler.ToPx (ExplorerPickerBar::kBandDip), band.bottom - band.top);
        Assert::AreEqual (rig.parts->layout.GetStatusBand().GetBounds().top, band.bottom, L"above the status bar");

        band = rig.parts->layout.GetTabBand().GetBounds();
        Assert::AreEqual (band.top, band.bottom, L"no tab strip");
    }


    //  Choosing another filter entry lists the folder by its masks.
    TEST_METHOD (Picker_SwitchingTheFilterListsTheFolderAgain)
    {
        Rig            rig;
        PickerRequest  request;



        request.filters = { { L"ProDOS images", L"*.po" }, { L"DOS images", L"*.dsk" } };

        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        rig.Seed ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");
        rig.app.picker = &request;
        rig.Open();
        rig.browser.NavigateToAddress (L"C:\\Disks");

        Assert::AreEqual ((size_t) 1, rig.browser.GetRows().size());
        Assert::AreEqual (std::wstring (L"prodos.po"), rig.browser.GetRows()[0].name);
        Assert::AreEqual (std::wstring (L"ProDOS images (*.po)"), rig.parts->pickerMode.GetFilterLabel (0));

        rig.parts->pickerMode.SetActiveFilter (1);

        Assert::AreEqual ((size_t) 1, rig.browser.GetRows().size());
        Assert::AreEqual (std::wstring (L"dos33.dsk"), rig.browser.GetRows()[0].name);
    }

    //  The count field has how many files the scope left out, and an empty
    //  Casso node's list has where to browse from.
    TEST_METHOD (Picker_StatusCountsTheFilesItsScopeLeftOut)
    {
        Rig            rig;
        PickerRequest  request;



        Assert::AreEqual (std::wstring (L"3 items"),                              ExplorerStatusBar::DescribePickerCount (L"3 items", 3, 0, true, false));
        Assert::AreEqual (std::wstring (L"3 items (12 more hidden because they don't match this drive)"),  ExplorerStatusBar::DescribePickerCount (L"3 items", 3, 12, true, false));
        Assert::AreEqual (std::wstring (L"3 items (2 more hidden because they don't match the filter)"), ExplorerStatusBar::DescribePickerCount (L"3 items", 3, 2, false, false));
        Assert::AreEqual (std::wstring (L"No recent folders. Browse from This PC."), ExplorerStatusBar::DescribePickerCount (L"0 items", 0, 0, true, true));
        Assert::AreEqual (std::wstring (L"0 items (4 more hidden because they don't match this drive)"),   ExplorerStatusBar::DescribePickerCount (L"0 items", 0, 4, true, true));

        request.filters = { { L"ProDOS images", L"*.po" } };

        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        rig.Seed ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");
        rig.app.picker = &request;
        rig.Open();
        rig.browser.NavigateToAddress (L"C:\\Disks");

        Assert::AreEqual ((size_t) 1, rig.browser.GetScopedOutCount());
    }

    //  An image in one of the running machine's drives is marked with the
    //  drive, whatever the case of its path.
    TEST_METHOD (Picker_MarksTheImageInADrive)
    {
        Rig            rig;
        PickerRequest  request;



        request.mounted = { { L"c:\\disks\\PRODOS.PO", L"slot 6 drive 1" } };

        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        rig.Seed ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");
        rig.app.picker = &request;
        rig.Open();
        rig.browser.NavigateToAddress (L"C:\\Disks");

        Assert::AreEqual ((size_t) 2, rig.browser.GetRows().size());

        for (const CatalogRow & row : rig.browser.GetRows())
        {
            Assert::AreEqual (std::wstring (row.name == L"prodos.po" ? L"slot 6 drive 1" : L""), row.inDrive, row.name.c_str());
        }
    }

    //  A disk drive's picker creates through the New disk image dialog; the
    //  recorder's only through its request's flow; a request without a
    //  drive, and a window that is no picker, not at all.
    TEST_METHOD (Picker_CreatesNewForADrive)
    {
        Rig            plain;
        Rig            forDisk;
        Rig            forTape;
        Rig            forTapeWithFlow;
        Rig            forFile;
        PickerRequest  disk;
        PickerRequest  tape;
        PickerRequest  tapeWithFlow;
        PickerRequest  file;



        disk.form                   = PickerForm::DriveWindow;
        disk.receivingDrive         = ReceivingDrive { ReceivingDriveKind::Apple35, 0 };
        tape.form                   = PickerForm::DriveWindow;
        tape.receivingDrive         = ReceivingDrive { ReceivingDriveKind::Recorder, 0 };
        tapeWithFlow                = tape;
        tapeWithFlow.onCreateNew    = [] (std::wstring & outPath) { outPath.clear(); return true; };
        file.form                   = PickerForm::ModalOpen;

        forDisk.app.picker         = &disk;
        forTape.app.picker         = &tape;
        forTapeWithFlow.app.picker = &tapeWithFlow;
        forFile.app.picker         = &file;

        plain.Open();
        forDisk.Open();
        forTape.Open();
        forTapeWithFlow.Open();
        forFile.Open();

        Assert::IsFalse (plain.parts->pickerMode.CanCreateNew());
        Assert::IsTrue  (forDisk.parts->pickerMode.CanCreateNew());
        Assert::IsFalse (forTape.parts->pickerMode.CanCreateNew());
        Assert::IsTrue  (forTapeWithFlow.parts->pickerMode.CanCreateNew());
        Assert::IsFalse (forFile.parts->pickerMode.CanCreateNew());

        //  The flow inserted what it made, so the picker closes.
        forTapeWithFlow.parts->pickerMode.CreateNew();
        Assert::IsTrue (forTapeWithFlow.window.closed);
    }

    //  The "Stock disks" node is the Casso node's first child in a picker
    //  with stock disks. A stock disk not on the disk yet is downloaded by
    //  the request before it is confirmed, and a download that fails
    //  confirms nothing.
    TEST_METHOD (Picker_StockDisk_IsDownloadedBeforeItIsConfirmed)
    {
        Rig                        rig;
        Rig                        plain;
        PickerRequest              request;
        std::vector<TreeNode>      nodes;
        std::vector<std::wstring>  log;
        bool                       fetchResult = false;



        request.form           = PickerForm::DriveWindow;
        request.receivingDrive = ReceivingDrive { ReceivingDriveKind::Floppy525, 0 };
        request.stockDisks     = { { L"ProDOS Users Disk", L"C:\\Stock\\ProDOS Users Disk.dsk", false } };
        request.onFetchStock   = [&] (const std::wstring & path) { log.push_back (L"fetch " + path); return fetchResult; };
        request.onConfirm      = [&] (const std::wstring & path) { log.push_back (L"insert " + path); return true; };

        plain.Open();
        AssertSucceeded (plain.browser.GetTreeModel().GetChildren (TreeModel::kCassoRootId, nodes));
        Assert::IsTrue (nodes.empty() || nodes[0].id != TreeModel::kStockDisksId);

        rig.app.picker = &request;
        rig.Open();

        AssertSucceeded (rig.browser.GetTreeModel().GetChildren (TreeModel::kCassoRootId, nodes));
        Assert::IsFalse (nodes.empty());
        Assert::AreEqual (std::wstring (TreeModel::kStockDisksId), nodes[0].id);
        Assert::AreEqual (std::wstring (L"Stock disks"), nodes[0].label);

        rig.browser.NavigateToLocation (Location::MakeRoot (TreeModel::kStockDisksId));
        Assert::AreEqual ((size_t) 1, rig.browser.GetRows().size());

        rig.browser.SetSelectedRows ({ 0 });
        Assert::IsTrue (rig.parts->pickerMode.CanConfirm());

        rig.parts->pickerMode.Confirm();
        Assert::IsTrue (log == std::vector<std::wstring> { L"fetch C:\\Stock\\ProDOS Users Disk.dsk" });
        Assert::IsFalse (rig.window.closed);

        fetchResult = true;
        rig.parts->pickerMode.Confirm();
        Assert::AreEqual (std::wstring (L"insert C:\\Stock\\ProDOS Users Disk.dsk"), log.back());
        Assert::IsTrue (rig.window.closed);
    }

    //  A save form returns the name in its box, in the folder listed. A file
    //  already there is replaced only on a yes.
    TEST_METHOD (PickerSaveForm_ReturnsTheTypedName_AndAsksBeforeReplacing)
    {
        Rig            rig;
        PickerRequest  request;
        PickerOutcome  outcome;
        int            asked  = 0;
        bool           answer = false;



        request.form            = PickerForm::ModalSave;
        request.filters         = { { L"Disk images", L"*.dsk" } };
        request.defaultFileName = L"New disk.dsk";

        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        rig.app.picker  = &request;
        rig.app.outcome = &outcome;
        rig.Open();
        rig.parts->pickerMode.SetAskToReplace ([&] (const std::wstring &) { asked++; return answer; });

        Assert::AreEqual (std::wstring (L"New disk.dsk"), rig.parts->pickerBar.GetNameText());
        Assert::AreEqual (std::wstring (L"Save"), rig.parts->pickerMode.GetConfirmLabel());

        rig.browser.NavigateToAddress (L"C:\\Disks");
        Assert::IsTrue (rig.parts->pickerMode.CanConfirm());

        //  Selecting a file puts its name in the box.
        rig.browser.SetSelectedRows ({ 0 });
        rig.parts->statusBar.Fill();
        Assert::AreEqual (std::wstring (L"dos33.dsk"), rig.parts->pickerBar.GetNameText());

        rig.parts->pickerMode.Confirm();
        Assert::AreEqual (1, asked);
        Assert::IsFalse (rig.window.closed, L"No keeps the picker open");

        answer = true;
        rig.parts->pickerMode.Confirm();
        Assert::AreEqual (2, asked);
        Assert::IsTrue (rig.window.closed);
        Assert::IsTrue (outcome.isConfirmed);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33.dsk"), outcome.path);
    }



    //  A name that is not taken is returned without a question, with the
    //  filter's extension when none was typed. The Casso node's list is no
    //  folder to save in. New folder is a save form's command and no other
    //  picker's.
    TEST_METHOD (PickerSaveForm_NewName_NeedsAFolder_AndHasNewFolder)
    {
        Rig            rig;
        Rig            openForm;
        PickerRequest  request;
        PickerRequest  open;
        PickerOutcome  outcome;
        int            asked = 0;



        request.form    = PickerForm::ModalSave;
        request.filters = { { L"Disk images", L"*.dsk" } };
        open.form       = PickerForm::ModalOpen;

        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        rig.app.picker  = &request;
        rig.app.outcome = &outcome;
        rig.Open();
        rig.parts->pickerMode.SetAskToReplace ([&] (const std::wstring &) { asked++; return false; });

        openForm.app.picker = &open;
        openForm.Open();

        rig.parts->pickerBar.SetNameText (L"blank");

        rig.browser.NavigateToLocation (Location::MakeRoot (TreeModel::kCassoRootId));
        Assert::IsFalse (rig.parts->pickerMode.CanConfirm());

        rig.browser.NavigateToAddress (L"C:\\Disks");
        Assert::IsTrue (rig.parts->pickerMode.CanConfirm());

        Assert::IsTrue  (CassoExplorerCommands::IsAllowedInPicker (CassoExplorerCommands::kNewFolder, true));
        Assert::IsFalse (CassoExplorerCommands::IsAllowedInPicker (CassoExplorerCommands::kNewFolder, false));

        rig.parts->pickerMode.Confirm();
        Assert::AreEqual (0, asked);
        Assert::IsTrue (rig.window.closed);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\blank.dsk"), outcome.path);
    }
};
