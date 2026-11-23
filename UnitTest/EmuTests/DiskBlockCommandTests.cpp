#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBlockCommandTests
//
//  disk blockread and blockwrite: the 512-byte ProDOS view of an image.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskBlockCommandTests)
{
public:

    static CommandLineOptions MakeBlockRead (const char * image, int block, int count)
    {
        CommandLineOptions  options;

        options.subcommand        = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = CommandLineOptions::DiskOptions::Command::BlockRead;
        options.disk.commandWord  = "blockread";
        options.disk.imagePath    = image;
        options.disk.block        = block;
        options.disk.count        = count;

        return options;
    }

    static CommandLineOptions MakeBlockWrite (const char * image, const char * file, int block)
    {
        CommandLineOptions  options;

        options.subcommand        = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = CommandLineOptions::DiskOptions::Command::BlockWrite;
        options.disk.commandWord  = "blockwrite";
        options.disk.imagePath    = image;
        options.disk.hostFile     = file;
        options.disk.block        = block;

        return options;
    }




    ////////////////////////////////////////////////////////////////////////////
    //
    //  blockread / blockwrite: the 512-byte ProDOS view, through the same
    //  single block map the ProDOS reader and writer use.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BlockWrite_PlacesBothHalvesWhereTheProDosMapPutsThem)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>  payload  = DiskRunnerFixtures::MakePayload (512);
        vector<Byte>  written;
        size_t        firstAt  = ProDosSkeleton::GetBlockByteOffset (3, 0);
        size_t        secondAt = ProDosSkeleton::GetBlockByteOffset (3, 256);
        size_t        naiveAt  = (size_t) 3 * 512;
        size_t        i        = 0;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.po.dsk");
        io.files["block.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeBlockWrite ("raw.po.dsk", "block.bin", 3)).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("raw.po.dsk", written));

        for (i = 0; i < 256; i++)
        {
            Assert::AreEqual ((int) payload[i],       (int) written[firstAt + i],
                L"the block's first half sits where the ProDOS map puts it");
            Assert::AreEqual ((int) payload[256 + i], (int) written[secondAt + i],
                L"and the second half where the map puts that");
        }

        Assert::AreNotEqual (naiveAt, firstAt,
            L"which for this block is not 512 * N into the buffer, or the assertions "
            L"above discriminate nothing");
    }

    TEST_METHOD (BlockRead_ReturnsWhatBlockWritePutThere)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload = DiskRunnerFixtures::MakePayload (1024);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.po.dsk");
        io.files["blocks.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeBlockWrite ("raw.po.dsk", "blocks.bin", 7)).exitStatus);

        result = runner.Run (MakeBlockRead ("raw.po.dsk", 7, 2));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.hasPayload);
        Assert::IsTrue (result.payload == payload,
            L"two blocks in, the same two blocks out, through the one block map");
    }

    //  THE LENS WORKS ON ANY CONTAINER. A ProDOS volume shipped inside a
    //  .dsk reads by block number exactly as a .po would, because the
    //  session normalizes every image into the same buffer -- and the proof
    //  is vendor material: the Merlin disk's volume directory key block
    //  carries its own volume name.
    TEST_METHOD (BlockRead_FindsTheVolumeDirectory_OnAProDosVolumeInADskContainer)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        std::string        text;

        DiskRunnerFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", DiskRunnerFixtures::kImage);

        result = runner.Run (MakeBlockRead (DiskRunnerFixtures::kImage, 2, 1));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.hasPayload);
        Assert::AreEqual (size_t (512), result.payload.size(), L"one whole block");

        text.assign (result.payload.begin(), result.payload.end());

        Assert::IsTrue (text.find ("MERLIN") != std::string::npos,
            L"the volume directory key block holds the volume name, which only a correct "
            L"block map could have assembled from its two sector records");
    }

    TEST_METHOD (Block_OffTheDisk_IsRefusedWithTheRange)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.po.dsk");
        io.files["block.bin"] = DiskRunnerFixtures::MakePayload (512);

        result = runner.Run (MakeBlockRead ("raw.po.dsk", 280, 1));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"there is no block 280");
        Assert::IsTrue (result.diagnostics.find ("0-279") != std::string::npos,
            L"and the refusal states the range");

        result = runner.Run (MakeBlockWrite ("raw.po.dsk", "block.bin", 279));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
            L"while the last block on the disk is a legal place for one block");

        result = runner.Run (MakeBlockRead ("raw.po.dsk", 279, 2));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"and a read that runs off the end is refused rather than shortened");
    }
};
