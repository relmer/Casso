#include "Pch.h"

#include "ExplorerWindowPartsRig.h"
#include "CassoExplorer/Window/Managers/ExplorerPickerMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerPickerMode.h"
#include "CassoExplorer/Window/Managers/ExplorerListMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "CassoExplorer/Window/Managers/ExplorerRename.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMenusTests
//
//  A picker's context menus: the file dialog's clipboard, rename, delete,
//  New folder and Properties commands, and none of the main window's
//  commands that open a program, change an image or start Casso.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExplorerPickerMenusTests)
{
public:

    using Rig  = ExplorerWindowPartsRig;
    using Verb = CassoExplorerActions::Verb;




    //  GetPickerVerbs returns the dialog's verbs alone, in their original order.
    TEST_METHOD (GetPickerVerbs_KeepsOnlyTheDialogVerbs)
    {
        std::vector<Verb>  verbs       = { Verb::Open, Verb::Cut, Verb::Boot, Verb::Copy, Verb::OpenWith, Verb::Rename, Verb::Delete,
                                           Verb::InsertDrive1, Verb::NewDisk, Verb::NewFolder, Verb::Paste, Verb::Share, Verb::Refresh };
        std::vector<Verb>  pickerVerbs = ExplorerPickerMenus::GetPickerVerbs (verbs);
        std::vector<Verb>  expected    = { Verb::Cut, Verb::Copy, Verb::Rename, Verb::Delete, Verb::NewFolder, Verb::Paste, Verb::Refresh };



        Assert::IsTrue (pickerVerbs == expected);
    }


    //  A selected image's menu has the confirm, the icon row, Copy as path and
    //  Properties, and nothing else.
    TEST_METHOD (FileMenu_HasTheDialogRowsOnly)
    {
        Rig                        rig;
        PickerRequest              request;
        std::vector<std::wstring>  labels;
        std::vector<std::wstring>  allowed = { L"Insert", ExplorerMenus::GetVerbLabel (Verb::Cut), ExplorerMenus::GetVerbLabel (Verb::Copy),
                                              ExplorerMenus::GetVerbLabel (Verb::Rename), ExplorerMenus::GetVerbLabel (Verb::Delete),
                                              L"Copy as &path", L"P&roperties" };



        request.form = PickerForm::DriveWindow;
        OpenOnImage (rig, request);

        labels = GetLabels (rig, rig.parts->listPane.GetMenus().GetPickerMenus().GetListItems (-1));

        Assert::IsTrue (labels.size() >= allowed.size(), L"the menu has every dialog row");

        for (const std::wstring & label : allowed)
        {
            Assert::IsTrue (std::find (labels.begin(), labels.end(), label) != labels.end(), label.c_str());
        }

        for (const std::wstring & label : labels)
        {
            Assert::IsTrue (std::find (allowed.begin(), allowed.end(), label) != allowed.end(), (L"not a dialog row: " + label).c_str());
        }
    }


    //  Delete moves the selected file to the Recycle Bin through the same call
    //  as the main window's Delete.
    TEST_METHOD (FileMenu_DeleteRecyclesTheFile)
    {
        Rig                             rig;
        PickerRequest                   request;
        std::vector<DxuiPopupMenuItem>  items;
        bool                            ran     = false;



        request.form = PickerForm::ModalOpen;
        OpenOnImage (rig, request);

        items = rig.parts->listPane.GetMenus().GetPickerMenus().GetListItems (-1);

        for (const std::shared_ptr<DxuiCommand> & command : rig.parts->menus.GetMenuCommands())
        {
            if (command->id == (int) Verb::Delete && command->dispatch)
            {
                command->dispatch();
                ran = true;
            }
        }

        Assert::IsTrue   (ran, L"the menu has Delete");
        Assert::AreEqual ((size_t) 1, rig.verbs->recycled.size());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33.dsk"), rig.verbs->recycled[0]);
    }


    //  The confirm row confirms the selected file, as Enter does.
    TEST_METHOD (FileMenu_ConfirmRowConfirmsTheFile)
    {
        Rig                             rig;
        PickerRequest                   request;
        PickerOutcome                   outcome;
        std::vector<DxuiPopupMenuItem>  items;



        request.form     = PickerForm::ModalOpen;
        rig.app.outcome  = &outcome;
        OpenOnImage (rig, request);

        items = rig.parts->listPane.GetMenus().GetPickerMenus().GetListItems (-1);
        RunCommand (rig, L"Open");

        Assert::IsTrue   (outcome.isConfirmed);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33.dsk"), outcome.path);
    }


    //  The background has New folder, Refresh and Properties, and Paste is
    //  grayed with nothing to paste.
    TEST_METHOD (BackgroundMenu_HasNewFolderAndProperties)
    {
        Rig                        rig;
        PickerRequest              request;
        std::vector<std::wstring>  labels;
        bool                       pasteGrayed = false;



        request.form = PickerForm::ModalOpen;
        OpenOnImage (rig, request);
        rig.browser.GetSelection().SetSelectedRows ({});

        labels = GetLabels (rig, rig.parts->listPane.GetMenus().GetPickerMenus().GetListItems (-1));

        for (const wchar_t * label : { ExplorerMenus::GetVerbLabel (Verb::NewFolder), ExplorerMenus::GetVerbLabel (Verb::Refresh), L"P&roperties" })
        {
            Assert::IsTrue (std::find (labels.begin(), labels.end(), std::wstring (label)) != labels.end(), label);
        }

        for (const std::shared_ptr<DxuiCommand> & command : rig.parts->menus.GetMenuCommands())
        {
            pasteGrayed = pasteGrayed || (command->label == ExplorerMenus::GetVerbLabel (Verb::Paste) && command->isEnabled && !command->isEnabled());
        }

        Assert::IsTrue (pasteGrayed, L"Paste is grayed");
        Assert::IsTrue (std::find (labels.begin(), labels.end(), std::wstring (ExplorerMenus::GetVerbLabel (Verb::NewDisk))) == labels.end(), L"no New disk");
    }


    //  A picker has Rename, so F2 renames, and has no Casso commands.
    TEST_METHOD (Picker_OffersRename)
    {
        Rig            rig;
        PickerRequest  request;



        request.form = PickerForm::ModalOpen;
        OpenOnImage (rig, request);

        Assert::IsTrue  (rig.parts->listPane.GetMenus().IsListVerbOffered (Verb::Rename));
        Assert::IsFalse (rig.parts->listPane.GetMenus().IsListVerbOffered (Verb::InsertDrive1));
    }


    //  The toolbar queries several verbs on every paint; the list is built
    //  once until the selection changes or a result arrives.
    TEST_METHOD (OfferedVerbs_AreBuiltOnceUntilTheSelectionChanges)
    {
        Rig                        rig;
        PickerRequest              request;
        const ExplorerListMenus  & menus = rig.parts->listPane.GetMenus();
        int                        builds = 0;



        request.form = PickerForm::ModalOpen;
        OpenOnImage (rig, request);

        menus.IsListVerbOffered (Verb::Rename);
        builds = menus.GetOfferedBuildCount();

        menus.IsListVerbOffered (Verb::Copy);
        menus.IsListVerbOffered (Verb::Cut);
        menus.IsListVerbOffered (Verb::Delete);
        Assert::AreEqual (builds, menus.GetOfferedBuildCount(), L"a paint with no change doesn't build the list again");

        menus.InvalidateOfferedVerbs();
        menus.IsListVerbOffered (Verb::Rename);
        Assert::AreEqual (builds + 1, menus.GetOfferedBuildCount());

        rig.parts->listPane.SetListSelection ({ 0 });
        menus.IsListVerbOffered (Verb::Rename);
        builds = menus.GetOfferedBuildCount();

        rig.parts->listPane.SetListSelection ({});
        menus.IsListVerbOffered (Verb::Rename);
        Assert::AreEqual (builds + 1, menus.GetOfferedBuildCount(), L"a new selection builds it again");
    }


    //  The Casso node is no folder on the disk and has no menu; a folder has
    //  Open and Properties.
    TEST_METHOD (TreeMenu_FolderOnly)
    {
        Rig                        rig;
        PickerRequest              request;
        std::vector<std::wstring>  labels;
        std::wstring               folderId = TreeModel::MakeFolderId (true, L"C:\\Disks\\Games");



        request.form = PickerForm::ModalOpen;
        OpenOnImage (rig, request);

        Assert::IsTrue (rig.parts->listPane.GetMenus().GetPickerMenus().GetTreeItems (TreeModel::kCassoRootId).empty());

        rig.browser.GetTreeModel().SetKnownFolders ({ L"C:\\Disks" });
        rig.browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring &) { return true; });
        rig.browser.GetTreeModel().InvalidateAll();
        rig.parts->treePane.RefreshTree();
        rig.parts->treePane.ExpandNode (TreeModel::kCassoRootId);
        rig.parts->treePane.ExpandNode (TreeModel::MakeFolderId (true, L"C:\\Disks"));

        labels = GetLabels (rig, rig.parts->listPane.GetMenus().GetPickerMenus().GetTreeItems (folderId));

        Assert::IsTrue (std::find (labels.begin(), labels.end(), std::wstring (L"&Open")) != labels.end(),       L"Open");
        Assert::IsTrue (std::find (labels.begin(), labels.end(), std::wstring (L"P&roperties")) != labels.end(), L"Properties");
    }


    //  A drive's picker opens on the Casso node, whose images are files in
    //  its folders: each has Rename, Cut, Copy and Delete, and Delete
    //  recycles the file.
    TEST_METHOD (CassoNodeImage_HasTheFileCommands)
    {
        Rig            rig;
        PickerRequest  request;



        request.form = PickerForm::DriveWindow;
        OpenOnImage (rig, request);

        rig.browser.GetTreeModel().SetKnownFolders ({ L"C:\\Disks" });
        rig.browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring &) { return true; });
        rig.browser.GetNavigator().NavigateToLocation (Location::MakeRoot (TreeModel::kCassoRootId));
        rig.parts->listPane.FillList();
        rig.browser.GetSelection().SetSelectedRows ({ 0 });

        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33.dsk"), rig.browser.GetRows()[0].hostPath);

        for (Verb verb : { Verb::Rename, Verb::Cut, Verb::Copy, Verb::Delete })
        {
            Assert::IsTrue (rig.parts->listPane.GetMenus().IsListVerbOffered (verb), ExplorerMenus::GetVerbLabel (verb));
        }

        rig.parts->rename.BeginRename();
        Assert::IsTrue (rig.parts->rename.IsRenaming(), L"the rename box opened");
        rig.parts->rename.EndRename (false);

        rig.parts->verbs.RunVerb (Verb::Delete);
        Assert::AreEqual ((size_t) 1, rig.verbs->recycled.size());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33.dsk"), rig.verbs->recycled[0]);
    }

private:
    static void  OpenOnImage (Rig & rig, PickerRequest & request)
    {
        int  image = -1;



        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        AssertSucceeded (rig.fs.WriteAllText (L"C:\\Disks\\Games\\note.txt", "x"));

        rig.app.picker = &request;
        rig.Open();
        rig.browser.GetNavigator().NavigateToAddress (L"C:\\Disks");
        rig.parts->listPane.FillList();

        for (int row = 0; row < (int) rig.browser.GetRows().size(); row++)
        {
            image = rig.browser.GetRows()[(size_t) row].isDirectory ? image : row;
        }

        Assert::IsTrue (image >= 0, L"the image is listed");
        rig.browser.GetSelection().SetSelectedRows ({ image });
    }


    //  The labels of every command the menu was built with, the icon row's too.
    static std::vector<std::wstring>  GetLabels (Rig & rig, const std::vector<DxuiPopupMenuItem> & items)
    {
        std::vector<std::wstring>  labels;



        Assert::IsFalse (items.empty(), L"the menu has rows");

        for (const std::shared_ptr<DxuiCommand> & command : rig.parts->menus.GetMenuCommands())
        {
            labels.push_back (command->label);
        }

        return labels;
    }


    static void  RunCommand (Rig & rig, const std::wstring & label)
    {
        for (const std::shared_ptr<DxuiCommand> & command : rig.parts->menus.GetMenuCommands())
        {
            if (command->label == label)
            {
                command->dispatch();
                return;
            }
        }

        Assert::Fail ((L"no row " + label).c_str());
    }
};
