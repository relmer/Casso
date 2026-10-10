#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Devices/Disk/Inspector/FileMap/SectorSource.h"
#include "Devices/Disk/Inspector/InspectorSearch.h"
#include "Ui/DiskInspector/FileMapText.h"
#include "FileMapTestImages.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using Images = FileMapTestImages;





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTests
//
//  The file and sector map on made-up volumes (SC-014, SC-015): every role
//  and owner where the layout puts it, files in order with their holes, no
//  findings on a standard volume of each system, no volume mistaken for
//  another, and each kind of damage reported once with its place while the
//  files read before it stay listed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FileMapTests)
{
public:

    static FileMap MapOf (const vector<Byte> & image, bool isProDosOrder)
    {
        DiskAnalysis     analysis;
        vector<FileMap>  maps;



        Images::Analyze (image, isProDosOrder, analysis);
        maps = FileMapBuilder::Build (analysis);
        Assert::AreEqual (static_cast<size_t> (1), maps.size());

        return maps[0];
    }



    static FileMap MapOfBroken (const vector<Byte> & image, bool isProDosOrder, int track, int physical)
    {
        DiskAnalysis  analysis;



        Images::Analyze (image, isProDosOrder, analysis);
        Images::BreakSector (analysis, track, physical, false);

        return FileMapBuilder::Build (analysis)[0];
    }



    static const MappedFile & FileOf (const FileMap & map, const std::wstring & path)
    {
        auto  found = std::find_if (map.files.begin(), map.files.end(), [&] (const MappedFile & f) { return f.path == path; });



        Assert::IsTrue (found != map.files.end(), path.c_str());

        return *found;
    }



    static int CountKind (const FileMap & map, FindingKind kind)
    {
        return static_cast<int> (std::count_if (map.findings.begin(), map.findings.end(), [&] (const Finding & f) { return f.kind == kind; }));
    }



    static void AssertOnce (const FileMap & map, FindingKind kind, bool hasPlace)
    {
        auto  found = std::find_if (map.findings.begin(), map.findings.end(), [&] (const Finding & f) { return f.kind == kind; });



        Assert::AreEqual (1, CountKind (map, kind), L"reported exactly once");
        Assert::IsFalse  (found->detail.empty(), L"with its reason");
        Assert::IsTrue   (!hasPlace || found->quarterTrack >= 0, L"and its place");
    }



    static int DosCell (int track, int logical) { return track * 16 + logical; }



    TEST_METHOD (Dos33RolesOwnersAndFileOrder)
    {
        FileMap  map = MapOf (Images::MakeDos33(), false);



        Assert::IsTrue (map.fileSystem == MapFileSystem::Dos33);
        Assert::IsTrue (map.findings.empty(), L"a standard volume gives no findings");

        Assert::AreEqual (4,   FileOf (map, L"HELLO").GetUsedCount());
        Assert::AreEqual (253, FileOf (map, L"BIG").GetUsedCount());
        Assert::IsTrue   (FileOf (map, L"BIG").sectors[123].role == SectorRole::IndexBlock, L"each list ahead of the sectors it gives");
        Assert::IsTrue   (FileOf (map, L"BIG").sectors[246].role == SectorRole::IndexBlock);
        Assert::AreEqual (-1, FileOf (map, L"SPARSE").sectors[2].cell, L"the hole in its place");
        Assert::IsTrue   (FileOf (map, L"OLD").isDeleted);
        Assert::AreEqual (2, FileOf (map, L"OLD").stillFree);

        Assert::IsTrue (map.cells[DosCell (17, 0)].role == SectorRole::VtocOrKeyBlock);
        Assert::IsTrue (map.cells[DosCell (17, 15)].role == SectorRole::CatalogOrDirectory);
        Assert::IsTrue (map.cells[DosCell (0, 0)].role == SectorRole::BootArea);
        Assert::IsTrue (map.cells[DosCell (18, 14)].role == SectorRole::FileData);
        Assert::AreEqual (static_cast<size_t> (1), map.cells[DosCell (18, 14)].owners.size());
        Assert::IsTrue (map.cells[DosCell (10, 0)].role == SectorRole::Free);
    }



    TEST_METHOD (ProDosRolesForksTreesAndSubdirectories)
    {
        for (bool isProDosOrder : { true, false })
        {
            vector<Byte>  image = Images::MakeProDos();
            FileMap       map   = MapOf (isProDosOrder ? image : Images::ToDosOrder (image), isProDosOrder);

            Assert::IsTrue (map.fileSystem == MapFileSystem::ProDos);
            Assert::IsTrue (map.findings.empty(), L"a standard volume gives no findings");

            Assert::AreEqual (6, FileOf (map, L"SPARSE.TREE").GetUsedCount());
            Assert::AreEqual (-1, FileOf (map, L"SPARSE.TREE").sectors[3].cell, L"the hole between its data blocks");
            Assert::AreEqual (5, FileOf (map, L"FORKED").GetUsedCount(), L"both forks under the extended key block");
            Assert::AreEqual (0, FileOf (map, L"EMPTY").GetUsedCount());
            Assert::IsTrue   (FileOf (map, L"GONE").isDeleted);
            Assert::AreEqual (1, FileOf (map, L"DIR1/DIR2/DEEP").GetUsedCount(), L"two subdirectories deep");

            Assert::IsTrue (map.cells[2].role == SectorRole::VtocOrKeyBlock);
            Assert::IsTrue (map.cells[3].role == SectorRole::CatalogOrDirectory);
            Assert::IsTrue (map.cells[6].role == SectorRole::VolumeBitmap);
            Assert::IsTrue (map.cells[20].role == SectorRole::Subdirectory);
            Assert::IsTrue (map.cells[14].role == SectorRole::IndexBlock);
            Assert::IsTrue (map.cells[19].role == SectorRole::Free, L"a deleted file owns nothing");
        }
    }



    TEST_METHOD (PascalRunsSizesAndBadBlocks)
    {
        for (bool isProDosOrder : { true, false })
        {
            vector<Byte>  image = Images::MakePascal();
            FileMap       map   = MapOf (isProDosOrder ? image : Images::ToDosOrder (image), isProDosOrder);

            Assert::IsTrue (map.fileSystem == MapFileSystem::Pascal);
            Assert::IsTrue (map.findings.empty(), L"a standard volume gives no findings");
            Assert::AreEqual (static_cast<uint64_t> (2560), FileOf (map, L"SYSTEM.PASCAL").recordedSize);
            Assert::AreEqual (static_cast<uint64_t> (1636), FileOf (map, L"NOTES.TEXT").recordedSize);
            Assert::IsTrue   (map.cells[20].role == SectorRole::BadBlocksFile);
            Assert::IsTrue   (map.cells[12].role == SectorRole::Free, L"the gap between files");
        }
    }



    TEST_METHOD (CpmExtentsHolesUsersAndTheSystemEntry)
    {
        for (bool isProDosOrder : { false, true })
        {
            vector<Byte>  image = Images::MakeCpm();
            FileMap       map   = MapOf (isProDosOrder ? Images::ToProDosOrder (image) : image, isProDosOrder);

            Assert::IsTrue (map.fileSystem == MapFileSystem::Cpm);
            Assert::IsTrue (map.findings.empty(), L"a standard volume gives no findings");
            Assert::AreEqual (18 * 4, FileOf (map, L"0:PIP.COM").GetUsedCount(), L"two extents' blocks");
            Assert::AreEqual (-1, FileOf (map, L"0:SPARSE.DAT").sectors[4].cell, L"block 0 is a hole");
            Assert::AreEqual (4, FileOf (map, L"5:NOTES.TXT").GetUsedCount());
            Assert::IsTrue   (std::none_of (map.files.begin(), map.files.end(), [] (const MappedFile & f) { return f.path.starts_with (L"31:"); }), L"the system entry is no file");
            Assert::IsTrue   (FileOf (map, L"0:OLD.TXT").isDeleted);
            Assert::IsTrue   (map.cells[0].role == SectorRole::BootArea);
            Assert::IsTrue   (map.cells[3 * 16].role == SectorRole::CatalogOrDirectory);
        }
    }



    TEST_METHOD (NoVolumeIsMistakenForPascalOrCpm)
    {
        Assert::IsTrue (MapOf (Images::MakeDos33(), false).fileSystem == MapFileSystem::Dos33);
        Assert::IsTrue (MapOf (Images::MakeProDos(), true).fileSystem == MapFileSystem::ProDos);
        Assert::IsTrue (MapOf (vector<Byte> (Images::kImageBytes, 0), false).notMapped == NotMappedReason::NoFileSystem);
    }



    TEST_METHOD (AnUnreadableVtocIsGivenAsTheLikelyCause)
    {
        FileMap  map = MapOfBroken (Images::MakeDos33(), false, 17, SectorSource::GetDos33Physical (0));



        Assert::IsTrue (map.notMapped == NotMappedReason::NoFileSystem);
        Assert::IsTrue (map.likelyCause.find (L"Track 17") != std::wstring::npos);
    }



    TEST_METHOD (CrossLinksAndBitmapProblemsAreReportedOnce)
    {
        vector<Byte>  image = Images::MakeDos33();
        Byte *        vtoc  = Images::GetDosSector (image, 17, 0);
        Byte *        list  = Images::GetDosSector (image, 18, 15);
        FileMap       map;



        //  SPARSE's second data sector is HELLO's first, its own freed; track
        //  10 sector 0 is marked used with no owner; HELLO's last data sector
        //  is marked free.
        Images::GetDosSector (image, 34, 14)[0x10] = list[0x0C];
        Images::GetDosSector (image, 34, 14)[0x11] = list[0x0D];
        vtoc[0x38 + 4 * 10 + 1] &= static_cast<Byte> (~1);
        vtoc[0x38 + 4 * 18 + 0] |= static_cast<Byte> (1 << (12 % 8));
        vtoc[0x38 + 4 * 34 + 0] |= static_cast<Byte> (1 << (12 % 8));

        map = MapOf (image, false);

        Assert::IsTrue (map.cells[DosCell (18, 14)].role == SectorRole::CrossLinked);
        AssertOnce (map, FindingKind::FileCrossLinked, true);
        AssertOnce (map, FindingKind::FileAllocatedUnowned, true);
        AssertOnce (map, FindingKind::FileOwnedMarkedFree, true);
        Assert::IsTrue ((FileOf (map, L"HELLO").states & MappedFile::kCrossLinked) != 0);
    }



    TEST_METHOD (BrokenDosChainsKeepWhatWasRead)
    {
        vector<Byte>  loopList  = Images::MakeDos33();
        vector<Byte>  outside   = Images::MakeDos33();
        vector<Byte>  sixteen   = Images::MakeDos33();
        vector<Byte>  loopCat   = Images::MakeDos33();
        vector<Byte>  miscount  = Images::MakeDos33();
        FileMap       map;



        //  BIG's last list points back at its first.
        Images::GetDosSector (loopList, 33, 5)[0x01] = 18;
        Images::GetDosSector (loopList, 33, 5)[0x02] = 11;
        map = MapOf (loopList, false);
        Assert::IsTrue (FileOf (map, L"BIG").chainReason == ChainReason::Loop);
        Assert::AreEqual (4, FileOf (map, L"HELLO").GetUsedCount(), L"files read before the damage stay listed");

        //  HELLO's first pair points at track 40, and SPARSE's at sector $10.
        Images::GetDosSector (outside, 18, 15)[0x0C] = 40;
        map = MapOf (outside, false);
        Assert::IsTrue (FileOf (map, L"HELLO").chainReason == ChainReason::OutsideVolume);
        AssertOnce (map, FindingKind::FileChainBroken, false);

        Images::GetDosSector (sixteen, 34, 14)[0x0D] = 0x10;
        map = MapOf (sixteen, false);
        Assert::IsTrue (FileOf (map, L"SPARSE").chainReason == ChainReason::OutsideVolume);

        //  The catalog's second sector links back to its first.
        Images::GetDosSector (loopCat, 17, 14)[0x01] = 17;
        Images::GetDosSector (loopCat, 17, 14)[0x02] = 15;
        map = MapOf (loopCat, false);
        Assert::IsFalse (map.isCatalogComplete);
        AssertOnce (map, FindingKind::FileChainBroken, true);
        Assert::AreEqual (4, FileOf (map, L"HELLO").GetUsedCount());

        //  HELLO's catalog count says five sectors.
        Images::GetDosSector (miscount, 17, 15)[0x0B + 0x21] = 5;
        map = MapOf (miscount, false);
        AssertOnce (map, FindingKind::FileCountDiffers, false);
    }



    TEST_METHOD (AnUnreadableCatalogSectorIsGiven)
    {
        FileMap  map = MapOfBroken (Images::MakeDos33(), false, 17, SectorSource::GetDos33Physical (14));



        Assert::IsFalse  (map.isCatalogComplete);
        Assert::AreEqual (DosCell (17, 14), map.unreadableCell);
        Assert::AreEqual (4, FileOf (map, L"HELLO").GetUsedCount(), L"the first catalog sector's files stay listed");
    }



    TEST_METHOD (BrokenProDosChains)
    {
        vector<Byte>  loopDir = Images::MakeProDos();
        FileMap       map;



        //  Block 5's next is block 3 again.
        Images::GetBlock (loopDir, 5)[2] = 3;
        map = MapOf (loopDir, true);
        AssertOnce (map, FindingKind::FileChainBroken, true);
        Assert::AreEqual (1, FileOf (map, L"SEED").GetUsedCount());

        //  SPARSE.TREE's first index block is bad.
        map = MapOfBroken (Images::MakeProDos(), true, 1, SectorSource::GetBlockPhysical (9, 0));
        Assert::IsTrue (FileOf (map, L"SPARSE.TREE").chainReason == ChainReason::BadSector);
    }



    TEST_METHOD (PascalDamage)
    {
        vector<Byte>  overlap = Images::MakePascal();
        vector<Byte>  past    = Images::MakePascal();
        FileMap       map;



        //  NOTES.TEXT starts inside SYSTEM.PASCAL.
        Images::GetBlock (overlap, 2)[26 * 2] = 9;
        map = MapOf (overlap, true);
        AssertOnce (map, FindingKind::PascalEntryOutOfOrder, true);
        Assert::IsTrue (CountKind (map, FindingKind::FileCrossLinked) >= 1, L"the shared blocks are cross-linked");

        //  The .BAD file runs past the volume's end.
        Images::GetBlock (past, 2)[26 * 3 + 2] = 0x2C;
        Images::GetBlock (past, 2)[26 * 3 + 3] = 0x01;
        map = MapOf (past, true);
        AssertOnce (map, FindingKind::PascalEntryOutOfRange, true);

        //  A directory sector that is bad.
        map = MapOfBroken (Images::MakePascal(), true, 0, SectorSource::GetBlockPhysical (3, 1));
        Assert::IsFalse (map.isCatalogComplete);
    }



    TEST_METHOD (CpmDamage)
    {
        vector<Byte>  shared  = Images::MakeCpm();
        vector<Byte>  invalid = Images::MakeCpm();
        FileMap       map;



        //  NOTES.TXT takes SPARSE.DAT's first block.
        Images::GetCpmSector (shared, 3, 0)[3 * 32 + 16] = 20;
        map = MapOf (shared, false);
        Assert::AreEqual (4, CountKind (map, FindingKind::FileCrossLinked), L"each of the block's four sectors");

        //  An entry with user 40 among the valid ones.
        Images::GetCpmSector (invalid, 3, 0)[6 * 32] = 40;
        map = MapOf (invalid, false);
        Assert::IsTrue (map.fileSystem == MapFileSystem::Cpm, L"more than four valid entries still find the volume");
        AssertOnce (map, FindingKind::CpmInvalidEntry, true);
    }



    TEST_METHOD (TheTabSaysWhereEachSectorSitsInItsFile)
    {
        FileMap           map  = MapOf (Images::MakeDos33(), false);
        vector<TableRow>  rows = FileMapText::BuildFileRows (map, false, false, 0, false);
        std::wstring      copy = FileMapText::FormatMap (map);



        Assert::AreEqual (std::wstring (L"HELLO, data sector 2 of 3"), FileMapText::FormatOwners (map, DosCell (18, 13)));
        Assert::AreEqual (std::wstring (L"HELLO, track/sector list"),  FileMapText::FormatOwners (map, DosCell (18, 15)));
        Assert::IsTrue   (FileMapText::FormatTooltip (map, DosCell (17, 0)).starts_with (L"Track 17, DOS 3.3 sector $0 (physical $0)"));
        Assert::IsTrue   (std::none_of (rows.begin(), rows.end(), [] (const TableRow & r) { return r.cells[0] == L"OLD"; }), L"deleted files hidden by default");
        Assert::AreEqual (std::wstring (L"Complete"), rows[0].cells[4]);
        Assert::IsTrue   (copy.starts_with (L" 0  BBBBBBBBBBBBBBBB\r\n"), L"a row per track");
        Assert::IsTrue   (copy.find (L"V  VTOC\r\n") != std::wstring::npos, L"and the key to the letters");
        Assert::IsTrue   (copy.find (L"17  VCCCCCCCCCCCCCCC") != std::wstring::npos);
    }



    TEST_METHOD (NotMappedReasonsAreGiven)
    {
        FileMap  zero = MapOf (vector<Byte> (Images::kImageBytes, 0), false);



        Assert::IsTrue (FileMapText::FormatNotMapped (zero).starts_with (L"No DOS 3.3, ProDOS, Apple Pascal or CP/M volume was found."));
    }



    TEST_METHOD (GoToAFileSelectsItsFirstSector)
    {
        DiskAnalysis  analysis;
        GoToTarget    target;
        std::wstring  error;



        Images::Analyze (Images::MakeDos33(), false, analysis);

        Assert::IsTrue   (InspectorGoTo::Resolve (analysis, 0, GoToKind::File, L"hello", target, error), error.c_str());
        Assert::AreEqual (18 * 4, target.quarterTrack);
        Assert::AreEqual (SectorSource::GetDos33Physical (15), static_cast<int> (analysis.tracks[analysis.entries[18 * 4].slot]->sectors[target.sectorIndex].sector),
                          L"its track/sector list");
        Assert::IsFalse  (InspectorGoTo::Resolve (analysis, 0, GoToKind::File, L"NOSUCH", target, error));
        Assert::IsFalse  (InspectorGoTo::Resolve (analysis, 0, GoToKind::File, L"OLD", target, error), L"a deleted file is not gone to");
    }



    TEST_METHOD (TheMapIsBuiltWellWithinATenthOfASecond)
    {
        DiskAnalysis  analysis;



        Images::Analyze (Images::MakeDos33(), false, analysis);

        auto  start = std::chrono::steady_clock::now();

        (void) FileMapBuilder::Build (analysis);

        Assert::IsTrue (std::chrono::steady_clock::now() - start < std::chrono::milliseconds (100));
    }
};
