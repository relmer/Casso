#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/Inspector/Blocks/BlockMapBuilder.h"
#include "Machines/Apple2/Common/HfsVolume.h"
#include "Machines/Apple2/Common/HfsWriter.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapTests
//
//  The block map and the annotations of ProDOS, HFS and Pascal volumes the
//  volume layer writes in memory: each block's role and files, the fields
//  a block's structure has and the blocks they link to, and the roles of
//  blocks where a damaged free map disagrees with the catalog.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockMapTests)
{
public:

    static constexpr size_t  kBlockBytes  = 512;
    static constexpr size_t  k800KBlocks  = 1600;
    static constexpr size_t  kSeedBytes   = 300;
    static constexpr size_t  kSapBytes    = 70'000;
    static constexpr size_t  kTreeBytes   = 200'000;
    static constexpr Byte    kFill        = 0x5A;
    static constexpr Word    kLoadAddress = 0x2000;

    static FilePayload MakePayload (size_t bytes)
    {
        FilePayload  payload;



        payload.bytes.assign (bytes, kFill);
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = kLoadAddress;
        payload.hasLoadAddress = true;

        return payload;
    }


    //  An 800K ProDOS volume with a seedling, a sapling, a tree and a
    //  subdirectory with a file in it.
    static vector<Byte> MakeProDos()
    {
        vector<Byte>  buffer (k800KBlocks * kBlockBytes, 0);
        vector<Byte>  next;



        AssertSucceeded (ProDosSkeleton::Write (buffer, "BLOCKS"));

        for (const auto & [path, bytes] : { std::pair<LPCSTR, size_t> ("SEED", kSeedBytes), std::pair<LPCSTR, size_t> ("SAP", kSapBytes), std::pair<LPCSTR, size_t> ("TREE", kTreeBytes) })
        {
            AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse (path), MakePayload (bytes), next));
            buffer = next;
        }

        AssertSucceeded (ProDosVolume (buffer).CreateDirectory (FilePath::Parse ("DIR"), next));
        buffer = next;

        AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("DIR/INNER"), MakePayload (kSeedBytes), next));

        return next;
    }


    static BlockMap Build (const vector<Byte> & bytes)
    {
        BlockImage  image (bytes);
        BlockMap    map;



        BlockMapBuilder::Build (image, map);

        return map;
    }


    static int FindFile (const BlockMap & map, const std::wstring & path)
    {
        auto  found = std::find (map.files.begin(), map.files.end(), path);



        Assert::IsTrue (found != map.files.end(), (L"no file " + path).c_str());

        return (int) (found - map.files.begin());
    }


    //  The first of a file's blocks with this structure.
    static uint32_t FindBlock (const BlockMap & map, int file, BlockStructure structure)
    {
        for (uint32_t block : map.GetBlocksOf (file))
        {
            if (map.blocks[block].structure == structure)
            {
                return block;
            }
        }

        Assert::Fail (L"the file has no block of that structure");

        return 0;
    }


    static vector<BlockField> Annotate (const BlockMap & map, const vector<Byte> & bytes, uint32_t block)
    {
        vector<BlockField>  fields;



        BlockMapBuilder::Annotate (map, block, BlockBytes (bytes).subspan (block * kBlockBytes, kBlockBytes), fields);

        return fields;
    }


    static const BlockField & FindField (const vector<BlockField> & fields, const std::wstring & label)
    {
        for (const BlockField & field : fields)
        {
            if (field.label == label)
            {
                return field;
            }
        }

        Assert::Fail ((L"no field " + label).c_str());

        return fields[0];
    }


    static void SetBitmapBit (vector<Byte> & bytes, uint32_t block, bool isFree)
    {
        constexpr size_t  kBitmapBlock = 6;

        Byte &  at   = bytes[kBitmapBlock * kBlockBytes + block / 8];
        Byte    mask = (Byte) (0x80 >> (block % 8));



        at = isFree ? (Byte) (at | mask) : (Byte) (at & ~mask);
    }


    TEST_METHOD (ProDos_MapsBootDirectoryAndBitmap)
    {
        vector<Byte>  bytes = MakeProDos();
        BlockMap      map   = Build (bytes);



        Assert::IsTrue (map.fileSystem == BlockFileSystem::ProDos);
        Assert::AreEqual (std::wstring (L"BLOCKS"), map.volumeName);
        Assert::AreEqual ((uint32_t) k800KBlocks, map.GetBlockCount());

        Assert::IsTrue (map.blocks[0].role == SectorRole::BootArea);
        Assert::IsTrue (map.blocks[1].role == SectorRole::BootArea);
        Assert::IsTrue (map.blocks[2].role == SectorRole::VtocOrKeyBlock);
        Assert::IsTrue (map.blocks[2].structure == BlockStructure::ProDosDirectory);
        Assert::IsTrue (map.blocks[3].role == SectorRole::CatalogOrDirectory);
        Assert::IsTrue (map.blocks[5].role == SectorRole::CatalogOrDirectory);
        Assert::IsTrue (map.blocks[6].role == SectorRole::VolumeBitmap);
        Assert::IsTrue (map.blocks[k800KBlocks - 1].role == SectorRole::Free);
        Assert::IsTrue (map.isCatalogComplete);
    }


    TEST_METHOD (ProDos_MapsEachStorageTypeToItsFile)
    {
        vector<Byte>  bytes = MakeProDos();
        BlockMap      map   = Build (bytes);
        int           seed  = FindFile (map, L"SEED");
        int           sap   = FindFile (map, L"SAP");
        int           tree  = FindFile (map, L"TREE");
        int           dir   = FindFile (map, L"DIR");
        int           inner = FindFile (map, L"DIR/INNER");



        Assert::AreEqual ((size_t) 1, map.GetBlocksOf (seed).size());
        Assert::AreEqual ((size_t) (1 + (kSapBytes + kBlockBytes - 1) / kBlockBytes), map.GetBlocksOf (sap).size());

        Assert::IsTrue (map.blocks[FindBlock (map, sap, BlockStructure::ProDosIndex)].role == SectorRole::IndexBlock);
        Assert::IsTrue (map.blocks[FindBlock (map, tree, BlockStructure::ProDosMasterIndex)].role == SectorRole::IndexBlock);
        Assert::IsTrue (map.blocks[FindBlock (map, dir, BlockStructure::ProDosDirectory)].role == SectorRole::Subdirectory);
        Assert::IsTrue (map.blocks[FindBlock (map, inner, BlockStructure::FileData)].role == SectorRole::FileData);

        Assert::AreEqual (std::wstring (L"SAP"), map.DescribeOwners (FindBlock (map, sap, BlockStructure::FileData)));
    }


    TEST_METHOD (ProDos_AnnotatesTheVolumeHeaderAndLinksItsPointers)
    {
        vector<Byte>        bytes  = MakeProDos();
        BlockMap            map    = Build (bytes);
        vector<BlockField>  fields = Annotate (map, bytes, 2);



        Assert::AreEqual ((int64_t) 6, FindField (fields, L"Volume header: bitmap pointer").link);
        Assert::AreEqual ((int64_t) 3, FindField (fields, L"Next directory block").link);
        Assert::AreEqual (BlockField::kNoLink, FindField (fields, L"Previous directory block").link);
        Assert::AreEqual (std::wstring (L"\"BLOCKS\""), FindField (fields, L"Volume header: name").value);
    }


    TEST_METHOD (ProDos_LinksAnEntryToItsKeyBlockAndAnIndexToItsData)
    {
        vector<Byte>        bytes  = MakeProDos();
        BlockMap            map    = Build (bytes);
        int                 sap    = FindFile (map, L"SAP");
        uint32_t            index    = FindBlock (map, sap, BlockStructure::ProDosIndex);
        vector<BlockField>  fields   = Annotate (map, bytes, 2);
        bool                isLinked = false;



        for (const BlockField & field : fields)
        {
            isLinked = isLinked || (field.label.ends_with (L"SAP: key pointer") && field.link == (int64_t) index);
        }

        Assert::IsTrue (isLinked, L"the entry's key pointer links to its index block");

        fields = Annotate (map, bytes, index);

        Assert::AreEqual (256, FindField (fields, L"Data block 0").pairOffset);
        Assert::IsTrue (map.blocks[(size_t) FindField (fields, L"Data block 0").link].structure == BlockStructure::FileData);
        Assert::AreEqual (std::wstring (L"SAP"), map.DescribeOwners ((uint32_t) FindField (fields, L"Data block 0").link));
    }


    TEST_METHOD (ProDos_AnnotatesTheBitmap)
    {
        vector<Byte>        bytes  = MakeProDos();
        BlockMap            map    = Build (bytes);
        vector<BlockField>  fields = Annotate (map, bytes, 6);



        Assert::AreEqual ((size_t) 1, fields.size());
        Assert::AreEqual (std::wstring (L"Volume bitmap, blocks 0 to 4095"), fields[0].label);
    }


    TEST_METHOD (ProDos_FreeMapDisagreementsHaveTheirOwnRoles)
    {
        vector<Byte>  bytes   = MakeProDos();
        BlockMap      map     = Build (bytes);
        uint32_t      spare   = (uint32_t) k800KBlocks - 1;
        uint32_t      data    = FindBlock (map, FindFile (map, L"SEED"), BlockStructure::FileData);



        SetBitmapBit (bytes, spare, false);
        SetBitmapBit (bytes, data,  true);

        map = Build (bytes);

        Assert::IsTrue (map.blocks[spare].role == SectorRole::AllocatedUnowned);
        Assert::IsTrue (map.blocks[data].role  == SectorRole::OwnedMarkedFree);
    }


    TEST_METHOD (ProDos_ABlockInTwoFilesIsCrossLinked)
    {
        vector<Byte>  bytes = MakeProDos();
        BlockMap      map   = Build (bytes);
        uint32_t      seed  = FindBlock (map, FindFile (map, L"SEED"), BlockStructure::FileData);
        uint32_t      index = FindBlock (map, FindFile (map, L"SAP"), BlockStructure::ProDosIndex);
        int           sap   = 0;



        bytes[index * kBlockBytes]       = (Byte) (seed & 0xFF);
        bytes[index * kBlockBytes + 256] = (Byte) (seed >> 8);

        map = Build (bytes);
        sap = FindFile (map, L"SAP");

        Assert::IsTrue (map.blocks[seed].role == SectorRole::CrossLinked);
        Assert::AreEqual (std::wstring (L"SEED\nSAP"), map.DescribeOwners (seed));
        Assert::IsTrue (std::ranges::contains (map.GetBlocksOf (sap), seed));
    }


    TEST_METHOD (ProDos_ADirectoryLoopEndsTheWalk)
    {
        vector<Byte>  bytes = MakeProDos();
        BlockMap      map;



        bytes[3 * kBlockBytes + 2] = 2;
        bytes[3 * kBlockBytes + 3] = 0;

        map = Build (bytes);

        Assert::IsFalse (map.isCatalogComplete);
    }


    TEST_METHOD (Hfs_MapsTheVolumeStructuresAndAFile)
    {
        vector<Byte>  image;
        vector<Byte>  next;
        FilePayload   payload = MakePayload (3000);
        BlockMap      map;
        int           doc     = 0;
        uint32_t      data    = 0;



        AssertSucceeded (HfsWriter::Format (image, k800KBlocks, "Untitled"));
        AssertSucceeded (HfsVolume (image).Write (FilePath::Parse ("Doc"), payload, next));

        map = Build (next);
        doc = FindFile (map, L"Doc");

        Assert::IsTrue (map.fileSystem == BlockFileSystem::Hfs);
        Assert::AreEqual (std::wstring (L"Untitled"), map.volumeName);
        Assert::IsTrue (map.blocks[2].structure == BlockStructure::HfsMasterDirectory);
        Assert::IsTrue (map.blocks[k800KBlocks - 2].structure == BlockStructure::HfsAlternateMasterDirectory);
        Assert::IsTrue (map.blocks[0].role == SectorRole::BootArea);
        Assert::IsFalse (map.catalogNodes.empty());
        Assert::IsFalse (map.extentsNodes.empty());
        Assert::IsTrue (map.blocks[map.catalogNodes[0]].structure == BlockStructure::HfsNode);

        data = FindBlock (map, doc, BlockStructure::FileData);

        Assert::IsTrue (map.blocks[data].role == SectorRole::FileData);
        Assert::AreEqual (kFill, next[data * kBlockBytes]);
        Assert::IsTrue (map.isCatalogComplete);
    }


    TEST_METHOD (Hfs_AnnotatesTheMasterDirectoryAndTheCatalogHeader)
    {
        vector<Byte>        image;
        BlockMap            map;
        vector<BlockField>  fields;



        AssertSucceeded (HfsWriter::Format (image, k800KBlocks, "Untitled"));

        map    = Build (image);
        fields = Annotate (map, image, 2);

        Assert::AreEqual ((int64_t) map.catalogNodes[0], FindField (fields, L"Catalog file, extent 1 start").link);
        Assert::AreEqual ((int64_t) map.extentsNodes[0], FindField (fields, L"Extents overflow file, extent 1 start").link);
        Assert::AreEqual (std::wstring (L"\"Untitled\""), FindField (fields, L"Volume name").value);

        fields = Annotate (map, image, map.catalogNodes[0]);

        Assert::AreEqual (std::wstring (L"$01 (header)"), FindField (fields, L"Node type").value);
        Assert::AreEqual ((int64_t) map.catalogNodes[1], FindField (fields, L"First leaf node").link);
    }


    TEST_METHOD (Hfs_AnnotatesALeafRecordsExtentsAsLinks)
    {
        vector<Byte>        image;
        vector<Byte>        next;
        BlockMap            map;
        vector<BlockField>  fields;
        uint32_t            data     = 0;
        bool                isLinked = false;



        AssertSucceeded (HfsWriter::Format (image, k800KBlocks, "Untitled"));
        AssertSucceeded (HfsVolume (image).Write (FilePath::Parse ("Doc"), MakePayload (3000), next));

        map    = Build (next);
        data   = FindBlock (map, FindFile (map, L"Doc"), BlockStructure::FileData);
        fields = Annotate (map, next, map.catalogNodes[1]);

        Assert::AreEqual (std::wstring (L"$FF (leaf)"), FindField (fields, L"Node type").value);

        for (const BlockField & field : fields)
        {
            isLinked = isLinked || (field.label.ends_with (L"data fork, extent 1 start") && field.link == (int64_t) data);
        }

        Assert::IsTrue (isLinked, L"the file record's first extent links to its first block");
    }


    TEST_METHOD (Pascal_MapsTheDirectoryAndAFileWithoutAFreeMap)
    {
        vector<Byte>        bytes (k800KBlocks * kBlockBytes, 0);
        vector<Byte>        next;
        FilePayload         payload;
        BlockMap            map;
        int                 file = 0;
        vector<BlockField>  fields;



        PascalVolume::WriteBlank ("BLOCKS", PascalVolume::MakeDate (1985, 1, 1), bytes);

        payload.bytes.assign (2000, kFill);
        payload.type = PascalVolume::kKindData;

        AssertSucceeded (PascalVolume (bytes).Write (FilePath::Parse ("NOTES.DATA"), payload, next));

        map  = Build (next);
        file = FindFile (map, L"NOTES.DATA");

        Assert::IsTrue (map.fileSystem == BlockFileSystem::Pascal);
        Assert::IsFalse (map.hasFreeMap);
        Assert::IsTrue (map.blocks[2].role == SectorRole::VtocOrKeyBlock);
        Assert::IsTrue (map.blocks[5].structure == BlockStructure::PascalDirectory);
        Assert::AreEqual ((size_t) 4, map.GetBlocksOf (file).size());

        fields = Annotate (map, next, 2);

        Assert::AreEqual ((int64_t) map.GetBlocksOf (file)[0], FindField (fields, L"Entry 1 NOTES.DATA: first block").link);
    }


    TEST_METHOD (ApplyReport_MapsHfsAllocationBlocksToBlocks)
    {
        BlockMap               map;
        VolumeIntegrityReport  report;



        map.fileSystem          = BlockFileSystem::Hfs;
        map.allocationStart     = 10;
        map.blocksPerAllocation = 2;
        map.blocks.assign (20, BlockMapEntry());

        report.Reset (5);
        report.SetAllocatedInFreeMap (1, true);
        report.Finish();

        BlockMapBuilder::ApplyReport (report, map);

        Assert::IsTrue (map.blocks[11].role == SectorRole::Free);
        Assert::IsTrue (map.blocks[12].role == SectorRole::AllocatedUnowned);
        Assert::IsTrue (map.blocks[13].role == SectorRole::AllocatedUnowned);
        Assert::IsTrue (map.blocks[14].role == SectorRole::Free);
        Assert::IsTrue (map.blocks[3].role  == SectorRole::Free);
    }
};
