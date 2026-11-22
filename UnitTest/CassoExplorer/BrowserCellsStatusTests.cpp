#include "Pch.h"

#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/Browser/BrowserHostInterfaces.h"
#include "CassoExplorer/Browser/BrowserParts.h"
#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/DiskImageCatalog.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/HostFolderReader.h"
#include "Devices/Disk/ImageClassCache.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../UiTests/InMemoryFileSystem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCellsStatusTests
//
//  The status bar's text from list state built in memory, with a fake host
//  for Home and the row listing.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BrowserCellsStatusTests)
{
public:

    class FakeHost : public IBrowserCellsHost
    {
    public:
        explicit FakeHost (BrowserListState & list) : m_list (list) {}

        bool                      IsHomeLocation() const override { return false; }
        BrowserRowPaths::Listing  GetRowListing() override        { return BrowserRowPaths::Listing { Location(), m_list.rows, m_list.rootChildren, false, false, nullptr }; }

    private:
        BrowserListState  & m_list;
    };


    struct Rig
    {
        InMemoryFileSystem          fs;
        FakeDiskFileIo              io;
        ImageClassCache             imageClasses;
        DiskOperations              operations    { io };
        DiskImageCatalog            catalog       { operations };
        HostFolderReader            hostFolders;
        TreeModel                   tree          { fs, hostFolders };
        BrowserModel                model;
        CassoExplorerShellListings  shellListings;
        BrowserListState            list;
        BrowserSources              sources;
        BrowserParts                parts         { fs, io, imageClasses, catalog, tree, hostFolders, model, operations, shellListings, list, sources };
        FakeHost                    host          { list };
        BrowserCells                cells         { parts, host };

        void AddRow (const wchar_t * name, uint64_t sizeBytes)
        {
            CatalogRow  row;

            row.name        = name;
            row.sizeBytes   = sizeBytes;
            row.sourceIndex = list.rows.size();

            list.rows.push_back (row);
        }
    };



    TEST_METHOD (UpdateStatus_CountsTheRows_AndTheSelection)
    {
        Rig  rig;



        rig.AddRow (L"a.txt", 100);
        rig.AddRow (L"b.txt", 200);
        rig.list.selectedRows = { 1 };

        rig.cells.UpdateStatus();

        Assert::AreEqual (BrowserCells::FormatSelection (0, 2), rig.list.status.selection);
        Assert::AreEqual (0u, (unsigned) rig.list.status.selected.find (L"1 item selected"));
        Assert::IsTrue   (rig.list.status.freeSpace.empty(), L"A host folder has no volume space");
    }



    TEST_METHOD (UpdateStatus_NoSelection_LeavesTheSelectedFieldEmpty)
    {
        Rig  rig;



        rig.AddRow (L"a.txt", 100);

        rig.cells.UpdateStatus();

        Assert::IsTrue (rig.list.status.selected.empty());
        Assert::IsTrue (rig.list.status.detail.empty());
    }
};
