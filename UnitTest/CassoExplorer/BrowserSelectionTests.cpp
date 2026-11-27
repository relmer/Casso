#include "Pch.h"

#include "CassoExplorer/Browser/BrowserSelection.h"
#include "CassoExplorer/Browser/IBrowserSelectionSource.h"
#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Machines/Apple2/Common/VolumeTypes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelectionTests
//
//  The selected rows and what is behind them, over rows built in memory and
//  a fake source for the location and listing, which counts the preview and
//  status updates.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BrowserSelectionTests)
{
public:

    class FakeSource : public IBrowserSelectionSource
    {
    public:
        Location               GetLocation() const override                                           { return location; }
        bool                   IsHomeLocation() const override                                        { return false; }
        bool                   IsImageLocation() const override                                       { return isImage; }
        bool                   IsVolumeLocation() const override                                      { return isImage; }
        const VolumeListing  & GetListing() const override                                            { return listing; }
        bool                   TryGetSelectedCassoLinkPaths (std::vector<std::wstring> &) const override { return false; }
        void                   UpdatePreviewAndStatus() override                                      { updates++; }

        Location       location = Location::MakeHostFolder (L"C:\\Disks");
        bool           isImage  = false;
        VolumeListing  listing;
        int            updates  = 0;
    };


    struct Rig
    {
        BrowserModel             model;
        std::vector<CatalogRow>  rows;
        std::vector<int>         selected;
        FakeSource               source;
        BrowserSelection         selection  { model, rows, selected, source };

        Rig()
        {
            model.OpenTab (source.location);
        }

        void AddRow (const wchar_t * name, bool isDiskImage = false)
        {
            CatalogRow  row;

            row.name        = name;
            row.isDiskImage = isDiskImage;
            row.sourceIndex = rows.size();

            rows.push_back (row);
        }
    };



    TEST_METHOD (StockDisks_AreSelectedByTheirFiles)
    {
        Rig                        rig;
        std::vector<std::wstring>  paths;



        rig.source.location = Location::MakeRoot (TreeModel::kStockDisksId);
        rig.AddRow (L"ProDOS 2.4.3", true);
        rig.AddRow (L"DOS 3.3 System Master", true);
        rig.rows[0].hostPath        = L"C:\\Stock\\ProDOS 2.4.3.po";
        rig.rows[0].isStockDisk     = true;
        rig.rows[0].stockDownloaded = true;
        rig.rows[1].hostPath        = L"C:\\Stock\\DOS 3.3 System Master.dsk";
        rig.rows[1].isStockDisk     = true;

        rig.selection.SetSelectedRows ({ 0, 1 });
        rig.selection.GetSelectedHostPaths (paths);

        Assert::AreEqual ((size_t) 2, paths.size(), L"one not downloaded yet is downloaded there first");
        Assert::AreEqual (std::wstring (L"C:\\Stock\\DOS 3.3 System Master.dsk"), paths[1]);
        Assert::AreEqual (std::wstring (L"C:\\Stock\\ProDOS 2.4.3.po"), paths[0]);
    }



    TEST_METHOD (CassoNodeImages_AreImages_UnlessNotDownloaded)
    {
        Rig  rig;



        rig.source.location = Location::MakeRoot (TreeModel::kCassoRootId);
        rig.AddRow (L"Sixies.dsk", true);
        rig.AddRow (L"DOS 3.3 System Master", true);
        rig.rows[0].hostPath    = L"C:\\Demo\\Sixies.dsk";
        rig.rows[1].hostPath    = L"C:\\Stock\\DOS 3.3 System Master.dsk";
        rig.rows[1].isStockDisk = true;

        rig.selection.SetSelectedRows ({ 0 });
        Assert::IsTrue  (rig.selection.AreSelectedRowsImages(), L"A file the Casso node lists is an image, for its thumbnail");

        rig.selection.SetSelectedRows ({ 1 });
        Assert::IsFalse (rig.selection.AreSelectedRowsImages(), L"A stock disk not downloaded has no file yet");

        rig.rows[1].stockDownloaded = true;
        Assert::IsTrue  (rig.selection.AreSelectedRowsImages());
    }



    TEST_METHOD (SetSelectedRows_DropsRowsOutOfRange_AndUpdatesTheView)
    {
        Rig  rig;



        rig.AddRow (L"a.dsk", true);
        rig.AddRow (L"b.txt");

        rig.selection.SetSelectedRows ({ 1, 7, -1 });

        Assert::AreEqual ((size_t) 1, rig.selected.size());
        Assert::AreEqual (1, rig.selected[0]);
        Assert::AreEqual (1, rig.source.updates);
    }



    TEST_METHOD (RowKeys_CountRepeatedNames_AndSelectByKeyFindsThem)
    {
        Rig                        rig;
        std::vector<std::wstring>  keys;



        rig.AddRow (L"HELLO");
        rig.AddRow (L"HELLO");
        rig.AddRow (L"WORLD");

        rig.selection.GetRowKeys (keys);

        Assert::AreEqual (std::wstring (L"0:HELLO"), keys[0]);
        Assert::AreEqual (std::wstring (L"1:HELLO"), keys[1]);
        Assert::AreEqual (keys[1], rig.selection.GetSelectionKey (1));

        rig.selection.SelectRowsByKeys ({ L"1:HELLO", L"0:WORLD" });

        Assert::AreEqual ((size_t) 2, rig.selected.size());
        Assert::AreEqual (1, rig.selected[0]);
        Assert::AreEqual (2, rig.selected[1]);
    }



    TEST_METHOD (SelectRowAt_PastTheEnd_SelectsTheLastRow)
    {
        Rig  rig;



        rig.selection.SelectRowAt (3);
        Assert::IsTrue (rig.selected.empty(), L"An empty list selects nothing");

        rig.AddRow (L"a");
        rig.AddRow (L"b");
        rig.selection.SelectRowAt (5);

        Assert::AreEqual ((size_t) 1, rig.selected.size());
        Assert::AreEqual (1, rig.selected[0]);
    }



    TEST_METHOD (SelectedHostPathsAndImages_InAHostFolder)
    {
        Rig                        rig;
        std::vector<std::wstring>  paths;



        rig.AddRow (L"a.dsk", true);
        rig.AddRow (L"b.txt");

        rig.selection.SetSelectedRows ({ 0 });
        rig.selection.GetSelectedHostPaths (paths);

        Assert::AreEqual ((size_t) 1, paths.size());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\a.dsk"), paths[0]);
        Assert::IsTrue   (rig.selection.AreSelectedRowsImages());

        rig.selection.SetSelectedRows ({ 0, 1 });
        Assert::IsFalse  (rig.selection.AreSelectedRowsImages());
    }



    TEST_METHOD (SelectedEntries_InAnImage_AreTheListingsEntries)
    {
        Rig                     rig;
        std::vector<FileEntry>  entries;
        const FileEntry       * entry = nullptr;



        rig.source.isImage = true;
        rig.source.listing.entries.resize (2);
        rig.source.listing.entries[1].name = "WORLD";
        rig.AddRow (L"HELLO");
        rig.AddRow (L"WORLD");

        rig.selection.SetSelectedRows ({ 1 });
        rig.selection.GetSelectedEntries (entries);

        Assert::AreEqual ((size_t) 1, entries.size());
        Assert::AreEqual (std::string ("WORLD"), entries[0].name);
        Assert::IsTrue   (rig.selection.TryGetSelectedEntry (entry));
        Assert::AreEqual (std::string ("WORLD"), entry->name);
    }



    TEST_METHOD (HasShellRows_ShellFoldersAndHome)
    {
        Assert::IsTrue  (BrowserSelection::HasShellRows (Location::Kind::None, true));
        Assert::IsTrue  (BrowserSelection::HasShellRows (Location::Kind::ShellFolder, false));
        Assert::IsFalse (BrowserSelection::HasShellRows (Location::Kind::HostFolder, false));
    }
};
