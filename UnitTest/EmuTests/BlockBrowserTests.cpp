#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Ui/DiskInspector/Blocks/BlockBrowser.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"
#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"
#include "Ui/DiskInspector/Blocks/BlockNavigator.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowserTests
//
//  Block mode's tree, grouped list and address bar without a window: the
//  tree's nodes, each place's groups and rows, the address bar's segments,
//  a selection's highlight on the map, opening rows, Back and Forward, a
//  click on the map, and a search.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockBrowserTests)
{
public:

    static constexpr size_t   kBlockBytes = 512;
    static constexpr size_t   k800KBlocks = 1600;
    static constexpr int64_t  kTime       = 1'700'000'000;

    //  BLOCKS with SEED, 300 bytes, and GAMES/HELLO, 1,200 bytes.
    static void OpenModel (FakeDiskFileIo & io, BlockInspectorModel & model)
    {
        vector<Byte>                     buffer (k800KBlocks * kBlockBytes, 0);
        vector<Byte>                     next;
        FilePayload                      payload;
        std::unique_ptr<BlockImageFile>  file;
        std::wstring                     error;



        AssertSucceeded (ProDosSkeleton::Write (buffer, "BLOCKS"));

        payload.bytes.assign (300, 0x5A);
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;
        AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("SEED"), payload, next));
        buffer = next;

        AssertSucceeded (ProDosVolume (buffer).CreateDirectory (FilePath::Parse ("GAMES"), next));
        buffer = next;

        payload.bytes.assign (1200, 0x41);
        AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("GAMES/HELLO"), payload, next));

        io.files["vol.po"]  = next;
        io.stamps["vol.po"] = { next.size(), kTime };

        AssertSucceeded (BlockImageFile::Open (io, "vol.po", file, error));
        AssertSucceeded (model.Open (std::move (file)));
    }


    static int FindEntry (const BlockMap & map, const std::wstring & path)
    {
        auto  found = std::find (map.files.begin(), map.files.end(), path);



        return found == map.files.end() ? -1 : (int) (found - map.files.begin());
    }


    static const BlockListGroup * FindGroup (const BlockListing & listing, const std::wstring & title)
    {
        for (const BlockListGroup & group : listing.groups)
        {
            if (group.title == title)
            {
                return &group;
            }
        }

        return nullptr;
    }


    static int FindRowNamed (const BlockListing & listing, const std::wstring & name)
    {
        int  index = 0;



        for (const BlockListGroup & group : listing.groups)
        {
            for (const BlockListRow & row : group.rows)
            {
                if (row.cells[0] == name)
                {
                    return index;
                }

                index++;
            }
        }

        return -1;
    }


    TEST_METHOD (Tree_HasTheVolumeAndItsDirectories)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockBrowser          browser (&model);
        vector<DxuiTreeNode>  nodes;
        BlockPlace            place;



        OpenModel (io, model);
        nodes = browser.BuildTree (L"vol.po");

        Assert::AreEqual ((size_t) 1, nodes.size());
        Assert::AreEqual (std::wstring (L"BLOCKS"), nodes[0].label);
        Assert::AreEqual ((size_t) 1, nodes[0].children.size());
        Assert::AreEqual (std::wstring (L"GAMES"), nodes[0].children[0].label);
        Assert::IsTrue (BlockBrowser::ParseId (nodes[0].children[0].id, place));
        Assert::IsTrue (place.kind == BlockPlace::Kind::Directory);
    }


    TEST_METHOD (Listing_VolumeHasStructuresDirectoriesAndFiles)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockBrowser          browser (&model);
        BlockListing          listing;



        OpenModel (io, model);
        browser.Start();
        listing = browser.BuildListing (browser.GetPlace());

        Assert::IsTrue (browser.GetPlace().kind == BlockPlace::Kind::Volume, L"a volume opens at its place");
        Assert::IsNotNull (FindGroup (listing, L"Structures"));
        Assert::IsNull    (FindGroup (listing, L"Findings"), L"a sound volume has no findings");
        Assert::AreEqual ((size_t) 1, FindGroup (listing, L"Directories")->rows.size());
        Assert::AreEqual ((size_t) 1, FindGroup (listing, L"Files")->rows.size());
        Assert::AreEqual (std::wstring (L"BIN"), FindGroup (listing, L"Files")->rows[0].cells[BlockBrowser::kType]);
        Assert::AreEqual (2u, listing.groups[0].rows[FindRowNamed (listing, L"Volume directory")].first);
        Assert::AreEqual (std::wstring (L"Boot blocks"), FindGroup (listing, L"Structures")->rows[0].cells[0]);
    }


    TEST_METHOD (Listing_FileHasItsIndexBlockAndRuns)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockBrowser          browser (&model);
        BlockListing          listing;
        int                   hello = 0;



        OpenModel (io, model);
        hello   = FindEntry (model.GetVolumeMap (0), L"GAMES/HELLO");
        listing = browser.BuildListing ({ BlockPlace::Kind::File, 0, hello });

        Assert::AreEqual ((size_t) 1, FindGroup (listing, L"Index blocks")->rows.size());
        Assert::AreEqual (std::wstring (L"Run 1"), FindGroup (listing, L"Data")->rows[0].cells[0]);
        Assert::AreEqual (std::wstring (L"3"), FindGroup (listing, L"Data")->rows[0].cells[BlockBrowser::kBlocks]);
    }


    TEST_METHOD (Crumbs_RunFromTheVolumeThroughDirectoriesToTheBlock)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockBrowser          browser (&model);
        vector<BlockPlace>    targets;
        vector<std::wstring>  crumbs;
        int                   hello = 0;
        uint32_t              key   = 0;



        OpenModel (io, model);
        hello  = FindEntry (model.GetVolumeMap (0), L"GAMES/HELLO");
        key    = model.GetVolumeMap (0).entries[hello].keyBlock;
        crumbs = browser.BuildCrumbs (browser.MakeBlockPlace (key), L"vol.po", targets);

        Assert::AreEqual ((size_t) 4, crumbs.size());
        Assert::AreEqual (std::wstring (L"BLOCKS"), crumbs[0]);
        Assert::AreEqual (std::wstring (L"GAMES"),  crumbs[1]);
        Assert::AreEqual (std::wstring (L"HELLO"),  crumbs[2]);
        Assert::IsTrue (targets[1].kind == BlockPlace::Kind::Directory);
        Assert::IsTrue (targets[2].kind == BlockPlace::Kind::File);
        Assert::IsTrue (targets[3].kind == BlockPlace::Kind::Block);
    }


    TEST_METHOD (Listing_DirectoryHasItsOwnBlocksBeforeItsFiles)
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        BlockBrowser         browser (&model);
        BlockListing         listing;
        int                  games = 0;



        OpenModel (io, model);
        games   = FindEntry (model.GetVolumeMap (0), L"GAMES");
        listing = browser.BuildListing ({ BlockPlace::Kind::Directory, 0, games });

        Assert::AreEqual (std::wstring (L"Directory blocks"), listing.groups[0].title);
        Assert::IsTrue   (listing.groups[0].rows[0].kind == BlockListRow::Kind::Blocks, L"a row that opens the block");
        Assert::AreEqual (model.GetVolumeMap (0).entries[games].keyBlock, listing.groups[0].rows[0].first);
        Assert::AreEqual (std::wstring (L"Files"), listing.groups.back().title);
    }


    TEST_METHOD (Path_FormatsAndFindsPlacesAndListsDirectories)
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        BlockBrowser         browser (&model);
        BlockPlace           place;
        int                  hello = 0;



        OpenModel (io, model);
        hello = FindEntry (model.GetVolumeMap (0), L"GAMES/HELLO");

        Assert::AreEqual (std::wstring (L"/BLOCKS/GAMES/HELLO"), browser.FormatPath ({ BlockPlace::Kind::File, 0, hello }));

        Assert::IsTrue (browser.FindPath (L"/blocks/games/hello", L"vol.po", place));
        Assert::IsTrue (place.kind == BlockPlace::Kind::File && place.entry == hello);

        Assert::IsTrue (browser.FindPath (L"GAMES", L"vol.po", place));
        Assert::IsTrue (place.kind == BlockPlace::Kind::Directory);

        Assert::IsFalse (browser.FindPath (L"/BLOCKS/NOPE", L"vol.po", place));
        Assert::IsFalse (browser.FindPath (L"7", L"vol.po", place), L"a block's number is for Go to");

        Assert::AreEqual ((size_t) 1, browser.GetChildPlaces ({ BlockPlace::Kind::Volume, 0 }).size(), L"the volume's one directory");
        Assert::AreEqual (std::wstring (L"GAMES"), browser.GetChildPlaces ({ BlockPlace::Kind::Volume, 0 })[0].first);
    }


    TEST_METHOD (MapView_FilesOnGivesEachFileItsOwnColor)
    {
        FakeDiskFileIo             io;
        BlockInspectorModel        model;
        BlockMapView               view;
        DxuiDpiScaler              scaler;
        BlockMapGrid               grid (k800KBlocks, 400, 400);
        uint32_t                   a      = UINT32_MAX;
        uint32_t                   b      = UINT32_MAX;
        POINT                      at     = {};
        vector<DxuiTooltipSwatch>  first;
        vector<DxuiTooltipSwatch>  second;
        uint32_t                   block  = 0;



        OpenModel (io, model);

        for (block = 0; block < k800KBlocks; block++)
        {
            int64_t  key = model.GetOwnerKey (block);

            if (key >= 0 && a == UINT32_MAX && model.GetRole (block) == SectorRole::FileData)
            {
                a = block;
            }
            else if (key >= 0 && a != UINT32_MAX && b == UINT32_MAX && key != model.GetOwnerKey (a) && model.GetRole (block) == SectorRole::FileData)
            {
                b = block;
            }
        }

        Assert::IsTrue (b != UINT32_MAX, L"two files' data blocks");

        view.SetModel        (&model);
        view.SetFilesColored (true);
        view.Layout          (RECT { 0, 0, 400, 400 }, scaler);
        view.Refresh();

        at = grid.GetCellOrigin (grid.GetCellOf (a));
        Assert::IsTrue (view.GetTooltipSwatches ({ at.x, at.y }, first));
        at = grid.GetCellOrigin (grid.GetCellOf (b));
        Assert::IsTrue (view.GetTooltipSwatches ({ at.x, at.y }, second));

        Assert::AreNotEqual (first[0].argb, second[0].argb);
    }


    TEST_METHOD (Search_FindsAnEntryInAnyDirectory)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockBrowser          browser (&model);
        BlockListing          listing;



        OpenModel (io, model);
        browser.Start();
        listing = browser.BuildListing (browser.GetPlace(), L"hel");

        Assert::AreEqual ((size_t) 1, listing.groups.size());
        Assert::AreEqual ((size_t) 1, listing.groups[0].rows.size());
        Assert::AreEqual (std::wstring (L"GAMES/HELLO"), listing.groups[0].rows[0].cells[0]);
    }


    TEST_METHOD (Navigator_SelectsOpensAndGoesBackAcrossPlaces)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockInspectorPanel   panel;
        DxuiDarkTheme         theme;
        DxuiDpiScaler         scaler;
        MockDxuiTextRenderer  text;
        int                   hello = 0;
        int                   row   = 0;



        OpenModel (io, model);

        panel.Init   (&model, nullptr, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1400, 800 }, scaler);

        BlockNavigator  & nav = panel.GetNavigator();

        hello = FindEntry (model.GetVolumeMap (0), L"GAMES/HELLO");

        Assert::IsTrue (nav.GetPlace().kind == BlockPlace::Kind::Volume);

        nav.GoTo ({ BlockPlace::Kind::Directory, 0, FindEntry (model.GetVolumeMap (0), L"GAMES") });
        row = FindRowNamed (nav.GetListing(), L"HELLO");
        Assert::IsTrue (row > 0, L"the directory's blocks come before its files");

        nav.GetList().SetSelectedRow (row);
        nav.GoTo ({ BlockPlace::Kind::File, 0, hello });
        Assert::IsTrue (model.IsHighlighted (model.GetVolumeMap (0).entries[hello].keyBlock), L"the file's blocks are highlighted");

        panel.SelectBlock (2);
        Assert::IsTrue   (nav.IsBlockPlace());
        Assert::AreEqual (2u, model.GetBlock());
        Assert::IsTrue   (nav.GetList().IsVisible(), L"the block is drawn below the list");
        Assert::IsTrue   (nav.GetSelectedRow() != nullptr && nav.GetSelectedRow()->kind == BlockListRow::Kind::Structure, L"the list is the volume's, with the row of the block's structure selected");

        panel.GoBack();
        Assert::IsTrue (nav.GetPlace().kind == BlockPlace::Kind::File);
        panel.GoBack();
        Assert::IsTrue (nav.GetPlace().kind == BlockPlace::Kind::Directory);
        panel.GoForward();
        Assert::IsTrue (nav.GetPlace().kind == BlockPlace::Kind::File);
    }


    TEST_METHOD (Navigator_TypingInTheSearchBoxListsWhatMatches)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockInspectorPanel   panel;
        DxuiDarkTheme         theme;
        DxuiDpiScaler         scaler;
        MockDxuiTextRenderer  text;



        OpenModel (io, model);

        panel.Init   (&model, nullptr, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1400, 800 }, scaler);

        BlockNavigator  & nav = panel.GetNavigator();

        nav.GetSearch().SetFocused (true);
        Assert::IsTrue (nav.GetSearch().OnChar (L'h'));
        Assert::IsTrue (nav.GetSearch().OnChar (L'e'));

        Assert::IsTrue   (nav.IsTyping (&nav.GetSearch()), L"keys go to the search box first");
        Assert::AreEqual ((size_t) 1, nav.GetListing().groups.size());
        Assert::AreEqual (std::wstring (L"GAMES/HELLO"), nav.GetListing().groups[0].rows[0].cells[0]);
    }


    TEST_METHOD (Navigator_AMapClickSelectsTheRowThatCoversTheBlock)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockInspectorPanel   panel;
        DxuiDarkTheme         theme;
        DxuiDpiScaler         scaler;
        MockDxuiTextRenderer  text;
        int                   hello = 0;
        uint32_t              key   = 0;



        OpenModel (io, model);

        panel.Init   (&model, nullptr, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1400, 800 }, scaler);

        BlockNavigator  & nav = panel.GetNavigator();

        hello = FindEntry (model.GetVolumeMap (0), L"GAMES/HELLO");
        key   = model.GetVolumeMap (0).entries[hello].keyBlock;

        nav.SelectForBlock (key);
        Assert::AreEqual (std::wstring (L"Selected: GAMES/HELLO"), nav.GetSelectionLine(), L"the file itself is selected, in its directory");
        Assert::IsTrue   (nav.GetPlace().kind == BlockPlace::Kind::Directory, L"the list is the file's directory");
        Assert::IsTrue   (nav.GetTree().GetHighlight() == nav.GetTree().FindRowById (BlockBrowser::MakeId (nav.GetPlace())), L"the tree follows to the directory");

        nav.SelectForBlock (2);
        Assert::AreEqual (std::wstring (L"Selected: Volume directory, Blocks 2 to 5"), nav.GetSelectionLine());
    }


    TEST_METHOD (Panel_GoToBlockSelectsTheFileThatUsesTheBlock)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockInspectorPanel   panel;
        DxuiDarkTheme         theme;
        DxuiDpiScaler         scaler;
        MockDxuiTextRenderer  text;
        int                   hello = 0;
        uint32_t              key   = 0;



        OpenModel (io, model);

        panel.Init   (&model, nullptr, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1400, 800 }, scaler);

        hello = FindEntry (model.GetVolumeMap (0), L"GAMES/HELLO");
        key   = model.GetVolumeMap (0).entries[hello].keyBlock;

        panel.GoToBlock (key);
        Assert::AreEqual (key, model.GetBlock());
        Assert::AreEqual (std::wstring (L"Selected: GAMES/HELLO"), panel.GetNavigator().GetSelectionLine());
        Assert::AreEqual (std::wstring (L"/BLOCKS/GAMES"), panel.GetNavigator().GetBrowser().FormatPath (panel.GetNavigator().GetPlace()), L"the address bar is the file's directory");
    }
};
