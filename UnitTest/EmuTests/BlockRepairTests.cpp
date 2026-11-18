#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "PartitionCard.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CommandLineOptions.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Archives/GzipFile.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/Blocks/BlockMapBuilder.h"
#include "Devices/Disk/Inspector/Blocks/BlockRepair.h"
#include "Devices/Disk/Inspector/Blocks/Dos33SectorLayout.h"
#include "Machines/Apple2/Common/HfsBTree.h"
#include "Machines/Apple2/Common/HfsVolume.h"
#include "Machines/Apple2/Common/HfsWriter.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Ui/DiskInspector/Blocks/BlockBrowser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairTests
//
//  The file system verdict and the repair of volumes of blocks. A ProDOS
//  volume of 10,000 blocks, whose bitmap is three blocks long: a block a
//  file uses marked free, a block in no file marked used, a directory's
//  file count, a block in two files, the changes chosen, and the repair
//  written to a copy in block mode, with its finding on the Volume level.
//  An 800K Apple Pascal volume's file count, an 800K HFS volume's bitmap
//  and free block count, and a DOS 3.3 volume of 50 tracks of 32 sectors,
//  alone and as half of a UniDOS disk.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockRepairTests)
{
public:

    static constexpr size_t    kBlockBytes   = 512;
    static constexpr size_t    kBlocks       = 10000;
    static constexpr uint32_t  kKeyBlock     = 2;
    static constexpr uint32_t  kBitmapAt     = 0x27;
    static constexpr uint32_t  kBitsPerBlock = 4096;
    static constexpr uint32_t  kFarBlock     = 9000;
    static constexpr uint32_t  kFileCountAt  = 0x25;
    static constexpr uint32_t  kKeyPointer   = 0x11;
    static constexpr int64_t   kTime         = 1'700'000'000;

    //  BIG with SEED, GAMES/HELLO and GAMES/OTHER.
    static vector<Byte> MakeVolume()
    {
        vector<Byte>  buffer (kBlocks * kBlockBytes, 0);
        vector<Byte>  next;
        FilePayload   payload;



        AssertSucceeded (ProDosSkeleton::Write (buffer, "BIG"));

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
        buffer = next;

        payload.bytes.assign (100, 0x42);
        AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("GAMES/OTHER"), payload, next));

        return next;
    }


    static uint32_t GetWord (const vector<Byte> & buffer, uint32_t block, uint32_t offset)
    {
        size_t  at = block * kBlockBytes + offset;



        return buffer[at] | ((uint32_t) buffer[at + 1] << 8);
    }


    //  A block's bit in the bitmap, set when the block is free.
    static void SetFree (vector<Byte> & buffer, uint32_t block, bool isFree)
    {
        uint32_t  bitmap = GetWord (buffer, kKeyBlock, kBitmapAt);
        size_t    at     = (bitmap + block / kBitsPerBlock) * kBlockBytes + (block % kBitsPerBlock) / 8;
        Byte      mask   = (Byte) (0x80 >> (block % 8));



        buffer[at] = isFree ? (Byte) (buffer[at] | mask) : (Byte) (buffer[at] & ~mask);
    }


    static BlockMap BuildMap (const vector<Byte> & buffer)
    {
        BlockMap    map;
        BlockImage  image (buffer);



        BlockMapBuilder::Build (image, map);

        return map;
    }


    static int FindEntry (const BlockMap & map, const std::wstring & path)
    {
        auto  found = std::find (map.files.begin(), map.files.end(), path);



        return found == map.files.end() ? -1 : (int) (found - map.files.begin());
    }


    //  The offset of a file's entry in its directory block.
    static size_t FindEntryOffset (const vector<Byte> & buffer, const BlockMap & map, int file)
    {
        const BlockFileEntry  & entry = map.entries[file];
        std::string             name  = TextEncoding::WideToUtf8 (entry.name);
        size_t                  start = entry.entryBlock * kBlockBytes;



        for (size_t at = start + 4; at + 0x27 <= start + kBlockBytes; at += 0x27)
        {
            if ((buffer[at] & 0x0F) == name.size() && std::equal (name.begin(), name.end(), buffer.begin() + at + 1))
            {
                return at;
            }
        }

        Assert::Fail (L"The entry wasn't found");
        return 0;
    }


    static void ApplyPlan (const RepairPlan & plan, vector<Byte> & ioBuffer)
    {
        for (const RepairBlock & block : plan.blocks)
        {
            std::copy (block.bytes.begin(), block.bytes.end(), ioBuffer.begin() + block.block * kBlockBytes);
        }
    }


    static bool HasKey (const RepairPlan & plan, const std::wstring & key)
    {
        return std::any_of (plan.changes.begin(), plan.changes.end(), [&key] (const RepairChange & change) { return change.key == key; });
    }


    TEST_METHOD (Plan_CleanVolume_IsOkAndHasNothingToRepair)
    {
        vector<Byte>  buffer = MakeVolume();
        BlockMap      map    = BuildMap (buffer);
        RepairPlan    plan   = BlockRepair::Plan (BlockImage (buffer), map, 0, {});



        Assert::IsTrue  (BlockRepair::GetVerdict (map) == FileSystemVerdict::Ok);
        Assert::IsTrue  (map.counts.empty());
        Assert::IsFalse (plan.canRepair);
        Assert::AreEqual (std::wstring (L"The file system has no damage."), plan.reason);
    }


    TEST_METHOD (Plan_BitmapDamage_IsRepairedAcrossTheBitmapsBlocks)
    {
        vector<Byte>  buffer = MakeVolume();
        BlockMap      map    = BuildMap (buffer);
        int           hello  = FindEntry (map, L"GAMES/HELLO");
        uint32_t      key    = map.entries[hello].keyBlock;
        RepairPlan    plan;



        SetFree (buffer, key, true);
        SetFree (buffer, kKeyBlock, true);
        SetFree (buffer, kFarBlock, false);

        map = BuildMap (buffer);
        Assert::IsTrue (BlockRepair::GetVerdict (map) == FileSystemVerdict::Bad);

        plan = BlockRepair::Plan (BlockImage (buffer), map, 0, {});

        Assert::IsTrue (plan.canRepair, plan.reason.c_str());
        Assert::IsTrue (HasKey (plan, L"used:GAMES/HELLO"));
        Assert::IsTrue (HasKey (plan, L"structures"));
        Assert::IsTrue (HasKey (plan, L"unowned"));

        ApplyPlan (plan, buffer);
        map = BuildMap (buffer);

        Assert::IsTrue (BlockRepair::GetVerdict (map) == FileSystemVerdict::Ok, L"the repaired volume is OK");
    }


    TEST_METHOD (Plan_FileCountDiffers_IsFoundAndSetToTheEntries)
    {
        vector<Byte>  buffer = MakeVolume();
        BlockMap      map    = BuildMap (buffer);
        uint32_t      games  = map.entries[FindEntry (map, L"GAMES")].keyBlock;
        RepairPlan    plan;



        buffer[games * kBlockBytes + kFileCountAt] = 5;

        map = BuildMap (buffer);

        Assert::AreEqual ((size_t) 1, map.counts.size());
        Assert::AreEqual (std::wstring (L"GAMES: the file count is 5, but the directory has 2 files"), map.counts[0].finding);

        plan = BlockRepair::Plan (BlockImage (buffer), map, 0, {});

        Assert::IsTrue   (plan.canRepair, plan.reason.c_str());
        Assert::AreEqual (std::wstring (L"Set the file count of GAMES to 2"), plan.changes[0].text);

        ApplyPlan (plan, buffer);

        Assert::AreEqual (2u, GetWord (buffer, games, kFileCountAt));
        Assert::IsTrue   (BuildMap (buffer).counts.empty());
    }


    TEST_METHOD (Plan_BlockInTwoFiles_IsNotRepaired)
    {
        vector<Byte>  buffer = MakeVolume();
        BlockMap      map    = BuildMap (buffer);
        int           hello  = FindEntry (map, L"GAMES/HELLO");
        int           other  = FindEntry (map, L"GAMES/OTHER");
        size_t        at     = FindEntryOffset (buffer, map, other);
        uint32_t      key    = map.entries[hello].keyBlock;
        RepairPlan    plan;



        buffer[at + kKeyPointer]     = (Byte) key;
        buffer[at + kKeyPointer + 1] = (Byte) (key >> 8);
        SetFree (buffer, kFarBlock, false);

        map  = BuildMap (buffer);
        plan = BlockRepair::Plan (BlockImage (buffer), map, 0, {});

        Assert::IsFalse (plan.canRepair);
        Assert::IsTrue  (plan.reason.starts_with (L"Files ") && plan.reason.find (L" are cross-linked at block ") != std::wstring::npos, plan.reason.c_str());
        Assert::IsTrue  (plan.blocks.empty());
    }


    TEST_METHOD (PlanChosen_OnlyTheChosenChangeIsMade)
    {
        vector<Byte>  buffer = MakeVolume();
        BlockMap      map;
        RepairPlan    plan;



        SetFree (buffer, kKeyBlock, true);
        SetFree (buffer, kFarBlock, false);

        map  = BuildMap (buffer);
        plan = BlockRepair::PlanChosen (BlockImage (buffer), map, 0, {}, { L"unowned" });

        Assert::IsTrue   (plan.canRepair, plan.reason.c_str());
        Assert::AreEqual ((size_t) 1, plan.changes.size());
        Assert::AreEqual (std::wstring (L"unowned"), plan.changes[0].key);

        ApplyPlan (plan, buffer);
        map = BuildMap (buffer);

        Assert::IsTrue (map.blocks[kFarBlock].role != SectorRole::AllocatedUnowned, L"the block in no file is free");
        Assert::IsTrue (map.blocks[kKeyBlock].role == SectorRole::OwnedMarkedFree, L"the volume directory's block is marked free");
        Assert::IsTrue (BlockRepair::GetVerdict (map) == FileSystemVerdict::Bad);
    }


    TEST_METHOD (Model_RepairIsWrittenToACopyAndTheImageIsUnchanged)
    {
        FakeDiskFileIo                   io;
        BlockInspectorModel              model;
        std::unique_ptr<BlockImageFile>  file;
        std::wstring                     error;
        vector<Byte>                     buffer = MakeVolume();
        vector<Byte>                     damaged;
        RepairPlan                       plan;
        BlockBrowser                     browser (&model);
        BlockListing                     listing;
        BlockPlace                       place;
        bool                             isListed = false;



        buffer[kKeyBlock * kBlockBytes + kFileCountAt] = 9;
        SetFree (buffer, kFarBlock, false);
        damaged = buffer;

        io.files["vol.po"]  = buffer;
        io.stamps["vol.po"] = { buffer.size(), kTime };

        AssertSucceeded (BlockImageFile::Open (io, "vol.po", file, error));
        AssertSucceeded (model.Open (std::move (file)));

        place.kind   = BlockPlace::Kind::Volume;
        place.volume = 0;
        listing      = browser.BuildListing (place);

        for (const BlockListGroup & group : listing.groups)
        {
            for (const BlockListRow & row : group.rows)
            {
                isListed = isListed || (group.title == L"Findings" && row.cells[0] == L"The volume directory's file count is 9, but it has 2 files");
            }
        }

        Assert::IsTrue (isListed, L"the Volume level lists the count");

        plan = model.PlanRepair (0, {});
        Assert::IsTrue (plan.canRepair, plan.reason.c_str());

        //  A copy of the image, as the window makes before writing into it.
        io.files["vol (repaired).po"]  = damaged;
        io.stamps["vol (repaired).po"] = { damaged.size(), kTime };

        AssertSucceeded (model.WriteRepairTo (io, "vol (repaired).po", 0, plan, false, error));

        Assert::IsTrue (BlockRepair::GetVerdict (BuildMap (io.files["vol (repaired).po"])) == FileSystemVerdict::Ok, L"the copy is repaired");
        Assert::IsTrue (io.files["vol.po"] == damaged, L"the image isn't written");

        //  A volume written alone, as for a card's partition.
        AssertSucceeded (model.WriteRepairTo (io, "partition.po", 0, plan, true, error));

        Assert::AreEqual (kBlocks * kBlockBytes, io.files["partition.po"].size());
        Assert::IsTrue   (BlockRepair::GetVerdict (BuildMap (io.files["partition.po"])) == FileSystemVerdict::Ok);
    }


    TEST_METHOD (Pascal_FileCountDiffers_IsSetToTheEntries)
    {
        constexpr size_t  kPascalCountAt = kKeyBlock * kBlockBytes + 16;



        vector<Byte>  buffer (1600 * kBlockBytes, 0);
        vector<Byte>  next;
        FilePayload   payload;
        BlockMap      map;
        RepairPlan    plan;



        PascalVolume::WriteBlank ("BLOCKS", PascalVolume::MakeDate (1985, 1, 1), buffer);

        payload.bytes.assign (2000, 0x5A);
        payload.type = PascalVolume::kKindData;
        AssertSucceeded (PascalVolume (buffer).Write (FilePath::Parse ("A.DATA"), payload, next));
        AssertSucceeded (PascalVolume (next).Write (FilePath::Parse ("B.DATA"), payload, buffer));

        Assert::IsTrue (BlockRepair::GetVerdict (BuildMap (buffer)) == FileSystemVerdict::Ok);

        buffer[kPascalCountAt] = 3;
        map = BuildMap (buffer);

        Assert::AreEqual ((size_t) 1, map.counts.size());
        Assert::AreEqual (std::wstring (L"The directory's file count is 3, but it has 2 files"), map.counts[0].finding);

        plan = BlockRepair::Plan (BlockImage (buffer), map, 0, {});

        Assert::IsTrue   (plan.canRepair, plan.reason.c_str());
        Assert::AreEqual (std::wstring (L"Set the directory's file count to 2"), plan.changes[0].text);

        ApplyPlan (plan, buffer);

        Assert::AreEqual ((Byte) 2, buffer[kPascalCountAt]);
        Assert::IsTrue   (BlockRepair::GetVerdict (BuildMap (buffer)) == FileSystemVerdict::Ok);
    }


    //  An HFS allocation block's bit in the volume bitmap, set when it's in use.
    static void SetHfsUsed (vector<Byte> & buffer, uint32_t allocation, bool isUsed)
    {
        uint32_t  bitmap = (buffer[kKeyBlock * kBlockBytes + 14] << 8) | buffer[kKeyBlock * kBlockBytes + 15];
        size_t    at     = (bitmap + allocation / kBitsPerBlock) * kBlockBytes + (allocation % kBitsPerBlock) / 8;
        Byte      mask   = (Byte) (0x80 >> (allocation % 8));



        buffer[at] = isUsed ? (Byte) (buffer[at] | mask) : (Byte) (buffer[at] & ~mask);
    }


    static vector<Byte> MakeHfs()
    {
        vector<Byte>  image;
        vector<Byte>  next;
        FilePayload   payload;



        AssertSucceeded (HfsWriter::Format (image, 1600, "Untitled"));

        payload.bytes.assign (3000, 0x5A);
        payload.type = ProDosVolume::kTypeBinary;
        AssertSucceeded (HfsVolume (image).Write (FilePath::Parse ("Doc"), payload, next));

        return next;
    }


    TEST_METHOD (Hfs_FileMarkedFreeInTheBitmap_IsMarkedUsed)
    {
        vector<Byte>  buffer = MakeHfs();
        BlockMap      map    = BuildMap (buffer);
        int           doc    = FindEntry (map, L"Doc");
        uint32_t      first  = map.GetBlocksOf (doc)[0];
        RepairPlan    plan;



        Assert::IsTrue (BlockRepair::GetVerdict (map) == FileSystemVerdict::Ok, L"a new HFS volume is OK");
        Assert::IsTrue (map.counts.empty(), L"its free block count matches its bitmap");

        SetHfsUsed (buffer, (first - map.allocationStart) / map.blocksPerAllocation, false);
        SetHfsUsed (buffer, 1500 / map.blocksPerAllocation, true);

        map  = BuildMap (buffer);
        plan = BlockRepair::Plan (BlockImage (buffer), map, 0, {});

        Assert::IsTrue (BlockRepair::GetVerdict (map) == FileSystemVerdict::Bad);
        Assert::IsTrue (plan.canRepair, plan.reason.c_str());
        Assert::IsTrue (HasKey (plan, L"used:Doc"));
        Assert::IsTrue (HasKey (plan, L"unowned"));

        ApplyPlan (plan, buffer);

        Assert::IsTrue (BlockRepair::GetVerdict (BuildMap (buffer)) == FileSystemVerdict::Ok, L"the bitmap and the free block count agree again");
    }


    TEST_METHOD (Hfs_FreeBlockCountDiffers_IsSetFromTheBitmap)
    {
        vector<Byte>  buffer = MakeHfs();
        size_t        at     = kKeyBlock * kBlockBytes + 34;
        Word          free   = (Word) ((buffer[at] << 8) | buffer[at + 1]);
        BlockMap      map;
        RepairPlan    plan;



        buffer[at]     = 0;
        buffer[at + 1] = 7;

        map = BuildMap (buffer);

        Assert::AreEqual ((size_t) 1, map.counts.size());
        Assert::IsTrue   (map.counts[0].isFreeCount);

        plan = BlockRepair::Plan (BlockImage (buffer), map, 0, {});

        Assert::IsTrue (plan.canRepair, plan.reason.c_str());
        Assert::IsTrue (HasKey (plan, L"freecount"));

        ApplyPlan (plan, buffer);

        Assert::AreEqual (free, (Word) ((buffer[at] << 8) | buffer[at + 1]));
        Assert::IsTrue   (BuildMap (buffer).counts.empty());
    }


    //  The extents overflow file rebuilt with one record: the catalog's
    //  last allocation block as the extent that continues the first.
    static void WriteCatalogOverflow (vector<Byte> & buffer, uint16_t start, uint16_t count, bool isListed)
    {
        constexpr Byte      kCatalogId  = 4;
        size_t              mdb         = kKeyBlock * kBlockBytes;
        uint32_t            bytes       = ((uint32_t) buffer[mdb + 130] << 24) | ((uint32_t) buffer[mdb + 131] << 16) | ((uint32_t) buffer[mdb + 132] << 8) | buffer[mdb + 133];
        uint32_t            allocation  = ((uint32_t) buffer[mdb + 20] << 24) | ((uint32_t) buffer[mdb + 21] << 16) | ((uint32_t) buffer[mdb + 22] << 8) | buffer[mdb + 23];
        uint32_t            first       = (uint32_t) ((buffer[mdb + 28] << 8) | buffer[mdb + 29]);
        uint32_t            extents     = (uint32_t) ((buffer[mdb + 134] << 8) | buffer[mdb + 135]);
        vector<Byte>        nodes;
        HfsBTree::Record    record;



        record.key  = { 7, 0, 0, 0, 0, kCatalogId, (Byte) ((count - 1) >> 8), (Byte) (count - 1) };
        record.data = { (Byte) ((start + count - 1) >> 8), (Byte) (start + count - 1), 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };

        AssertSucceeded (HfsBTree::Build (HfsBTree::Kind::Extents, isListed ? vector<HfsBTree::Record> { record } : vector<HfsBTree::Record>(), bytes / kBlockBytes, nodes));

        std::copy (nodes.begin(), nodes.end(), buffer.begin() + (ptrdiff_t) ((first * kBlockBytes) + (size_t) extents * allocation));
    }


    TEST_METHOD (Hfs_CatalogInTheOverflowFile_IsFollowed)
    {
        vector<Byte>  buffer  = MakeHfs();
        size_t        catalog = kKeyBlock * kBlockBytes + 150;
        uint16_t      start   = (uint16_t) ((buffer[catalog] << 8) | buffer[catalog + 1]);
        uint16_t      count   = (uint16_t) ((buffer[catalog + 2] << 8) | buffer[catalog + 3]);
        size_t        nodes   = BuildMap (buffer).catalogNodes.size();
        BlockMap      map;
        RepairPlan    plan;



        Assert::IsTrue (count >= 2, L"the catalog has more than one allocation block");
        Assert::IsTrue (buffer[catalog + 6] == 0 && buffer[catalog + 7] == 0, L"the catalog has one extent");

        buffer[catalog + 2] = (Byte) ((count - 1) >> 8);
        buffer[catalog + 3] = (Byte) (count - 1);
        WriteCatalogOverflow (buffer, start, count, true);

        map = BuildMap (buffer);

        Assert::AreEqual (nodes, map.catalogNodes.size());
        Assert::IsTrue   (BlockRepair::GetVerdict (map) == FileSystemVerdict::Ok);

        WriteCatalogOverflow (buffer, start, count, false);

        map  = BuildMap (buffer);
        plan = BlockRepair::Plan (BlockImage (buffer), map, 0, {});

        Assert::IsFalse  (map.isCatalogComplete, L"the catalog's last allocation block isn't listed");
        Assert::AreEqual (std::wstring (L"Some of the volume's directories can't be read."), plan.reason);
    }


    //  A DOS 3.3 sector's bit in the VTOC of a volume of 50 tracks of 32
    //  sectors, set when the sector is free.
    static void SetDosFree (vector<Byte> & sectors, int track, int sector, bool isFree)
    {
        size_t  at   = (size_t) (17 * 32) * 256 + 0x38 + track * 4 + (3 - sector / 8);
        Byte    mask = (Byte) (1 << (sector % 8));



        sectors[at] = isFree ? (Byte) (sectors[at] | mask) : (Byte) (sectors[at] & ~mask);
    }


    TEST_METHOD (Dos33Sectors_BitmapDamage_IsRepaired)
    {
        vector<Byte>  sectors = PartitionCard::MakeDos (50, 32);
        BlockMap      map;
        RepairPlan    plan;



        Dos33SectorLayout::Walk (sectors, 50, 32, map);
        Assert::IsTrue (BlockRepair::GetVerdict (map) == FileSystemVerdict::Ok, L"a new DOS 3.3 volume is OK");

        SetDosFree (sectors, 20, 5, false);
        SetDosFree (sectors, 17, 0, true);

        Dos33SectorLayout::Walk (sectors, 50, 32, map);
        plan = BlockRepair::PlanSectors (sectors, map, 32, {});

        Assert::IsTrue (plan.canRepair, plan.reason.c_str());
        Assert::IsTrue (HasKey (plan, L"unowned"));
        Assert::IsTrue (HasKey (plan, L"structures"));
        Assert::IsTrue (plan.blocks.empty());
        Assert::AreEqual ((size_t) 1, plan.sectors.size(), L"the VTOC is the one sector changed");

        for (const RepairSector & sector : plan.sectors)
        {
            std::copy (sector.bytes.begin(), sector.bytes.end(), sectors.begin() + (ptrdiff_t) (sector.track * 32 + sector.sector) * 256);
        }

        Dos33SectorLayout::Walk (sectors, 50, 32, map);
        Assert::IsTrue (BlockRepair::GetVerdict (map) == FileSystemVerdict::Ok);
    }


    TEST_METHOD (Model_UniDosVolume_IsRepairedInItsHalfOfTheDisk)
    {
        FakeDiskFileIo                   io;
        BlockInspectorModel              model;
        std::unique_ptr<BlockImageFile>  file;
        std::wstring                     error;
        vector<Byte>                     disk  = PartitionCard::MakeUniDosDisk();
        size_t                           vtoc  = (size_t) (17 * 32) * 256;
        RepairPlan                       plan;
        BlockMap                         map;



        disk[vtoc + 0x38 + 20 * 4 + 3] = (Byte) (disk[vtoc + 0x38 + 20 * 4 + 3] & ~(1 << 5));

        io.files["unidos.po"]           = disk;
        io.stamps["unidos.po"]          = { disk.size(), kTime };
        io.files["unidos (repaired).po"]  = disk;
        io.stamps["unidos (repaired).po"] = { disk.size(), kTime };

        AssertSucceeded (BlockImageFile::Open (io, "unidos.po", file, error));
        AssertSucceeded (model.Open (std::move (file)));

        Assert::IsTrue (model.IsSectorVolume (0));

        plan = model.PlanRepair (0, {});
        Assert::IsTrue (plan.canRepair, plan.reason.c_str());

        AssertSucceeded (model.WriteRepairTo (io, "unidos (repaired).po", 0, plan, false, error));

        Assert::IsTrue (io.files["unidos.po"] == disk, L"the image isn't written");
        Assert::AreNotEqual (disk[vtoc + 0x38 + 20 * 4 + 3], io.files["unidos (repaired).po"][vtoc + 0x38 + 20 * 4 + 3], L"the copy's VTOC is repaired");

        AssertSucceeded (model.WriteRepairTo (io, "volume.dsk", 0, plan, true, error));
        Assert::AreEqual ((size_t) 50 * 32 * 256, io.files["volume.dsk"].size());

        Dos33SectorLayout::Walk (io.files["volume.dsk"], 50, 32, map);
        Assert::IsTrue (BlockRepair::GetVerdict (map) == FileSystemVerdict::Ok, L"the volume written alone is repaired");
    }

    static DiskCommandResult RunCli (FakeDiskFileIo & io, CommandLineOptions::DiskOptions::Command command, const char * image)
    {
        CommandLineOptions  options = DiskOperations::MakeOptions (command, image);
        DiskCommandRunner   runner (io);



        return runner.Run (options);
    }


    //  A block of SEED marked free and a block in no file marked used.
    static vector<Byte> MakeDamagedVolume (uint32_t & outSeedBlock)
    {
        vector<Byte>  buffer = MakeVolume();
        BlockMap      map    = BuildMap (buffer);



        outSeedBlock = map.GetBlocksOf (FindEntry (map, L"SEED"))[0];

        SetFree (buffer, outSeedBlock, true);
        SetFree (buffer, kFarBlock, false);

        return buffer;
    }


    TEST_METHOD (Cli_Inspect_ListsEachVolumeAndItsFindings)
    {
        FakeDiskFileIo     io;
        uint32_t           seed   = 0;
        DiskCommandResult  result;



        io.files["C:\\Disks\\Big.po"]  = MakeDamagedVolume (seed);
        io.stamps["C:\\Disks\\Big.po"] = { kBlocks * kBlockBytes, kTime };
        result                         = RunCli (io, CommandLineOptions::DiskOptions::Command::Inspect, "C:\\Disks\\Big.po");

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::Utf8ToWide (result.diagnostics).c_str());
        Assert::IsTrue   (result.output.starts_with ("10000 blocks; volume 1, ProDOS \"BIG\": Bad\n"), TextEncoding::Utf8ToWide (result.output).c_str());
        Assert::IsTrue   (result.output.contains (std::format ("1\t{}\tIn a file, marked free\n", seed)), TextEncoding::Utf8ToWide (result.output).c_str());
        Assert::IsTrue   (result.output.contains (std::format ("1\t{}\tMarked used, in no file\n", kFarBlock)));
    }


    TEST_METHOD (Cli_Repair_WritesOneCopyAndKeepsBlocksInNoFileMarkedUsed)
    {
        FakeDiskFileIo     io;
        uint32_t           seed   = 0;
        vector<Byte>       damage = MakeDamagedVolume (seed);
        DiskCommandResult  result;
        BlockMap           map;



        io.files["C:\\Disks\\Big.po"]               = damage;
        io.stamps["C:\\Disks\\Big.po"]              = { damage.size(), kTime };
        io.stamps["C:\\Disks\\Big (repaired).po"]   = { damage.size(), kTime };
        result                                      = RunCli (io, CommandLineOptions::DiskOptions::Command::Repair, "C:\\Disks\\Big.po");

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::Utf8ToWide (result.diagnostics).c_str());
        Assert::IsTrue   (result.output.contains ("Big.po: volume 1, ProDOS \"BIG\": file system: Bad\n"), TextEncoding::Utf8ToWide (result.output).c_str());
        Assert::IsTrue   (result.output.contains ("Big.po: volume 1, ProDOS \"BIG\": In a file, marked free: 1 block\n"));
        Assert::IsTrue   (result.output.contains ("Big.po: volume 1, ProDOS \"BIG\": change: Mark 1 block of SEED as used in the bitmap\n"));
        Assert::IsTrue   (result.output.contains ("Big.po: volume 1, ProDOS \"BIG\": not changed: "));
        Assert::IsTrue   (result.output.contains ("repaired copy written to C:\\Disks\\Big (repaired).po\n"));
        Assert::IsTrue   (io.files["C:\\Disks\\Big.po"] == damage, L"the image isn't written");

        map = BuildMap (io.files["C:\\Disks\\Big (repaired).po"]);

        Assert::IsTrue (map.blocks[seed].role != SectorRole::OwnedMarkedFree, L"SEED's block is marked used");
        Assert::IsTrue (map.blocks[kFarBlock].role == SectorRole::AllocatedUnowned, L"the block in no file stays marked used");
    }

    TEST_METHOD (Cli_Repair_OfAGzipImage_WritesARecompressedCopy)
    {
        FakeDiskFileIo     io;
        uint32_t           seed   = 0;
        vector<Byte>       gzip;
        vector<Byte>       copy;
        std::string        name;
        std::string        error;
        DiskCommandResult  result;
        BlockMap           map;



        GzipFile::Build (MakeDamagedVolume (seed), "Big.po", gzip);

        io.files["C:\\Disks\\Big.po.gz"]  = gzip;
        io.stamps["C:\\Disks\\Big.po.gz"] = { gzip.size(), kTime };
        result                            = RunCli (io, CommandLineOptions::DiskOptions::Command::Repair, "C:\\Disks\\Big.po.gz");

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::Utf8ToWide (result.diagnostics).c_str());
        Assert::IsTrue   (result.output.contains ("repaired copy written to C:\\Disks\\Big (repaired).po.gz\n"), TextEncoding::Utf8ToWide (result.output).c_str());
        Assert::IsTrue   (io.files["C:\\Disks\\Big.po.gz"] == gzip, L"the image isn't written");

        AssertSucceeded (GzipFile::Expand (io.files["C:\\Disks\\Big (repaired).po.gz"], copy, name, error));
        map = BuildMap (copy);

        Assert::IsTrue (map.blocks[seed].role != SectorRole::OwnedMarkedFree, L"SEED's block is marked used in the copy");
        Assert::IsTrue (io.files["C:\\Disks\\Big (repaired).po.gz"].size() < copy.size() / 20, L"the copy is compressed");
    }
};
