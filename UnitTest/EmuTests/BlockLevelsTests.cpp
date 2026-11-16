#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Ui/DiskInspector/Blocks/BlockFilesLevel.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"
#include "Ui/DiskInspector/Blocks/BlockLevelHost.h"
#include "Ui/DiskInspector/Blocks/BlockLevels.h"
#include "Ui/DiskInspector/Blocks/BlockSectionsView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelsTests
//
//  Block mode's Card, Volume, Files and File levels without a window: the
//  text of each level, the breadcrumb, Back and Forward across levels, a
//  link from a level to its block, and the Files level's search on a
//  volume of 20,000 entries.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockLevelsTests)
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


    static const BlockField * FindRow (const vector<BlockSection> & sections, const std::wstring & label)
    {
        for (const BlockSection & section : sections)
        {
            for (const BlockField & row : section.rows)
            {
                if (row.label == label)
                {
                    return &row;
                }
            }
        }

        return nullptr;
    }


    TEST_METHOD (Volume_ListsTheHeaderAndLinksEachStructureToItsBlocks)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        vector<BlockSection>  sections;



        OpenModel (io, model);
        sections = BlockLevels::BuildVolume (model, 0);

        Assert::AreEqual (std::wstring (L"BLOCKS"), FindRow (sections, L"Name")->value);
        Assert::AreEqual (std::wstring (L"ProDOS"), FindRow (sections, L"File system")->value);
        Assert::AreEqual (std::wstring (L"1,600"),  FindRow (sections, L"Blocks")->value);
        Assert::AreEqual (std::wstring (L"2"),      FindRow (sections, L"Files")->value);
        Assert::AreEqual (std::wstring (L"1"),      FindRow (sections, L"Directories")->value);

        Assert::AreEqual ((int64_t) 0, FindRow (sections, L"Boot blocks")->link);
        Assert::AreEqual ((int64_t) 2, FindRow (sections, L"Volume directory")->link);
        Assert::IsNotNull (FindRow (sections, L"Volume bitmap"));
        Assert::IsNotNull (FindRow (sections, L"No problems found"));
    }


    TEST_METHOD (File_HasItsEntryKeyBlockAndRuns)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        vector<BlockSection>  sections;
        int                   hello = 0;



        OpenModel (io, model);
        hello    = FindEntry (model.GetVolumeMap (0), L"GAMES/HELLO");
        sections = BlockLevels::BuildFile (model, 0, hello);

        Assert::AreEqual (std::wstring (L"HELLO"),    FindRow (sections, L"Name")->value);
        Assert::AreEqual (std::wstring (L"BIN"),      FindRow (sections, L"Type")->value);
        Assert::AreEqual (std::wstring (L"1,200 bytes"), FindRow (sections, L"EOF")->value);
        Assert::AreEqual (std::wstring (L"Sapling"),  FindRow (sections, L"Storage")->value);
        Assert::AreNotEqual (BlockField::kNoLink,     FindRow (sections, L"Key block")->link);
        Assert::IsNotNull (FindRow (sections, L"Index block"));
        Assert::IsNotNull (FindRow (sections, L"Run 1"));
    }


    TEST_METHOD (Crumbs_RunFromTheVolumeThroughFilesToTheFileAndItsBlock)
    {
        FakeDiskFileIo                    io;
        BlockInspectorModel               model;
        vector<BlockLevelHost::Location>  targets;
        vector<std::wstring>              crumbs;
        int                               hello = 0;
        uint32_t                          key   = 0;



        OpenModel (io, model);
        hello  = FindEntry (model.GetVolumeMap (0), L"GAMES/HELLO");
        key    = model.GetVolumeMap (0).entries[hello].keyBlock;
        crumbs = BlockLevelHost::BuildCrumbs (model, { BlockLevelHost::Level::Block, 0, -1, key }, L"vol.po", false, targets);

        Assert::AreEqual ((size_t) 5, crumbs.size());
        Assert::AreEqual (std::wstring (L"BLOCKS"), crumbs[0]);
        Assert::AreEqual (std::wstring (L"Files"),  crumbs[1]);
        Assert::AreEqual (std::wstring (L"GAMES"),  crumbs[2]);
        Assert::AreEqual (std::wstring (L"HELLO"),  crumbs[3]);
        Assert::IsTrue (targets[2].level == BlockLevelHost::Level::Files);
        Assert::IsTrue (targets[3].level == BlockLevelHost::Level::File);
    }


    TEST_METHOD (Host_OpensAtVolumeAndBackAndForwardCrossLevels)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockInspectorPanel   panel;
        DxuiDarkTheme         theme;
        DxuiDpiScaler         scaler;
        MockDxuiTextRenderer  text;
        int                   hello = 0;



        OpenModel (io, model);

        panel.Init   (&model, nullptr, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1180, 700 }, scaler);

        BlockLevelHost  & levels = panel.GetLevels();

        Assert::IsTrue (levels.GetLevel() == BlockLevelHost::Level::Volume, L"a volume opens at its Volume level");

        hello = FindEntry (model.GetVolumeMap (0), L"GAMES/HELLO");

        levels.GoTo ({ BlockLevelHost::Level::Files, 0, -1 });
        levels.GetFiles().SetSearch (L"hel");
        Assert::AreEqual ((size_t) 1, levels.GetFiles().GetRowEntries().size(), L"the search finds HELLO in GAMES");

        levels.GoTo ({ BlockLevelHost::Level::File, 0, hello });
        Assert::IsTrue (model.IsHighlighted (model.GetVolumeMap (0).entries[hello].keyBlock), L"the file's blocks are highlighted");

        panel.SelectBlock (2);
        Assert::IsTrue  (levels.GetLevel() == BlockLevelHost::Level::Block);
        Assert::AreEqual (2u, model.GetBlock());

        panel.GoBack();
        Assert::IsTrue (levels.GetLevel() == BlockLevelHost::Level::File);
        panel.GoBack();
        Assert::IsTrue (levels.GetLevel() == BlockLevelHost::Level::Files);
        panel.GoForward();
        Assert::IsTrue (levels.GetLevel() == BlockLevelHost::Level::File);
    }


    TEST_METHOD (Host_ALinkAtTheVolumeLevelGoesToItsBlock)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockInspectorPanel   panel;
        DxuiDarkTheme         theme;
        DxuiDpiScaler         scaler;
        MockDxuiTextRenderer  text;
        DxuiMouseEvent        ev;
        BlockField            row;
        RECT                  bounds = {};
        int                   line   = 0;



        OpenModel (io, model);

        panel.Init   (&model, nullptr, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1180, 700 }, scaler);

        BlockSectionsView  & view = panel.GetLevels().GetVolumeView();

        bounds = view.GetBounds();

        for (line = 0; line < 40; line++)
        {
            ev.positionDip = { bounds.left + 300, bounds.top + (LONG) (line * BlockSectionsView::kRowDip) + 4 };

            if (view.TryGetRowAt (ev.positionDip, row) && row.label == L"Volume directory")
            {
                break;
            }
        }

        Assert::AreEqual (std::wstring (L"Volume directory"), row.label);

        ev.kind   = DxuiMouseEventKind::Down;
        ev.button = DxuiMouseButton::Left;
        panel.OnMouse (ev);

        Assert::IsTrue   (panel.GetLevels().GetLevel() == BlockLevelHost::Level::Block);
        Assert::AreEqual (2u, model.GetBlock());
    }


    TEST_METHOD (Files_ASearchOf20000EntriesTakesUnderASecond)
    {
        static constexpr int  kEntries     = 20000;
        static constexpr int  kDirectories = 200;

        BlockMap         map;
        BlockFilesLevel  files;
        DxuiDarkTheme    theme;
        auto             start   = std::chrono::steady_clock::now();
        int64_t          elapsed = 0;



        map.fileSystem = BlockFileSystem::ProDos;
        map.volumeName = L"BIG";
        map.blocks.assign (65535, BlockMapEntry());

        for (int i = 0; i < kEntries; i++)
        {
            BlockFileEntry  entry;

            entry.isDirectory = i < kDirectories;
            entry.parent      = i < kDirectories ? -1 : i % kDirectories;
            entry.name        = std::format (L"FILE{}", i);
            entry.keyBlock    = (uint32_t) (100 + i);

            map.files.push_back (entry.name);
            map.entries.push_back (entry);
            map.blocks[100 + i].owners.push_back (i);
        }

        files.Init (&theme, nullptr);

        start = std::chrono::steady_clock::now();
        files.SetVolume (&map, 0);
        files.SetSearch (L"file19999");
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now() - start).count();

        Assert::AreEqual ((size_t) 1, files.GetRowEntries().size());
        Assert::AreEqual (19999, files.GetRowEntries()[0]);
        Assert::IsTrue (elapsed < 1000, std::format (L"{} ms", elapsed).c_str());
    }
};
