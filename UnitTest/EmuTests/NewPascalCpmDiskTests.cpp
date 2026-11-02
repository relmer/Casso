#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "CommandLineOptions.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/StockBootDisks.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NewPascalCpmDiskTests
//
//  `disk create` and `disk init` for Apple Pascal and CP/M, in every container,
//  and detection of an empty CP/M disk.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (NewPascalCpmDiskTests)
{
public:

    static constexpr size_t  kImageBytes  = 143360;
    static constexpr size_t  kSectorBytes = 256;

    static DiskCommandResult  RunCreate (FakeDiskFileIo & io, const std::string & path, const char * format, const char * volume = "")
    {
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;

        options.disk.command    = CommandLineOptions::DiskOptions::Command::Create;
        options.disk.imagePath  = path;
        options.disk.formatName = format;
        options.disk.volumeName = volume;

        return runner.Run (options);
    }

    static void  LoadImage (FakeDiskFileIo & io, const std::string & path, std::vector<Byte> & outSectors)
    {
        SectorDecodeReport  report;

        AssertSucceeded (VolumeImage::Load (io.files[path], path, outSectors, report));
    }

    TEST_METHOD (CreateMakesAPascalDiskInEveryContainer)
    {
        size_t                                    count      = 0;
        const DiskCommandRunner::ContainerName *  containers = DiskCommandRunner::GetAdvertisedContainers (count);

        Assert::IsTrue (count > 0);

        for (size_t i = 0; i < count; i++)
        {
            FakeDiskFileIo     io;
            std::string        path   = std::string ("C:\\disks\\new.") + containers[i].name;
            DiskCommandResult  result = RunCreate (io, path, "pascal", "work");
            std::vector<Byte>  sectors;
            VolumeListing      listing;

            Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());
            Assert::AreEqual (std::string (path + ": Apple Pascal, volume WORK, not bootable\n"), result.output);

            LoadImage (io, path, sectors);

            Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Pascal, TextEncoding::NarrowToWide (path).c_str());

            PascalVolume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
            Assert::AreEqual (std::string ("WORK"), listing.volumeName);
            Assert::AreEqual ((uint32_t) 280, listing.totalUnits);
            Assert::AreEqual ((uint32_t) 274, listing.freeUnits);
            Assert::IsTrue (listing.entries.empty());
        }
    }

    TEST_METHOD (CreateRecordsTodayOnAPascalDisk)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;
        SYSTEMTIME          when = {};

        when.wYear  = 2026;
        when.wMonth = 11;
        when.wDay   = 1;

        runner.SetClock ([when]() { return when; });

        options.disk.command    = CommandLineOptions::DiskOptions::Command::Create;
        options.disk.imagePath  = "C:\\disks\\dated.dsk";
        options.disk.formatName = "pascal";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        //  Month in the low 4 bits, day in the next 5, year in the top 7.
        const std::vector<Byte> &  image = io.files["C:\\disks\\dated.dsk"];
        size_t                     at    = ProDosSkeleton::GetBlockByteOffset (2, 20);

        Assert::AreEqual ((26 << 9) | (1 << 4) | 11, image[at] | (image[at + 1] << 8));
        Assert::AreEqual ((int) 'N', (int) image[ProDosSkeleton::GetBlockByteOffset (2, 7)], L"The default volume name");
    }

    TEST_METHOD (CreateRefusesAnIllegalPascalVolumeName)
    {
        for (const char * name : { "TOOLONGX", "A B", "A:B", "A=B", "A$", "A?", "A,B" })
        {
            FakeDiskFileIo     io;
            DiskCommandResult  result = RunCreate (io, "C:\\disks\\new.dsk", "pascal", name);

            Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
            Assert::IsTrue (result.diagnostics.find ("illegal volume name") != std::string::npos);
            Assert::IsFalse (io.Exists ("C:\\disks\\new.dsk"), L"Nothing is written");
        }
    }

    TEST_METHOD (CreateMakesACpmDiskInEveryContainerAndItIsDetected)
    {
        size_t                                    count      = 0;
        const DiskCommandRunner::ContainerName *  containers = DiskCommandRunner::GetAdvertisedContainers (count);

        Assert::IsTrue (count > 0);

        for (size_t i = 0; i < count; i++)
        {
            FakeDiskFileIo     io;
            std::string        path   = std::string ("C:\\disks\\new.") + containers[i].name;
            DiskCommandResult  result = RunCreate (io, path, "cpm");
            std::vector<Byte>  sectors;
            VolumeListing      listing;

            Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());
            Assert::AreEqual (std::string (path + ": CP/M, not bootable\n"), result.output);

            LoadImage (io, path, sectors);

            Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Cpm, TextEncoding::NarrowToWide (path).c_str());
            Assert::IsTrue (std::all_of (sectors.begin(), sectors.end(), [] (Byte value) { return value == CpmVolume::kFree; }), L"$E5 throughout");

            CpmVolume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
            Assert::AreEqual ((uint32_t) 126, listing.freeUnits);
        }
    }

    TEST_METHOD (CreateRefusesAVolumeForCpm)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result = RunCreate (io, "C:\\disks\\new.dsk", "cpm", "WORK");

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("CP/M has no volume") != std::string::npos);
        Assert::IsFalse (io.Exists ("C:\\disks\\new.dsk"));
    }

    TEST_METHOD (InitReformatsToPascalAndCpm)
    {
        FakeDiskFileIo      io;
        CommandLineOptions  options;
        DiskCommandRunner   runner (io);
        std::vector<Byte>   sectors;

        io.files["C:\\disks\\old.po"].assign (kImageBytes, 0x55);
        io.stamps["C:\\disks\\old.po"] = FileStamp { kImageBytes, 1 };

        options.disk.command    = CommandLineOptions::DiskOptions::Command::Init;
        options.disk.imagePath  = "C:\\disks\\old.po";
        options.disk.formatName = "pascal";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        LoadImage (io, "C:\\disks\\old.po", sectors);
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Pascal);

        options.disk.formatName = "cpm";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        LoadImage (io, "C:\\disks\\old.po", sectors);
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Cpm);
    }

    TEST_METHOD (AFileOnANewDiskIsReadBackInEveryContainer)
    {
        size_t                                    count      = 0;
        const DiskCommandRunner::ContainerName *  containers = DiskCommandRunner::GetAdvertisedContainers (count);

        Assert::IsTrue (count > 0);

        for (size_t i = 0; i < count; i++)
        {
            for (const char * format : { "pascal", "cpm" })
            {
                FakeDiskFileIo      io;
                DiskCommandRunner   runner (io);
                CommandLineOptions  options;
                DiskCommandResult   result;
                std::string         path    = std::string ("C:\\disks\\new.") + containers[i].name;
                std::vector<Byte>   content (1024, 0x5A);

                Assert::AreEqual (DiskCommandResult::kClean, RunCreate (io, path, format).exitStatus);

                io.files["C:\\host\\DATA.BIN"] = content;

                options.disk.command   = CommandLineOptions::DiskOptions::Command::Put;
                options.disk.imagePath = path;
                options.disk.hostFile  = "C:\\host\\DATA.BIN";
                result                 = runner.Run (options);

                Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());

                options                = CommandLineOptions();
                options.disk.command   = CommandLineOptions::DiskOptions::Command::Get;
                options.disk.imagePath = path;
                options.disk.path      = "DATA.BIN";
                result                 = runner.Run (options);

                Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());
                Assert::IsTrue (result.payload == content, TextEncoding::NarrowToWide (path).c_str());
            }
        }
    }

    //  A Pascal disk with made-up boot blocks and made-up system files.
    static std::vector<Byte>  BuildPascalSystem (std::initializer_list<const char *> names, bool loader = true)
    {
        std::vector<Byte>  image (kImageBytes, 0);
        Byte               seed = 1;

        PascalVolume::WriteBlank ("SOURCE", 0, image);

        for (const char * name : names)
        {
            std::vector<Byte>  next;
            FilePayload        payload;

            payload.bytes.assign (600, seed++);

            AssertSucceeded (PascalVolume (image).Write (FilePath::Parse (name), payload, next));

            image = std::move (next);
        }

        for (size_t i = 0; loader && i < 512; i++)
        {
            image[ProDosSkeleton::GetBlockByteOffset (0, i)] = (Byte) (i + 1);
            image[ProDosSkeleton::GetBlockByteOffset (1, i)] = (Byte) (i + 2);
        }

        return image;
    }

    //  A CP/M disk with made-up system tracks, a file, and a user 31 entry over
    //  the tracks.
    static std::vector<Byte>  BuildCpmSystem (bool tracks = true)
    {
        std::vector<Byte>  image;
        std::vector<Byte>  next;
        FilePayload        payload;

        CpmVolume::WriteBlank (image);

        for (size_t i = 0; tracks && i < 3 * 16 * kSectorBytes; i++)
        {
            image[i] = (Byte) (i + 1);
        }

        payload.bytes.assign (256, 7);

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("USER.COM"), payload, next));

        image = std::move (next);

        if (tracks)
        {
            size_t  at = 5 * 32;

            for (size_t i = 0; i < 32; i++)
            {
                constexpr std::array<Byte, 32>  kEntry =
                {
                    0x1F, 'c', 'p', '/', 'a', 'm', ' ', ' ', ' ', 's', 'y', 's', 0, 0, 0, 0x60,
                    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0, 0, 0, 0,
                };

                image[CpmVolume::GetSectorOffset (0, (int) ((at + i) / kSectorBytes)) + (at + i) % kSectorBytes] = kEntry[i];
            }
        }

        return image;
    }

    static DiskCommandResult  RunBootable (FakeDiskFileIo & io, const char * format, const std::vector<Byte> & source, const char * target = "C:\\disks\\boot.dsk")
    {
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;

        io.files["C:\\src\\system.dsk"] = source;

        options.disk.command      = CommandLineOptions::DiskOptions::Command::Create;
        options.disk.imagePath    = target;
        options.disk.formatName   = format;
        options.disk.bootable     = true;
        options.disk.bootableFrom = "C:\\src\\system.dsk";

        return runner.Run (options);
    }

    TEST_METHOD (BootablePascalCopiesTheLoaderAndTheSystemFiles)
    {
        FakeDiskFileIo     io;
        std::vector<Byte>  source = BuildPascalSystem ({ "SYSTEM.APPLE", "SYSTEM.PASCAL", "SYSTEM.MISCINFO", "SYSTEM.WRK.TEXT", "MINE.CODE" });
        std::vector<Byte>  sectors;
        VolumeListing      listing;
        FilePayload        payload;
        DiskCommandResult  result = RunBootable (io, "pascal", source);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());
        Assert::IsTrue (result.output.find (", bootable") != std::string::npos);

        LoadImage (io, "C:\\disks\\boot.dsk", sectors);

        for (size_t i = 0; i < 512; i++)
        {
            Assert::AreEqual ((int) source[ProDosSkeleton::GetBlockByteOffset (0, i)], (int) sectors[ProDosSkeleton::GetBlockByteOffset (0, i)]);
            Assert::AreEqual ((int) source[ProDosSkeleton::GetBlockByteOffset (1, i)], (int) sectors[ProDosSkeleton::GetBlockByteOffset (1, i)]);
        }

        PascalVolume  volume (sectors);

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual ((size_t) 3, listing.entries.size(), L"The work file and the user's file stay behind");
        Assert::AreEqual (std::string ("NEWDISK"), listing.volumeName, L"The new disk has a new volume name");
        Assert::AreEqual (std::string ("SYSTEM.APPLE"), listing.entries[0].name);

        AssertSucceeded (volume.Read (FilePath::Parse ("SYSTEM.PASCAL"), payload));
        Assert::AreEqual ((size_t) 600, payload.bytes.size());
        Assert::AreEqual ((int) 2, (int) payload.bytes[0]);
    }

    TEST_METHOD (BootablePascalNeedsALoaderAndBothFiles)
    {
        struct { std::vector<Byte> source; const char * reason; }  cases[] =
        {
            { BuildPascalSystem ({ "SYSTEM.APPLE", "SYSTEM.PASCAL" }, false), "no boot loader" },
            { BuildPascalSystem ({ "SYSTEM.PASCAL" }),                        "no SYSTEM.APPLE" },
            { BuildPascalSystem ({ "SYSTEM.APPLE" }),                         "no SYSTEM.PASCAL" },
            { BuildCpmSystem(),                                               "is not an Apple Pascal disk" },
        };

        for (const auto & row : cases)
        {
            FakeDiskFileIo     io;
            DiskCommandResult  result = RunBootable (io, "pascal", row.source);

            Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
            Assert::IsTrue (result.diagnostics.find (row.reason) != std::string::npos, TextEncoding::NarrowToWide (result.diagnostics).c_str());
            Assert::IsFalse (io.Exists ("C:\\disks\\boot.dsk"), L"Nothing is written");
        }
    }

    TEST_METHOD (BootableCpmCopiesTheTracksAndTheSystemEntry)
    {
        FakeDiskFileIo     io;
        std::vector<Byte>  source = BuildCpmSystem();
        std::vector<Byte>  sectors;
        VolumeListing      listing;
        DiskCommandResult  result = RunBootable (io, "cpm", source);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());

        LoadImage (io, "C:\\disks\\boot.dsk", sectors);

        Assert::IsTrue (std::equal (source.begin(), source.begin() + 3 * 16 * kSectorBytes, sectors.begin()));
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Cpm);

        AssertSucceeded (CpmVolume (sectors).Enumerate (listing));

        Assert::AreEqual ((size_t) 1, listing.entries.size(), L"The user's file stays behind");
        Assert::AreEqual (std::string ("31:cp/am.sys"), listing.entries[0].name);
        Assert::AreEqual ((uint32_t) 126, listing.freeUnits);
        Assert::AreEqual ((uint32_t) 140, listing.totalUnits);
    }

    TEST_METHOD (BootableCpmNeedsSystemTracksAndACpmDisk)
    {
        struct { std::vector<Byte> source; const char * reason; }  cases[] =
        {
            { BuildCpmSystem (false),                                  "no system on tracks 0 to 2" },
            { BuildPascalSystem ({ "SYSTEM.APPLE", "SYSTEM.PASCAL" }), "is not a CP/M disk" },
        };

        for (const auto & row : cases)
        {
            FakeDiskFileIo     io;
            DiskCommandResult  result = RunBootable (io, "cpm", row.source);

            Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
            Assert::IsTrue (result.diagnostics.find (row.reason) != std::string::npos, TextEncoding::NarrowToWide (result.diagnostics).c_str());
            Assert::IsFalse (io.Exists ("C:\\disks\\boot.dsk"));
        }
    }

    TEST_METHOD (BootableReadsASourceInAnyContainer)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;
        std::vector<Byte>   source = BuildCpmSystem();
        std::vector<Byte>   reordered;
        std::vector<Byte>   sectors;

        VolumeImage::DosLogicalToProDosFile (source, reordered);

        io.files["C:\\src\\system.po"] = reordered;

        options.disk.command      = CommandLineOptions::DiskOptions::Command::Create;
        options.disk.imagePath    = "C:\\disks\\boot.dsk";
        options.disk.formatName   = "cpm";
        options.disk.bootable     = true;
        options.disk.bootableFrom = "C:\\src\\system.po";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        LoadImage (io, "C:\\disks\\boot.dsk", sectors);

        Assert::IsTrue (std::equal (source.begin(), source.begin() + 3 * 16 * kSectorBytes, sectors.begin()));
    }

    TEST_METHOD (TheStockDiskFollowsTheFormatAndTheWord)
    {
        StockBootDisks::Which  disk = StockBootDisks::Which::Dos33Master;

        Assert::IsTrue (StockBootDisks::TryGetStockDisk ("pascal", "", disk));
        Assert::IsTrue (disk == StockBootDisks::Which::PascalBootDisk);

        Assert::IsTrue (StockBootDisks::TryGetStockDisk ("cpm", "", disk));
        Assert::IsTrue (disk == StockBootDisks::Which::CpamBootDisk);

        Assert::IsTrue (StockBootDisks::TryGetStockDisk ("cpm", "SoftCard", disk));
        Assert::IsTrue (disk == StockBootDisks::Which::SoftCardBootDisk);

        Assert::IsTrue (StockBootDisks::TryGetStockDisk ("prodos", "", disk));
        Assert::IsTrue (disk == StockBootDisks::Which::ProDosUsersDisk);

        Assert::IsTrue (StockBootDisks::TryGetStockDisk ("dos33", "", disk));
        Assert::IsTrue (disk == StockBootDisks::Which::Dos33Master);

        Assert::IsFalse (StockBootDisks::TryGetStockDisk ("cpm", "C:\\mine.dsk", disk), L"An image overrides the stock disk");
        Assert::IsFalse (StockBootDisks::TryGetStockDisk ("pascal", "softcard", disk), L"The word is CP/M's");
        Assert::IsFalse (StockBootDisks::TryGetStockDisk ("none", "", disk));
    }

    TEST_METHOD (TheDialogSelectsTheBootSource)
    {
        using Choices = CassoExplorerNewDiskChoices;

        Assert::AreEqual ((size_t) 2, Choices::GetCpmSystemLabels().size());
        Assert::AreEqual (std::string (""),            Choices::MakeBootSource (Choices::kFormatCpm, 0, L""));
        Assert::AreEqual (std::string ("softcard"),    Choices::MakeBootSource (Choices::kFormatCpm, 1, L""));
        Assert::AreEqual (std::string ("C:\\own.dsk"), Choices::MakeBootSource (Choices::kFormatCpm, 1, L"C:\\own.dsk"), L"An image overrides the list");
        Assert::AreEqual (std::string (""),            Choices::MakeBootSource (Choices::kFormatPascal, 1, L""), L"The list is CP/M's");
    }

    TEST_METHOD (AListingReportsWhatABootNeeds)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;
        DiskCommandResult   result;

        Assert::AreEqual (DiskCommandResult::kClean, RunBootable (io, "pascal", BuildPascalSystem ({ "SYSTEM.APPLE", "SYSTEM.PASCAL" })).exitStatus);
        Assert::AreEqual (DiskCommandResult::kClean, RunCreate (io, "C:\\disks\\blank.dsk", "pascal").exitStatus);

        options.disk.command   = CommandLineOptions::DiskOptions::Command::List;
        options.disk.imagePath = "C:\\disks\\boot.dsk";

        result = runner.Run (options);
        Assert::IsTrue (result.output.find ("Not bootable") == std::string::npos);

        options.disk.imagePath = "C:\\disks\\blank.dsk";

        result = runner.Run (options);
        Assert::IsTrue (result.output.find ("Not bootable: it has no boot loader") != std::string::npos);
    }

    TEST_METHOD (TheNewDiskDialogOffersPascalAndCpm)
    {
        using Choices = CassoExplorerNewDiskChoices;

        Assert::AreEqual (std::string ("pascal"), Choices::MakeRequest (Choices::kFormatPascal, 0, L"", false).formatName);
        Assert::AreEqual (std::string ("cpm"),    Choices::MakeRequest (Choices::kFormatCpm, 0, L"", false).formatName);
        Assert::AreEqual (std::string ("none"),   Choices::MakeRequest (Choices::kFormatNone, 0, L"", false).formatName);
        Assert::AreEqual ((size_t) 7, Choices::GetVolumeMaxLength (Choices::kFormatPascal));
        Assert::IsTrue   (Choices::ValidateVolume (Choices::kFormatPascal, L"WORK").empty());
        Assert::IsFalse  (Choices::ValidateVolume (Choices::kFormatPascal, L"TOOLONGX").empty());
        Assert::IsFalse  (Choices::ValidateVolume (Choices::kFormatPascal, L"A:B").empty());
        Assert::IsTrue   (Choices::ValidateVolume (Choices::kFormatCpm, L"anything").empty(), L"CP/M has no volume to check");
    }

    TEST_METHOD (DetectionDoesNotCallOtherDisksCpm)
    {
        std::vector<Byte>  zeros (kImageBytes, 0);
        std::vector<Byte>  noise (kImageBytes, 0);
        std::vector<Byte>  dataOnly (kImageBytes, 0);
        std::vector<Byte>  dos (kImageBytes, 0);
        std::vector<Byte>  prodos (kImageBytes, 0);
        std::vector<Byte>  block (kImageBytes, 0);
        unsigned           seed = 1983;

        Assert::IsTrue (VolumeImage::DetectFilesystem (zeros) == VolumeKind::Unknown, L"An all-zero disk");

        for (Byte & value : noise)
        {
            seed  = seed * 1103515245u + 12345u;
            value = (Byte) (seed >> 16);
        }

        Assert::IsTrue (VolumeImage::DetectFilesystem (noise) == VolumeKind::Unknown, L"Random bytes");

        for (size_t i = 0; i < 16 * kSectorBytes; i++)
        {
            dataOnly[3 * 16 * kSectorBytes + i] = (Byte) (0x41 + i % 26);
        }

        Assert::IsTrue (VolumeImage::DetectFilesystem (dataOnly) == VolumeKind::Unknown, L"A track 3 of data");

        AssertSucceeded (Dos33Skeleton::Write (dos, 254));
        AssertSucceeded (ProDosSkeleton::Write (prodos, "BLANK"));
        PascalVolume::WriteBlank ("BLANK", 0, block);

        //  $E5 where CP/M's directory would be doesn't outweigh their signatures.
        for (std::vector<Byte> * disk : { &dos, &prodos, &block })
        {
            for (size_t sector : CpmVolume::GetDirectorySectors())
            {
                std::fill_n (disk->begin() + (ptrdiff_t) (sector * kSectorBytes), kSectorBytes, CpmVolume::kFree);
            }
        }

        Assert::IsTrue (VolumeImage::DetectFilesystem (dos)    == VolumeKind::Dos33);
        Assert::IsTrue (VolumeImage::DetectFilesystem (prodos) == VolumeKind::ProDos);
        Assert::IsTrue (VolumeImage::DetectFilesystem (block)  == VolumeKind::Pascal);
    }

    TEST_METHOD (DetectionFindsAnEmptyDirectoryWhateverIsAroundIt)
    {
        std::vector<Byte>  image (kImageBytes, 0);

        for (size_t sector : CpmVolume::GetDirectorySectors())
        {
            std::fill_n (image.begin() + (ptrdiff_t) (sector * kSectorBytes), kSectorBytes, CpmVolume::kFree);
        }

        Assert::IsTrue (VolumeImage::DetectFilesystem (image) == VolumeKind::Cpm);

        image[CpmVolume::GetSectorOffset (0, 0) + 1] = 0x01;

        Assert::IsTrue (VolumeImage::DetectFilesystem (image) == VolumeKind::Unknown, L"A control character in a free slot isn't an erased name");
    }

    TEST_METHOD (DetectionFindsADiskWithEveryFileErased)
    {
        std::vector<Byte>  image;
        std::vector<Byte>  edited;
        FilePayload        payload;

        CpmVolume::WriteBlank (image);

        payload.bytes.assign (1024, 3);

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("A.TXT"), payload, edited));
        AssertSucceeded (CpmVolume (edited).Delete (FilePath::Parse ("A.TXT"), image));

        Assert::IsTrue (VolumeImage::DetectFilesystem (image) == VolumeKind::Cpm);
    }
};
