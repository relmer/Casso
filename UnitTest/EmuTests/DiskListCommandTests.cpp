#include "Pch.h"

#include "Machines/Apple2/Common/NibbleFormat.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/HfsWriter.h"
#include "Core/TextEncoding.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskListCommandTests
//
//  disk list, mkdir and rmdir: catalog rows, columns, recursive listings,
//  paths and directories.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskListCommandTests)
{
public:

    TEST_METHOD (List_RealDisk_RendersEveryCatalogEntryIncludingHeadings)
    {
        // The vendor's own printed catalog shows all sixty-three rows, twenty
        // of which occupy no sectors and exist to draw section headings. This
        // listing renders them for the same reason DOS does: they are what
        // their author put on the disk to be seen, and hiding them would make
        // this output disagree with the machine's.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        size_t             lines = 0;
        size_t             at    = 0;

        DiskRunnerFixtures::SeedRealDisk (io);

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
            L"a disk that shipped must list cleanly");

        Assert::IsTrue (result.output.find ("DISK VOLUME 254") != std::string::npos,
            L"the volume number is part of the listing");

        Assert::IsTrue (result.output.find ("MERLIN PRO - DOS 3.3") != std::string::npos,
            L"heading rows are rendered, not filtered");

        Assert::IsTrue (result.output.find ("*T 000 ") != std::string::npos,
            L"and they render in the shape the guest prints: locked, type T, zero sectors");

        Assert::IsTrue (result.output.find (" A 004 HELLO") != std::string::npos,
            L"alongside real files in the same format");

        while ((at = result.output.find ('\n', at)) != std::string::npos)
        {
            lines++;
            at++;
        }

        // 63 entries, a volume header line, a blank, a blank and a free-space
        // line. The exact total matters less than that no entry was dropped.
        Assert::IsTrue (lines >= 63, L"every catalog entry must appear");
    }

    TEST_METHOD (List_ReportsFreeSpace)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedRealDisk (io);

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::IsTrue (result.output.find ("sectors free of 560") != std::string::npos,
            L"the listing states free space against the volume's capacity");
    }

    TEST_METHOD (List_ShowsAnIndexOnlyBesideNamesSeveralEntriesShare)
    {
        // Merlin's disk has ten heading entries of one name and two pairs of
        // another; those fourteen rows are listed with an index, and no other row is.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        size_t             marks = 0;
        size_t             at    = 0;

        DiskRunnerFixtures::SeedRealDisk (io);

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        for (at = result.output.find ("  (index "); at != std::string::npos; at = result.output.find ("  (index ", at + 1))
        {
            marks++;
        }

        Assert::AreEqual ((size_t) 14, marks);
        Assert::IsTrue   (result.output.find ("HELLO  (index") == std::string::npos, L"a unique name shows no index");
    }


    TEST_METHOD (Mkdir_MakesEveryDirectoryAlongThePath_AndARepeatIsRefused)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;

        DiskRunnerFixtures::SeedProDosDisk (io);

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir);
        options.disk.path = "CASSO/DEEP";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, L"both directories are made");

        //  The image the runner wrote is what the next command reads.
        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List);
        options.disk.path = "CASSO";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue   (result.output.find ("DEEP") != std::string::npos,
            L"and the deeper one is inside the first");

        //  Asking again for something that is there in full is an error.
        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir);
        options.disk.path = "CASSO/DEEP";
        result            = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("is already on this volume") != std::string::npos);
    }


    //  A recursive listing's name column is as wide as its longest path, so
    //  every type lines up.
    TEST_METHOD (List_Recurse_AlignsTheColumnsToTheLongestPath)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;
        std::istringstream  lines;
        std::string         line;
        size_t              column = std::string::npos;
        size_t              rows   = 0;

        DiskRunnerFixtures::SeedProDosDisk (io);

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir);
        options.disk.path = "GRAPHICS.FOLDER/PRINTSHOP.ITEMS";
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        options              = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List);
        options.disk.recurse = true;
        result               = runner.Run (options);
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        lines.str (result.output);

        while (std::getline (lines, line))
        {
            size_t  type = line.find (" $");

            if (line.find ("eof=") != std::string::npos)
            {
                Assert::IsTrue (column == std::string::npos || type == column, TextEncoding::NarrowToWide (line).c_str());
                Assert::IsTrue (line.find ("PRINTSHOP.ITEMSDIR") == std::string::npos);

                column = type;
                rows++;
            }
        }

        Assert::IsTrue (rows > 2);
    }


    TEST_METHOD (Mkdir_OnADos33Disk_SaysTheFilesystemHasNoDirectories)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;

        DiskRunnerFixtures::SeedRealDisk (io);

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir);
        options.disk.path = "CASSO";
        result            = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("Unable to create CASSO: this volume is DOS 3.3, which has no directories") != std::string::npos);
    }


    TEST_METHOD (Rmdir_ADirectoryHoldingThings_NeedsRecurse_ThenListsAndAsks)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;
        std::string         asked;

        DiskRunnerFixtures::SeedProDosDisk (io);

        //  A directory with something in it.
        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir);
        options.disk.path = "CASSO/DEEP";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        //  Without --recurse it is turned down, and the flag is named.
        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Rmdir);
        options.disk.path = "CASSO";
        result            = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("--recurse") != std::string::npos);

        //  With nobody to ask, it is turned down as well, and the list is shown.
        options.disk.recurse = true;
        result               = runner.Run (options);

        Assert::IsTrue (result.output.find ("CASSO/DEEP")   != std::string::npos, L"the list names what would go");
        Assert::IsTrue (result.output.find ("entries, ")    != std::string::npos, L"and totals it");
        Assert::IsTrue (result.diagnostics.find ("--yes")   != std::string::npos);

        //  An asker that says no leaves the directory where it is.
        runner.SetConfirmAsker ([&asked] (const std::string & question) { asked = question; return false; });

        result = runner.Run (options);

        Assert::IsTrue (asked.find ("CASSO") != std::string::npos, L"the question names the directory");
        Assert::IsTrue (result.diagnostics.find ("was not removed") != std::string::npos);

        //  One that says yes removes the subtree.
        runner.SetConfirmAsker ([] (const std::string &) { return true; });

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List);
        options.disk.path = "CASSO";
        result            = runner.Run (options);

        Assert::AreNotEqual (DiskCommandResult::kClean, result.exitStatus,
            L"the directory is gone, so listing it fails");
    }


    //  HFS has folders, as ProDOS has directories: mkdir makes each along
    //  the path, and rmdir removes them.
    TEST_METHOD (MkdirAndRmdir_ChangeAnHfsVolume)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;
        vector<Byte>        image;



        AssertSucceeded (HfsWriter::Format (image, 1600, "Mac Disk"));
        io.files["mac.po"]  = image;
        io.stamps["mac.po"] = FileStamp { image.size(), 100 };

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir, "mac.po");
        options.disk.path = "Work/Deep Folder";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List, "mac.po");
        options.disk.path = "Work";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());
        Assert::IsTrue   (result.output.find ("Deep Folder") != std::string::npos, L"the deeper folder is inside the first");

        options              = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Rmdir, "mac.po");
        options.disk.path    = "Work";
        options.disk.recurse = true;
        options.disk.yes     = true;
        result               = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List, "mac.po");
        options.disk.path = "Work";
        result            = runner.Run (options);

        Assert::AreNotEqual (DiskCommandResult::kClean, result.exitStatus, L"the folder is gone, so listing it fails");
    }


    TEST_METHOD (Rmdir_WithYes_NeedsNobodyToAsk)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;

        DiskRunnerFixtures::SeedProDosDisk (io);

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir);
        options.disk.path = "CASSO/DEEP";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        options              = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Rmdir);
        options.disk.path    = "CASSO";
        options.disk.recurse = true;
        options.disk.yes     = true;
        result               = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
    }


    TEST_METHOD (List_WithRecurse_WritesAFullPathPerRow)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;

        DiskRunnerFixtures::SeedProDosDisk (io);

        options              = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List);
        options.disk.recurse = true;
        result               = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue   (result.output.find ('/') != std::string::npos,
            L"a row below the volume directory gives the path that reaches it");
    }


    TEST_METHOD (AFullPath_NamesTheVolumeFirst_AndAnotherVolumeIsRefused)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   rooted;
        DiskCommandResult   relative;
        DiskCommandResult   wrong;
        CommandLineOptions  options;

        DiskRunnerFixtures::SeedProDosDisk (io);

        //  `/MERLIN` is this image, so a path through it reaches the volume
        //  directory exactly as a relative one does.
        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List);
        options.disk.path = "/MERLIN";
        rooted            = runner.Run (options);

        options.disk.path = "";
        relative          = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, rooted.exitStatus);
        Assert::AreEqual (relative.output, rooted.output, L"the two forms name one directory");

        //  A full path naming another volume is acting on the wrong disk.
        options.disk.path = "/NOTTHISONE/SOURCE";
        wrong             = runner.Run (options);

        Assert::AreNotEqual (DiskCommandResult::kClean, wrong.exitStatus);
        Assert::IsTrue      (wrong.diagnostics.find ("is a path on another volume") != std::string::npos);
        Assert::IsTrue      (wrong.diagnostics.find ("this image is /MERLIN")       != std::string::npos);
    }


    TEST_METHOD (ADos33NameKeepsItsSlashes)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;

        DiskRunnerFixtures::SeedRealDisk (io);

        //  DOS 3.3 has no directories and no volume names, so a leading slash
        //  is part of the name. This disk holds a file called /HELLO, and
        //  reading it is the proof that nothing stripped the slash.
        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        options.disk.path = "/HELLO";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue   (result.hasPayload);
        Assert::IsTrue   (result.diagnostics.find ("this image is") == std::string::npos,
            L"and no volume was read out of the name");
    }


    TEST_METHOD (List_SaysNotBootable_OnlyWhenTheBootTracksAreEmpty)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        vector<Byte>       stripped;


        DiskRunnerFixtures::SeedRealDisk (io);

        // The real disk carries DOS and says nothing about booting.
        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));
        Assert::IsTrue (result.output.find ("Not bootable") == std::string::npos,
            L"a disk with DOS on its boot tracks is not called unbootable");

        // The same disk with tracks 0 to 2 zeroed is a data disk: its catalog
        // is intact, and the boot ROM would find a BRK at $0801.
        stripped = io.files[DiskRunnerFixtures::kImage];
        std::fill (stripped.begin(), stripped.begin() + 3 * 16 * 256, (Byte) 0);
        io.files[DiskRunnerFixtures::kImage] = stripped;

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find ("sectors free of 560") != std::string::npos,
            L"the listing itself is unchanged");
        Assert::IsTrue (result.output.find ("Not bootable: no DOS on tracks 0 to 2") != std::string::npos,
            L"and it says the disk cannot boot");
    }


    TEST_METHOD (List_ProDosVolume_UsesItsOwnShape)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", "C:\\disks\\merlin.po.dsk");

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                          "C:\\disks\\merlin.po.dsk"));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find ("/MERLIN") != std::string::npos,
            L"a ProDOS volume has a name, not a number");
        Assert::IsTrue (result.output.find ("blocks free of 280") != std::string::npos,
            L"and counts in blocks rather than sectors");
        Assert::IsTrue (result.output.find ("Not bootable") == std::string::npos,
            L"a volume carrying PRODOS is not called unbootable");
    }

    //
    //  EVERY COLUMN THE VOLUME RECORDS, WITH NO FLAG TO ASK FOR IT.
    //
    //  `eof=` and `aux=` sat behind `--long`, and ProDosVolume::Enumerate fills
    //  both whether or not anybody asks -- so the flag bought nothing and cost
    //  a reading of the help plus a second run of the command. They are the two
    //  fields a build loop most wants, which is what made hiding them worst:
    //  the exact length of a file and the address a binary loads at are exactly
    //  what you check after placing one.
    //
    TEST_METHOD (List_OfAProDosVolume_CarriesEofAndAux_WithNoFlagToAskForThem)
    {
        static constexpr const char *  kProDosImage = "C:\\disks\\pro.dsk";

        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                                   kProDosImage);
        DiskCommandResult   result;

        DiskRunnerFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", kProDosImage);

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        //  A whole rendered row rather than a substring search for `eof=`: the
        //  columns have to land in their places, and the row has to stay inside
        //  80 characters now that nothing can turn them off.
        Assert::IsTrue (result.output.find ("*MERLIN.SYSTEM        $FF    37  eof=18432 aux=$2000\n")
                            != std::string::npos,
            L"the ProDOS row, with both columns, unasked for");

        DiskRunnerFixtures::AssertEveryLineFitsEightyColumns (result.output);
    }

    //  DOS 3.3 records neither field and has its own formatter, so it must be
    //  untouched by the ProDOS columns becoming unconditional.
    TEST_METHOD (List_OfADos33Volume_IsUnchangedByTheProDosColumns)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;

        DiskRunnerFixtures::SeedRealDisk (io);

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find (" B 004 MERLIN\n") != std::string::npos,
            L"the DOS 3.3 row is what a booted machine prints");
        Assert::IsTrue (result.output.find ("eof=") == std::string::npos,
            L"and carries no field this filesystem does not record");
    }


    //  A 143,360-byte sector image carrying boot code and no filesystem at all,
    //  which is what a great deal of Apple II software actually is.
    static vector<Byte> MakeBootableImageWithNoFilesystem()
    {
        vector<Byte>  image ((size_t) NibbleFormat::kImageByteSize, 0);
        size_t        i     = 0;

        // Track 0 sector 0 is what the drive's ROM reads and jumps into.
        for (i = 0; i < 64; i++)
        {
            image[i] = static_cast<Byte> (0xA9 + (i & 0x0F));
        }

        return image;
    }

    TEST_METHOD (List_ImageWithNoFilesystem_SaysSoInPlainWords_AndThenSaysWhatItCanTell)
    {
        //  TWELVE OF FOURTEEN REAL IMAGES LAND HERE, this project's own demo
        //  disk among them, and every one of them boots. Answering with the
        //  negative alone describes a working disk as though it were unreadable.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        const char *        path = "C:\\disks\\casso-rocks.dsk";
        vector<Byte>        image = MakeBootableImageWithNoFilesystem();

        io.files[path]  = image;
        io.stamps[path] = FileStamp { image.size(), 100 };

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List, path));

        Assert::IsTrue (result.diagnostics.find (DiskImageSession::kNoFilesystemText)
                            != std::string::npos,
            L"the sentence a person would say, not a sentence about this tool's tables");

        Assert::IsTrue (result.diagnostics.find ("filesystem this tool recognizes")
                            == std::string::npos,
            L"and not the old one");

        Assert::IsTrue (result.diagnostics.find ("35 tracks, 16 sectors, 256 bytes per sector")
                            != std::string::npos,
            L"the geometry is still knowable and is still worth stating");

        Assert::IsTrue (result.diagnostics.find ("sector 0 contains boot code")
                            != std::string::npos,
            L"and so is the fact that it boots");

        //  THE STATUS DOES NOT MOVE. A caller still got no catalog, so a script
        //  that branches on 2 branches the same way it always did.
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"a survey is not a listing");

        Assert::AreEqual (std::string(), result.output,
            L"and it goes to the error stream, so nothing appears in a pipe");
    }

    TEST_METHOD (List_ImageWithNoFilesystemAndNoBootCode_SaysThatInsteadOfClaimingItBoots)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        const char *        path  = "C:\\disks\\blank.dsk";
        vector<Byte>        image ((size_t) NibbleFormat::kImageByteSize, 0);

        io.files[path]  = image;
        io.stamps[path] = FileStamp { image.size(), 100 };

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List, path));

        Assert::IsTrue (result.diagnostics.find ("sector 0 is blank")
                            != std::string::npos,
            L"an empty first sector is reported as empty, not as bootable");
    }
};
