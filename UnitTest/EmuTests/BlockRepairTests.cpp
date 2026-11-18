#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/Blocks/BlockMapBuilder.h"
#include "Devices/Disk/Inspector/Blocks/BlockRepair.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Ui/DiskInspector/Blocks/BlockBrowser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairTests
//
//  The file system verdict and the repair of a ProDOS volume of blocks,
//  on a volume of 10,000 blocks, whose bitmap is three blocks long: a
//  block a file uses marked free, a block in no file marked used, a
//  directory's file count, a block in two files, the changes chosen, and
//  the repair written to a copy in block mode, with its finding on the
//  Volume level.
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
        Assert::AreEqual (std::wstring (L"GAMES: expected a directory file count of 2, but found 5"), map.counts[0].finding);

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
        Assert::IsTrue  (plan.reason.starts_with (L"Repairing this would change a file's data: block "), plan.reason.c_str());
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
                isListed = isListed || (group.title == L"Findings" && row.cells[0] == L"Expected a volume directory file count of 2, but found 9");
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
};
