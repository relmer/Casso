#include "Pch.h"

#include "Devices/Disk/Containers/DiskCopy42Container.h"
#include "Devices/Disk/Archives/NuFxArchive.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/HfsVolume.h"
#include "Devices/Disk/BlockImage.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Core/TextEncoding.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommandTests
//
//  disk create and init: each container and filesystem, sizes, the volume
//  name and number, and invalid command lines.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskCreateCommandTests)
{
public:

    //  IT ASKS FOR THE PAGE RATHER THAN LISTING THE COMMANDS ITSELF.
    //
    //  The refusal used to name all twelve, which put the same list on the
    //  screen twice once the edge started printing the disk page above it.
    //  What the refusal owes the reader now is which word it could not read;
    //  what the commands ARE is the page's job, and a sweep over the grammar's
    //  own table already holds the page to it.
    ////////////////////////////////////////////////////////////////////////////
    //
    //  create and init.
    //
    //  The commands that make a disk rather than edit one, and the pair the worked
    //  example needed: every step of it began `disk put mydisk.dsk`, and
    //  nothing anywhere made mydisk.dsk.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Create_WritesAFormattedImageThatListsAsEmpty)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result = runner.Run (DiskRunnerFixtures::MakeCreate ("new.dsk"));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (io.Exists ("new.dsk"), L"the image is there afterwards");
        Assert::IsTrue (result.output.find ("DOS 3.3") != std::string::npos,
                        L"and it reports what it made");
    }

    //  A .d13 image is a blank 13-sector DOS volume that lists, takes a file
    //  and formats again.
    TEST_METHOD (Create_WritesAThirteenSectorDos32Image)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("old.d13");
        CommandLineOptions  init    = DiskRunnerFixtures::MakeCreate ("old.d13");
        DiskCommandResult   result;
        vector<Byte>        bytes;
        Dos33Geometry       geometry;



        options.disk.volumeName = "77";
        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find ("DOS 3.2, volume 77") != std::string::npos);

        AssertSucceeded (io.ReadAllBytes ("old.d13", bytes));

        Assert::AreEqual (ThirteenSectorCodec::kImageByteSize, bytes.size());
        Assert::IsTrue (Dos33Geometry::TryDetect (bytes, geometry));
        Assert::IsTrue (geometry.IsThirteenSector());

        init.disk.command     = CommandLineOptions::DiskOptions::Command::Init;
        init.disk.commandWord = "init";
        result = runner.Run (init);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
    }

    TEST_METHOD (Create_ThirteenSectorImageOfAnotherFormat_IsABadCommandLine)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  asProDos = DiskRunnerFixtures::MakeCreate ("old.d13");
        CommandLineOptions  asDsk    = DiskRunnerFixtures::MakeCreate ("old.dsk");
        DiskCommandResult   result;



        asProDos.disk.formatName = "prodos";
        result = runner.Run (asProDos);

        Assert::IsTrue (result.badCommandLine);
        Assert::IsFalse (io.Exists ("old.d13"));

        asDsk.disk.formatName = "dos32";
        result = runner.Run (asDsk);

        Assert::IsTrue (result.badCommandLine);
        Assert::IsFalse (io.Exists ("old.dsk"));
    }

    //  A ProDOS volume larger than a floppy, in each block container.
    TEST_METHOD (Create_WritesA5MBProDosVolume)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("big.hdv");
        vector<Byte>        written;
        vector<Byte>        sectors;
        SectorDecodeReport  report;



        options.disk.sizeName = "5M";

        DiskCommandResult  result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        AssertSucceeded (io.ReadAllBytes ("big.hdv", written));
        Assert::AreEqual ((size_t) 10240 * 512, written.size());

        AssertSucceeded (VolumeImage::Load (written, "big.hdv", sectors, report));
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::ProDos);
    }

    TEST_METHOD (Create_WritesAn800KDiskCopyImage)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result = runner.Run (DiskRunnerFixtures::MakeCreate ("disk.dc42"));
        vector<Byte>       written;



        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        AssertSucceeded (io.ReadAllBytes ("disk.dc42", written));
        Assert::IsTrue (DiskCopy42Container::IsDiskCopy42 (written));
    }

    TEST_METHOD (Create_WritesA140KTwoImgInProDosOrder)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("small.2mg");
        vector<Byte>        written;
        vector<Byte>        sectors;
        SectorDecodeReport  report;



        options.disk.sizeName = "140k";

        DiskCommandResult  result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        AssertSucceeded (io.ReadAllBytes ("small.2mg", written));
        AssertSucceeded (VolumeImage::Load (written, "small.2mg", sectors, report));
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::ProDos);
    }

    TEST_METHOD (Init_ReformatsAHardDiskImageAtItsSize)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("big.hdv");
        vector<Byte>        written;
        vector<Byte>        sectors;
        SectorDecodeReport  report;
        VolumeListing       listing;



        options.disk.sizeName = "10M";
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        options.disk.command    = CommandLineOptions::DiskOptions::Command::Init;
        options.disk.sizeName   = "";
        options.disk.volumeName = "AGAIN";

        DiskCommandResult  result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        AssertSucceeded (io.ReadAllBytes ("big.hdv", written));
        Assert::AreEqual ((size_t) 20480 * 512, written.size());

        AssertSucceeded (VolumeImage::Load (written, "big.hdv", sectors, report));
        AssertSucceeded (VolumeImage::CreateVolume (VolumeKind::ProDos, sectors)->Enumerate (listing));
        Assert::AreEqual (std::string ("AGAIN"), listing.volumeName);
    }

    //  An HFS volume, 800K by default, then reformatted with another name.
    TEST_METHOD (Create_WritesAnHfsVolumeAndInitReformatsIt)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("mac.po");
        vector<Byte>        written;
        vector<Byte>        sectors;
        SectorDecodeReport  report;
        VolumeListing       listing;



        options.disk.formatName = "hfs";

        DiskCommandResult  created = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, created.exitStatus, TextEncoding::NarrowToWide (created.diagnostics).c_str());
        AssertSucceeded (io.ReadAllBytes ("mac.po", written));
        Assert::AreEqual ((size_t) 1600 * 512, written.size());

        AssertSucceeded (VolumeImage::Load (written, "mac.po", sectors, report));
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Hfs);
        AssertSucceeded (VolumeImage::CreateVolume (VolumeKind::Hfs, sectors)->Enumerate (listing));
        Assert::AreEqual (std::string ("Untitled"), listing.volumeName);

        options.disk.command    = CommandLineOptions::DiskOptions::Command::Init;
        options.disk.volumeName = "Work Disk";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);
        AssertSucceeded (io.ReadAllBytes ("mac.po", written));
        AssertSucceeded (VolumeImage::Load (written, "mac.po", sectors, report));
        AssertSucceeded (VolumeImage::CreateVolume (VolumeKind::Hfs, sectors)->Enumerate (listing));
        Assert::AreEqual (std::string ("Work Disk"), listing.volumeName);
    }

    TEST_METHOD (Create_WritesA32MBHfsHardDisk)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("mac.hdv");
        vector<Byte>        written;



        options.disk.formatName = "HFS";
        options.disk.sizeName   = "32M";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);
        AssertSucceeded (io.ReadAllBytes ("mac.hdv", written));
        Assert::AreEqual ((size_t) 65535 * 512, written.size());
        Assert::IsTrue (HfsVolume::IsFound (BlockImage (written)));
    }

    TEST_METHOD (Create_RefusesA140KHfsVolumeAndAColonInItsName)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("mac.po");



        options.disk.formatName = "hfs";
        options.disk.sizeName   = "140K";

        DiskCommandResult  tiny = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, tiny.exitStatus);
        Assert::IsTrue (tiny.diagnostics.find ("800K or larger") != std::string::npos);

        options.disk.sizeName   = "";
        options.disk.volumeName = "A:B";

        DiskCommandResult  colon = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, colon.exitStatus);
        Assert::IsTrue (colon.diagnostics.find ("colon") != std::string::npos);
        Assert::IsFalse (io.Exists ("mac.po"));
    }

    TEST_METHOD (Test_ReportsACleanImageAndADamagedArchive)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("clean.dsk");
        NuFxArchive         archive;
        NuFxRecord          record;
        vector<Byte>        bytes;



        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        options.disk.command     = CommandLineOptions::DiskOptions::Command::Test;
        options.disk.commandWord = "test";

        DiskCommandResult  clean = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, clean.exitStatus, TextEncoding::NarrowToWide (clean.output + clean.diagnostics).c_str());
        Assert::IsTrue (clean.output.find ("no problems") != std::string::npos);

        record.threads.push_back (NuFxArchive::MakeFilenameThread ("A"));
        record.threads.push_back (NuFxArchive::MakeDataThread (NuFxThread::kKindDataFork, vector<Byte> (100, 1)));
        record.threads[1].format = NuFxThread::kFormatStored;
        record.threads[1].stored = vector<Byte> (100, 1);
        archive.records.push_back (record);
        archive.Build (bytes);
        bytes[bytes.size() - 1] ^= 0xFF;
        io.files["bad.shk"] = bytes;

        options.disk.imagePath = "bad.shk";

        DiskCommandResult  damaged = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kWithComplaints, damaged.exitStatus);
        Assert::IsTrue (damaged.output.find ("CRC") != std::string::npos);
    }

    //  --bootable on a disk larger than 140K was dropped without a word, so a
    //  32 MB volume had no PRODOS and didn't start from a hard drive card.
    //  Found booting one on Apple's SCSI card in MAME.
    TEST_METHOD (Create_MakesALargeProDosVolumeBootable)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("big.hdv");
        DiskCommandResult   made;
        DiskCommandResult   listed;



        DiskRunnerFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-b.dsk", "master.dsk");

        for (const char * size : { "800K", "32M" })
        {
            options.disk.sizeName     = size;
            options.disk.formatName   = "prodos";
            options.disk.bootable     = true;
            options.disk.bootableFrom = "master.dsk";

            made = runner.Run (options);
            Assert::AreEqual (DiskCommandResult::kClean, made.exitStatus, TextEncoding::NarrowToWide (made.output + made.diagnostics).c_str());
            Assert::IsTrue   (made.output.find (", bootable") != std::string::npos, TextEncoding::NarrowToWide (made.output).c_str());
            Assert::AreNotEqual ((int) io.files["big.hdv"][0], 0, L"block 0 has the ProDOS loader");

            listed = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List, "big.hdv"));
            Assert::IsTrue (listed.output.find ("PRODOS") != std::string::npos, TextEncoding::NarrowToWide (listed.output).c_str());
            Assert::IsTrue (listed.output.find ("Not bootable") == std::string::npos);

            io.files.erase ("big.hdv");
            io.stamps.erase ("big.hdv");
        }

        options.disk.formatName = "hfs";
        made = runner.Run (options);
        Assert::AreEqual (DiskCommandResult::kNoOutput, made.exitStatus, L"an HFS volume isn't made bootable");
        Assert::IsFalse  (io.Exists ("big.hdv"));
    }

    TEST_METHOD (Create_RefusesALargeDosDisk)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("big.po");



        options.disk.sizeName   = "800K";
        options.disk.formatName = "dos33";

        DiskCommandResult  result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsFalse (io.Exists ("big.po"));
    }

    TEST_METHOD (Create_RefusesADiskCopyImageThatIsNot800K)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("disk.dc42");



        options.disk.sizeName = "5M";

        DiskCommandResult  result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("Disk Copy") != std::string::npos);
    }

    //  IT WILL NOT WRITE OVER SOMETHING. A disk somebody still wanted is one
    //  keystroke from a disk they no longer have, and the refusal names the
    //  command for meaning it.
    TEST_METHOD (Create_RefusesToReplaceAnImageThatIsAlreadyThere)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  first  = runner.Run (DiskRunnerFixtures::MakeCreate ("new.dsk"));
        DiskCommandResult  second = runner.Run (DiskRunnerFixtures::MakeCreate ("new.dsk"));

        Assert::AreEqual (DiskCommandResult::kClean,    first.exitStatus);
        Assert::AreEqual (DiskCommandResult::kNoOutput, second.exitStatus);
        Assert::IsTrue (second.diagnostics.find ("init") != std::string::npos,
                        L"and points at the command that does mean it");
    }

    TEST_METHOD (Create_AppendsTheTypeExtensionToABareName)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("dir.v2\\foo");
        DiskCommandResult   result;



        options.disk.containerType = "WOZ";
        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue  (io.Exists ("dir.v2\\foo.woz"), L"the extension follows --type");
        Assert::IsFalse (io.Exists ("dir.v2\\foo"),     L"and the bare name is not written");
    }

    TEST_METHOD (Create_KeepsAnExtensionOrTrailingDotAsGiven)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  dotted  = DiskRunnerFixtures::MakeCreate ("foo.");
        CommandLineOptions  named   = DiskRunnerFixtures::MakeCreate ("bar.img");
        DiskCommandResult   result;



        dotted.disk.containerType = "dsk";
        named.disk.containerType  = "dsk";

        result = runner.Run (dotted);
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (io.Exists ("foo."), L"a trailing dot means no extension");

        result = runner.Run (named);
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue  (io.Exists ("bar.img"),     L"an existing extension is kept");
        Assert::IsFalse (io.Exists ("bar.img.dsk"), L"and nothing is added to it");
    }

    //  The container follows the name when --type is not given, which is what
    //  makes `disk create mydisk.po` do the obvious thing.
    TEST_METHOD (Create_TakesTheContainerFromTheNameWhenTypeIsNotGiven)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("new.po");
        DiskCommandResult   result;

        options.disk.formatName = "prodos";
        result                  = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find ("ProDOS") != std::string::npos);
    }

    //  A word that names no container is refused BY NAME. Handing back a .dsk
    //  would be a disk they did not ask for under a name they did.
    TEST_METHOD (Create_RefusesAContainerItCannotWrite)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("new.dmg");
        DiskCommandResult   result;

        options.disk.containerType = "dmg";
        result                     = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine, L"so the page prints above it");
        Assert::IsTrue (result.diagnostics.find ("dmg") != std::string::npos,
                        L"and the word they typed is quoted back");
        Assert::IsFalse (io.Exists ("new.dmg"), L"and nothing was written");
    }

    //
    //  EVERY WORD THE TOOL ADVERTISES MUST MAKE A DISK. The list is swept
    //  rather than typed out again, so a container added to the table without
    //  a matching arm in the builder fails here instead of raising an
    //  assertion dialog in front of whoever typed the new word -- which is
    //  exactly what `disk create foo.do` did, in both spellings, while the
    //  tool's own error text offered it.
    //
    //  Raw contents for the sweep, because that is the one filling every
    //  container takes; asking each for its own filesystem would mean
    //  restating the pairing matrix here, which is the thing that went stale.
    //
    TEST_METHOD (Create_WritesEveryContainerItAdvertises)
    {
        const BlankDiskSpecResolver::ContainerName  * containers = nullptr;
        size_t                                        count      = 0;
        size_t                                        i          = 0;



        containers = BlankDiskSpecResolver::GetAdvertisedContainers (count);

        Assert::IsTrue (count > 0, L"the tool advertises at least one container");

        for (i = 0; i < count; i++)
        {
            FakeDiskFileIo      io;
            DiskCommandRunner   runner (io);
            std::string         word    = containers[i].name;
            std::string         byName  = "byname." + word;
            std::string         byType  = "bytype." + word;
            std::wstring        which   = TextEncoding::NarrowToWide (word);
            CommandLineOptions  options = DiskRunnerFixtures::MakeCreate (byName.c_str());
            DiskCommandResult   result;

            //  The name decides the container.
            options.disk.formatName = "none";
            result                  = runner.Run (options);

            Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
                (L"an advertised extension must write a disk: ." + which).c_str());
            Assert::IsTrue (io.Exists (byName), L"and leave the image behind");

            //  And so does the word, said outright.
            options                    = DiskRunnerFixtures::MakeCreate (byType.c_str());
            options.disk.formatName    = "none";
            options.disk.containerType = word;
            result                     = runner.Run (options);

            Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
                (L"and so must the same word given to the type flag: " + which).c_str());
            Assert::IsTrue (io.Exists (byType), L"and leave that image behind too");
        }
    }

    //
    //  AN ADVERTISED CONTAINER ASKED FOR THE WRONG FILESYSTEM IS REFUSED, NOT
    //  ASSERTED. The builder's rules answer in verdicts for this reason: every
    //  one of them is reachable by typing, so E_INVALIDARG -- which means a
    //  caller has a bug, and asserts to say so -- is the wrong verdict here.
    //
    //  No ExpectedEhmAssert guard on purpose. An assertion inside this call
    //  routes to Assert::Fail, so the test fails if one fires.
    //
    TEST_METHOD (Create_RefusesTheWrongFilesystemForAContainerWithoutAsserting)
    {
        const BlankDiskSpecResolver::ContainerName  * containers = nullptr;
        size_t                                        count      = 0;
        size_t                                        i          = 0;



        containers = BlankDiskSpecResolver::GetAdvertisedContainers (count);

        for (i = 0; i < count; i++)
        {
            FakeDiskFileIo      io;
            DiskCommandRunner   runner (io);
            std::string         word     = containers[i].name;
            std::string         dosPath  = "dos." + word;
            std::string         proPath  = "prodos." + word;
            std::wstring        which    = TextEncoding::NarrowToWide (word);
            CommandLineOptions  options  = DiskRunnerFixtures::MakeCreate (dosPath.c_str());
            DiskCommandResult   dos;
            DiskCommandResult   proDos;

            options.disk.formatName = "dos33";
            dos                     = runner.Run (options);

            options                 = DiskRunnerFixtures::MakeCreate (proPath.c_str());
            options.disk.formatName = "prodos";
            proDos                  = runner.Run (options);

            //  One of the two is refused for every container but woz, and the
            //  refusal is an ordinary exit rather than a broken invariant.
            Assert::IsTrue (dos.exitStatus    == DiskCommandResult::kClean
                         || dos.exitStatus    == DiskCommandResult::kNoOutput,
                (L"a DOS 3.3 disk is written or refused, never anything else: " + which).c_str());

            Assert::IsTrue (proDos.exitStatus == DiskCommandResult::kClean
                         || proDos.exitStatus == DiskCommandResult::kNoOutput,
                (L"and so is a ProDOS one: " + which).c_str());
        }
    }

    //  THE REFUSAL REPORTS THE RULE THAT WAS BROKEN, AND ONLY THAT ONE. The
    //  message this replaced recited the whole pairing matrix and the boot
    //  rule together, so it read the same whichever one had been tripped.
    TEST_METHOD (Create_RefusalReportsOnlyTheRuleThatWasBroken)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("wrong.po");
        DiskCommandResult   pairing;
        DiskCommandResult   badName;

        options.disk.formatName = "dos33";
        pairing                 = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, pairing.exitStatus);
        Assert::IsTrue (pairing.diagnostics.find ("illegal container and filesystem") != std::string::npos,
                        L"the broken rule is the one reported");
        Assert::IsTrue (pairing.diagnostics.find (".po holds ProDOS") != std::string::npos,
                        L"and the rule is stated in full");
        Assert::IsFalse (io.Exists ("wrong.po"), L"and nothing was written");

        options                 = DiskRunnerFixtures::MakeCreate ("badname.po");
        options.disk.formatName = "prodos";
        options.disk.volumeName = "1LEADINGDIGIT";
        badName                 = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, badName.exitStatus);
        Assert::IsTrue (badName.diagnostics.find ("illegal volume name") != std::string::npos,
                        L"the volume-name rule is the one reported");
        Assert::IsTrue (badName.diagnostics.find ("holds DOS 3.3") == std::string::npos,
                        L"and the pairing rule, which they did not break, stays out of it");

        //  A REFUSAL IS READ IN THE SAME TERMINAL A LISTING IS. A line that
        //  wraps loses the indent that marks it as the explanation.
        DiskRunnerFixtures::AssertEveryLineFitsEightyColumns (pairing.diagnostics);
        DiskRunnerFixtures::AssertEveryLineFitsEightyColumns (badName.diagnostics);
    }

    //  WHAT IS REPORTED IS WHAT IS ON THE DISK. ProDOS holds a volume name in
    //  upper case, so `--volume mydisk` makes /MYDISK and asking for it that
    //  way is perfectly good. The confirmation used to read back the name that
    //  was typed while `disk list` read back the name that is there, so the
    //  two disagreed over a disk that had been written correctly all along.
    TEST_METHOD (Create_ReportsTheProDosVolumeInTheCaseItIsStored_NotTheCaseItWasTyped)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("new.po");
        DiskCommandResult   result;

        options.disk.formatName = "prodos";
        options.disk.volumeName = "mydisk";
        result                  = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        Assert::IsTrue (result.output.find ("MYDISK") != std::string::npos,
                        L"the confirmation reports the volume the disk actually has");

        Assert::IsTrue (result.output.find ("mydisk") == std::string::npos,
                        L"and never the lower-case form, which is on no disk anywhere");
    }




    //  A DOS 3.3 volume is a NUMBER, and a word that is not one is refused
    //  rather than quietly reading as zero.
    TEST_METHOD (Create_RefusesAVolumeThatIsNotADosNumber)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("new.dsk");
        DiskCommandResult   result;

        options.disk.volumeName = "MYDISK";
        result                  = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsFalse (io.Exists ("new.dsk"));
    }

    //  init needs one to be there; create needs one not to be. Between them
    //  every state is covered and neither guesses.
    TEST_METHOD (Init_RefusesAnImageThatIsNotThere)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("missing.dsk");
        DiskCommandResult   result;

        options.disk.command     = CommandLineOptions::DiskOptions::Command::Init;
        options.disk.commandWord = "init";
        result                = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("create") != std::string::npos,
                        L"and points at the command that makes one");
    }

    //  THE CONTAINER IS NOT A CHOICE UNDER init. It was decided when the file
    //  was made, so --type is refused rather than silently ignored.
    TEST_METHOD (Init_RefusesTypeBecauseTheContainerIsAlreadyDecided)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("new.dsk");
        DiskCommandResult   result;

        runner.Run (options);

        options.disk.command          = CommandLineOptions::DiskOptions::Command::Init;
        options.disk.commandWord      = "init";
        options.disk.containerType = "woz";
        result                     = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine);
    }


    //  Every 4-and-4 address field on a track, as their volumes. The track's
    //  bits are read into nibbles as the drive reads them, since a self-sync
    //  byte has ten bits and leaves the nibbles off byte boundaries.
    static vector<Byte> GetAddressVolumes (const DiskImage & image, int track)
    {
        const vector<Byte> &  bits    = image.GetTrackBits (track);
        size_t                count   = image.GetTrackBitCount (track);
        Byte                  nibble  = 0;
        vector<Byte>          nibbles;
        vector<Byte>          volumes;

        for (size_t i = 0; i < count; i++)
        {
            nibble = (Byte) ((nibble << 1) | ((bits[i >> 3] >> (7 - (i & 7))) & 1));

            if (nibble & 0x80)
            {
                nibbles.push_back (nibble);
                nibble = 0;
            }
        }

        for (size_t i = 0; i + 4 < nibbles.size(); i++)
        {
            if (nibbles[i] == 0xD5 && nibbles[i + 1] == 0xAA && nibbles[i + 2] == 0x96)
            {
                volumes.push_back ((Byte) (((nibbles[i + 3] << 1) | 1) & nibbles[i + 4]));
            }
        }

        return volumes;
    }


    //  A DOS 3.3 volume number is in every address field, as INIT writes it,
    //  and a write afterward keeps it.
    TEST_METHOD (Create_ADosVolumeNumber_IsInEveryAddressField)
    {
        constexpr const char *  kImage = "C:\\disks\\v100.woz";
        FakeDiskFileIo          io;
        DiskCommandRunner       runner (io);
        CommandLineOptions      options;
        DiskImage               image;

        options.disk.command    = CommandLineOptions::DiskOptions::Command::Create;
        options.disk.imagePath  = kImage;
        options.disk.formatName = "dos";
        options.disk.volumeName = "100";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile, DiskRunnerFixtures::MakePayload());
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (DiskRunnerFixtures::MakeBinaryPutOptions (kImage)).exitStatus);

        AssertSucceeded (WozLoader::Load (io.files[kImage], image));

        for (int track = 0; track < NibbleFormat::kTrackCount; track++)
        {
            vector<Byte>  volumes = GetAddressVolumes (image, track);

            Assert::AreEqual ((size_t) NibbleFormat::kSectorsPerTrack, volumes.size());
            Assert::IsTrue   (std::all_of (volumes.begin(), volumes.end(), [] (Byte v) { return v == 100; }), std::format (L"track {}", track).c_str());
        }
    }


    //  A disk with no DOS 3.3 VTOC keeps 254.
    TEST_METHOD (Create_AProDosVolume_Has254InItsAddressFields)
    {
        constexpr const char *  kImage = "C:\\disks\\p.woz";
        FakeDiskFileIo          io;
        DiskCommandRunner       runner (io);
        CommandLineOptions      options;
        DiskImage               image;

        options.disk.command    = CommandLineOptions::DiskOptions::Command::Create;
        options.disk.imagePath  = kImage;
        options.disk.formatName = "prodos";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);
        AssertSucceeded (WozLoader::Load (io.files[kImage], image));

        Assert::IsTrue (GetAddressVolumes (image, 0) == vector<Byte> ((size_t) NibbleFormat::kSectorsPerTrack, NibbleFormat::kDefaultVolume));
    }
};
