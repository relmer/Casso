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
