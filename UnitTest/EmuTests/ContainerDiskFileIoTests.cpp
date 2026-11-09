#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/Archives/ContainerDiskFileIo.h"
#include "Devices/Disk/Archives/ContainerPath.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Update/ZipArchive.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIoTests
//
//  Disk commands run on a DOS 3.3 image inside a ZIP, through the nested path
//  text, over the fake file I/O: the ZIP on the fake is rebuilt with the
//  image changed, the other member intact, and no temporary file left.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ContainerDiskFileIoTests)
{
public:

    static constexpr const char *  kZip    = "C:\\Disks\\set.zip";
    static constexpr const char *  kNested = "C:\\Disks\\set.zip|disks/blank.dsk";

    static void SeedZip (FakeDiskFileIo & io)
    {
        static constexpr Byte  kEmptyZip[22] = { 0x50, 0x4B, 0x05, 0x06 };

        std::vector<Byte>  image (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>  readme = { 'h', 'i' };
        std::vector<Byte>  one;
        std::vector<Byte>  two;



        AssertSucceeded (Dos33Skeleton::Write (image, 254));
        AssertSucceeded (ZipArchive::Replace (kEmptyZip, "readme.txt", readme, one));
        AssertSucceeded (ZipArchive::Replace (one, "disks/blank.dsk", image, two));

        io.files[kZip]  = two;
        io.stamps[kZip] = FileStamp { two.size(), 100 };
    }

    static CommandLineOptions MakeOptions (CommandLineOptions::DiskOptions::Command command)
    {
        CommandLineOptions  options;



        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.command   = command;
        options.disk.imagePath = kNested;

        return options;
    }


    TEST_METHOD (Put_WritesTheFileIntoTheImageInTheZip)
    {
        FakeDiskFileIo         fake;
        ContainerDiskFileIo    io (fake);
        DiskCommandRunner      runner (io);
        DiskCommandRunner      reader (io);
        CommandLineOptions     put  = MakeOptions (CommandLineOptions::DiskOptions::Command::Put);
        std::vector<ZipEntry>  entries;



        SeedZip (fake);
        fake.files["GREET"]  = std::vector<Byte> (300, 0x41);
        fake.stamps["GREET"] = FileStamp { 300, 100 };

        put.disk.hostFile       = "GREET";
        put.disk.path           = "GREET";
        put.disk.typeName       = "B";
        put.disk.loadAddress    = 0x2000;
        put.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (put).exitStatus);
        Assert::IsTrue (reader.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List)).output.find ("GREET") != std::string::npos);

        AssertSucceeded (ZipArchive::Extract (fake.files[kZip], entries));
        Assert::AreEqual ((size_t) 2, entries.size());
        Assert::IsTrue (entries[0].data == std::vector<Byte> { 'h', 'i' });
        Assert::IsTrue (fake.HasNoTemporaryFiles());
    }


    TEST_METHOD (Paths_PassThroughWhenNotNested)
    {
        FakeDiskFileIo       fake;
        ContainerDiskFileIo  io (fake);
        std::vector<Byte>    bytes;



        fake.files["C:\\plain.dsk"] = { 1, 2, 3 };

        Assert::IsTrue (io.Exists ("C:\\plain.dsk"));
        AssertSucceeded (io.ReadAllBytes ("C:\\plain.dsk", bytes));
        Assert::AreEqual ((size_t) 3, bytes.size());
    }


    TEST_METHOD (NestedFile_ExistsReadsAndStatsTheHost)
    {
        FakeDiskFileIo       fake;
        ContainerDiskFileIo  io (fake);
        std::vector<Byte>    bytes;
        FileStamp            stamp;



        SeedZip (fake);

        Assert::IsTrue  (io.Exists (kNested));
        Assert::IsFalse (io.Exists ("C:\\Disks\\set.zip|disks/missing.dsk"));
        AssertSucceeded (io.ReadAllBytes (kNested, bytes));
        Assert::AreEqual ((size_t) NibblizationLayer::kImageByteSize, bytes.size());
        AssertSucceeded (io.Stat (kNested, stamp));
        Assert::AreEqual ((int64_t) 100, stamp.modifiedUnix);
    }


    TEST_METHOD (Parse_InfersEachStepsKind)
    {
        ContainerPath  path = ContainerPath::Parse ("C:\\a.zip|b/c.shk.gz|DOCS/README");



        Assert::AreEqual ((size_t) 4, path.steps.size());
        Assert::IsTrue (path.steps[0].kind == ContainerStep::Kind::HostFile);
        Assert::IsTrue (path.steps[1].kind == ContainerStep::Kind::ZipMember);
        Assert::IsTrue (path.steps[2].kind == ContainerStep::Kind::Gzip);
        Assert::IsTrue (path.steps[3].kind == ContainerStep::Kind::ImageFile);
        Assert::AreEqual (std::string ("C:\\a.zip|b/c.shk.gz|DOCS/README"), path.ToString());
    }
};
