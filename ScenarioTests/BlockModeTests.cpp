#include "Pch.h"
#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "FixtureProvider.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Archives/GzipFile.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/Blocks/BlockMapBuilder.h"
#include "Machines/Apple2/Common/HfsVolume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockModeTests
//
//  The disk inspector's block mode over CiderPress II's ProDOS, HFS,
//  partitioned and 800K DOS images: every block the volume's integrity
//  pass counts as used has a structure on the map, every block with a
//  structure has fields, every file in the catalog is on the map, and a step
//  to a block of a 32 MB volume or a card's partition takes under 100 ms.
//  The disks come from scripts/FetchCiderPressTestFiles.ps1; each test that
//  reads one is skipped when its disk is absent.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockModeTests)
{
public:

    static constexpr const char *  kRoot       = "CiderPress";
    static constexpr size_t        kBlockBytes = 512;
    static constexpr double        kMaxStepMs  = 100.0;
    static constexpr int64_t       kTime       = 1'700'000'000;

    static bool OpenImage (const std::string & image, std::vector<Byte> & outBytes)
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;
        std::string        name;
        std::string        error;
        HRESULT            hrOpen = S_OK;



        hrOpen = fixtures.OpenFixture (std::string (kRoot) + "/" + image, bytes);

        if (FAILED (hrOpen))
        {
            Logger::WriteMessage (std::format ("SKIPPED: {} is not on this machine; run scripts/FetchCiderPressTestFiles.ps1\n", image).c_str());
            return false;
        }

        if (GzipFile::IsGzip (bytes))
        {
            AssertSucceeded (GzipFile::Expand (bytes, outBytes, name, error), TextEncoding::Utf8ToWide (error).c_str());
        }
        else
        {
            outBytes = std::move (bytes);
        }

        return true;
    }


    //  The image as file "image.po" of a fake file system, opened in block mode.
    static void OpenModel (const std::vector<Byte> & bytes, FakeDiskFileIo & io, BlockInspectorModel & model)
    {
        std::unique_ptr<BlockImageFile>  file;
        std::wstring                     error;



        io.files["image.po"]  = bytes;
        io.stamps["image.po"] = { bytes.size(), kTime };

        AssertSucceeded (BlockImageFile::Open (io, "image.po", file, error));
        AssertSucceeded (model.Open (std::move (file)));
    }


    //  Every block that isn't free has fields. Returns how many blocks were
    //  checked.
    static size_t CheckEveryUsedBlockAnnotated (BlockInspectorModel & model)
    {
        BlockInspectorModel::Block  bytes   = {};
        vector<BlockField>          fields;
        SectorRole                  role    = SectorRole::Free;
        size_t                      checked = 0;
        uint32_t                    block   = 0;



        for (block = 0; block < model.GetBlockCount(); block++)
        {
            role = model.GetRole (block);

            if (role == SectorRole::Free || role == SectorRole::AllocatedUnowned)
            {
                continue;
            }

            AssertSucceeded (model.ReadBlock (block, bytes));
            model.GetFields (block, bytes, fields);

            Assert::IsFalse (fields.empty(), std::format (L"block {} has no fields", block).c_str());
            checked++;
        }

        return checked;
    }


    //  Compares the map's structures with the volume's integrity report: a unit
    //  the report counts as used has a structure on the map, and one it
    //  doesn't, doesn't. A unit is a block, or for HFS an allocation block.
    static void CheckMapAgreesWithReport (const BlockMap & map, const VolumeIntegrityReport & report)
    {
        uint32_t  block    = 0;
        uint32_t  unit     = 0;
        bool      isMapped = false;



        Assert::IsTrue (report.GetUnitCount() > 0);

        for (block = 0; block < map.GetBlockCount(); block++)
        {
            if (map.fileSystem == BlockFileSystem::Hfs && block < map.allocationStart)
            {
                continue;
            }

            unit = map.fileSystem == BlockFileSystem::Hfs ? (block - map.allocationStart) / map.blocksPerAllocation : block;

            if (unit >= report.GetUnitCount())
            {
                continue;
            }

            isMapped = map.blocks[block].structure != BlockStructure::None;

            Assert::AreEqual (report.IsClaimed (unit), isMapped, std::format (L"block {}, unit {}", block, unit).c_str());
        }
    }


    //  An HFS directory has no blocks, since its record is in the catalog.
    static void CheckFilesOnMap (const BlockMap & map, const VolumeListing & listing)
    {
        Assert::IsFalse (listing.entries.empty(), L"the catalog lists files");

        for (const FileEntry & entry : listing.entries)
        {
            if (entry.isDirectory && map.fileSystem == BlockFileSystem::Hfs)
            {
                continue;
            }

            std::wstring  name = TextEncoding::Utf8ToWide (entry.name);
            bool          isOn = std::any_of (map.files.begin(), map.files.end(), [&] (const std::wstring & path) { return path == name || path.ends_with (L"/" + name); });

            Assert::IsTrue (isOn, (L"file on the map: " + name).c_str());
        }
    }


    //  The slowest of `count` steps spread over the image, each a step and
    //  everything the window reads for the block.
    static double TimeSteps (BlockInspectorModel & model, uint32_t count)
    {
        BlockInspectorModel::Block  bytes  = {};
        vector<BlockField>          fields;
        double                      worst  = 0.0;
        uint32_t                    i      = 0;
        uint32_t                    block  = 0;
        SectorRole                  role   = SectorRole::Free;



        for (i = 0; i < count; i++)
        {
            auto  start = std::chrono::steady_clock::now();

            block = (uint32_t) (((uint64_t) i * 7919) % model.GetBlockCount());

            model.SelectBlock (block);
            AssertSucceeded (model.ReadBlock (block, bytes));
            model.GetFields (block, bytes, fields);
            role = model.GetRole (block);
            (void) role;

            worst = std::max (worst, std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count());
        }

        return worst;
    }


    TEST_METHOD (ProDosSimpleSparse_MapAgreesWithTheCatalog)
    {
        std::vector<Byte>      bytes;
        FakeDiskFileIo         io;
        BlockInspectorModel    model;
        uint32_t               inner  = 0;
        VolumeIntegrityReport  report;
        VolumeListing          listing;



        if (!OpenImage ("prodos/simple-sparse.po", bytes))
        {
            return;
        }

        OpenModel (bytes, io, model);

        AssertSucceeded (ProDosVolume (BlockImage (bytes)).BuildIntegrityReport (report));
        AssertSucceeded (ProDosVolume (BlockImage (bytes)).Enumerate (listing));

        CheckMapAgreesWithReport (*model.FindMap (0, inner), report);
        CheckFilesOnMap (*model.FindMap (0, inner), listing);

        Logger::WriteMessage (std::format ("{} blocks annotated\n", CheckEveryUsedBlockAnnotated (model)).c_str());
    }


    TEST_METHOD (HfsSimpleGsos_MapAgreesWithTheCatalog)
    {
        std::vector<Byte>      bytes;
        FakeDiskFileIo         io;
        BlockInspectorModel    model;
        uint32_t               inner  = 0;
        VolumeIntegrityReport  report;
        VolumeListing          listing;
        const BlockMap       * map    = nullptr;



        if (!OpenImage ("hfs/simple-gsos.po", bytes))
        {
            return;
        }

        OpenModel (bytes, io, model);

        map = model.FindMap (0, inner);

        Assert::IsTrue (map->fileSystem == BlockFileSystem::Hfs);

        AssertSucceeded (HfsVolume (BlockImage (bytes)).BuildIntegrityReport (report));
        AssertSucceeded (HfsVolume (BlockImage (bytes)).Enumerate (listing));

        CheckMapAgreesWithReport (*map, report);
        CheckFilesOnMap (*map, listing);

        Logger::WriteMessage (std::format ("{} blocks annotated\n", CheckEveryUsedBlockAnnotated (model)).c_str());
    }


    TEST_METHOD (MultiPart_HasABandAndAVolumeForEachPartition)
    {
        std::vector<Byte>     bytes;
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        size_t                volumes = 0;



        if (!OpenImage ("turducken/MultiPart.hdv", bytes))
        {
            return;
        }

        OpenModel (bytes, io, model);

        Assert::IsTrue (model.GetBands().size() > 1);

        for (const BlockBand & band : model.GetBands())
        {
            Logger::WriteMessage (std::format (L"band {} to {}: {}, volume {}\n", band.first, band.first + band.count - 1, band.label, band.volume).c_str());

            if (band.volume >= 0)
            {
                volumes++;
            }
        }

        Assert::IsTrue (volumes > 1, L"two or more partitions have file systems");

        Logger::WriteMessage (std::format ("{} blocks annotated\n", CheckEveryUsedBlockAnnotated (model)).c_str());
    }


    TEST_METHOD (Dos800Samples_HaveTwoSectorVolumesWithTheirVtocs)
    {
        static constexpr const char *  kImages[] = { "dos800/UniDOS-sample.po.gz", "dos800/AmDOS-sample.po.gz", "dos800/OzDOS-sample.po.gz" };

        for (const char * image : kImages)
        {
            std::vector<Byte>            bytes;
            FakeDiskFileIo               io;
            BlockInspectorModel          model;
            BlockInspectorModel::Block   block = {};
            vector<BlockField>           fields;
            size_t                       vtocs = 0;
            uint32_t                     at    = 0;

            if (!OpenImage (image, bytes))
            {
                continue;
            }

            OpenModel (bytes, io, model);

            Assert::AreEqual ((size_t) 2, model.GetBands().size(), TextEncoding::Utf8ToWide (image).c_str());

            for (at = 0; at < model.GetBlockCount(); at++)
            {
                AssertSucceeded (model.ReadBlock (at, block));
                model.GetFields (at, block, fields);

                vtocs += (size_t) std::count_if (fields.begin(), fields.end(), [] (const BlockField & field) { return field.label.ends_with (L"VTOC: tracks") && field.value == L"$32"; });
            }

            Assert::AreEqual ((size_t) 2, vtocs, (L"a VTOC of 50 tracks for each volume of " + TextEncoding::Utf8ToWide (image)).c_str());
            Logger::WriteMessage (std::format ("{}: {} blocks annotated\n", image, CheckEveryUsedBlockAnnotated (model)).c_str());
        }
    }


    TEST_METHOD (MicroDriveCard_AStepToAPartitionsBlockTakesUnderATenthOfASecond)
    {
        std::vector<Byte>     bytes;
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        double                worst = 0.0;



        if (!OpenImage ("microdrive/HD120MB.po.gz", bytes))
        {
            return;
        }

        OpenModel (bytes, io, model);

        Assert::IsTrue (model.GetBands().size() > 1);

        worst = TimeSteps (model, 500);

        Logger::WriteMessage (std::format ("slowest step {:.2f} ms\n", worst).c_str());
        Assert::IsTrue (worst < kMaxStepMs, std::format (L"slowest step {:.2f} ms", worst).c_str());
    }


    TEST_METHOD (ProDos32Mb_AStepTakesUnderATenthOfASecond)
    {
        constexpr size_t  k32MBBlocks = 65535;
        constexpr size_t  kFileBytes  = 200'000;
        constexpr int     kFiles      = 20;



        std::vector<Byte>     buffer (k32MBBlocks * kBlockBytes, 0);
        std::vector<Byte>     next;
        FilePayload           payload;
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        double                worst = 0.0;
        int                   i     = 0;



        AssertSucceeded (ProDosSkeleton::Write (buffer, "BIG"));

        payload.bytes.assign (kFileBytes, 0x5A);
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;

        for (i = 0; i < kFiles; i++)
        {
            AssertSucceeded (ProDosVolume (BlockImage (buffer)).Write (FilePath::Parse (std::format ("FILE{}", i)), payload, next));
            buffer = std::move (next);
        }

        OpenModel (buffer, io, model);

        worst = TimeSteps (model, 500);

        Logger::WriteMessage (std::format ("slowest step {:.2f} ms\n", worst).c_str());
        Assert::IsTrue (worst < kMaxStepMs, std::format (L"slowest step {:.2f} ms", worst).c_str());
    }
};
