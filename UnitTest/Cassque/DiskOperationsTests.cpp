#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "Cassque/Model/DiskOperations.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperationsTests
//
//  Byte identity with the command line, verb by verb: the runner is given
//  the command line's options over one copy of an image, the facade is asked
//  for the same operation over a second copy, and the two copies -- or the
//  two payloads -- must be identical afterwards. A refusal must carry the
//  runner's own words.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskOperationsTests)
{
public:

    using Command   = CommandLineOptions::DiskOptions::Command;
    using Encoding  = CommandLineOptions::DiskOptions::Encoding;
    using Numbering = CommandLineOptions::DiskOptions::Numbering;

    static constexpr const char *  kCliImage    = "C:\\disks\\cli.dsk";
    static constexpr const char *  kFacadeImage = "C:\\disks\\facade.dsk";
    static constexpr const char *  kCliProDos   = "C:\\disks\\cli.po";
    static constexpr const char *  kFacadeProDos = "C:\\disks\\facade.po";
    static constexpr const char *  kHostText    = "C:\\host\\notes.txt";
    static constexpr const char *  kHostBinary  = "C:\\host\\odd.bin";
    static constexpr const char *  kHostListing = "C:\\host\\applesoft.txt";
    static constexpr const char *  kCliOut      = "C:\\out\\cli";
    static constexpr const char *  kFacadeOut   = "C:\\out\\facade";



    static void Seed (FakeDiskFileIo & io, const char * fixture, const char * path)
    {
        FixtureProvider  fixtures;
        vector<Byte>     bytes;

        AssertSucceeded (fixtures.OpenFixture (fixture, bytes));

        io.files[path]  = bytes;
        io.stamps[path] = FileStamp { bytes.size(), 100 };
    }



    static void SeedBoth (FakeDiskFileIo & io)
    {
        Seed (io, "Cassque/dos33.dsk",          kCliImage);
        Seed (io, "Cassque/dos33.dsk",          kFacadeImage);
        Seed (io, "Cassque/prodos.po",          kCliProDos);
        Seed (io, "Cassque/prodos.po",          kFacadeProDos);
        Seed (io, "Cassque/Host/notes.txt",     kHostText);
        Seed (io, "Cassque/Host/odd.bin",       kHostBinary);
        Seed (io, "Cassque/Host/applesoft.txt", kHostListing);
    }



    static DiskCommandResult RunCli (FakeDiskFileIo & io, const CommandLineOptions & options)
    {
        DiskCommandRunner  runner (io);

        return runner.Run (options);
    }



    //  A clean run, or the runner's own diagnostics as the failure text.
    static void AssertCleanCli (const DiskCommandResult & result)
    {
        std::wstring  why (result.diagnostics.begin(), result.diagnostics.end());

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, why.c_str());
    }



    static void AssertCleanFacade (const DiskOperations::Result & result)
    {
        std::wstring  why (result.message.begin(), result.message.end());

        Assert::IsTrue (result.Succeeded(), why.c_str());
    }



    static void AssertImagesIdentical (FakeDiskFileIo & io, const char * cli, const char * facade)
    {
        Assert::IsTrue (io.files[cli] == io.files[facade], L"the two images must be byte identical");
        Assert::IsTrue (io.HasNoTemporaryFiles(), L"no temporary may be left behind");
    }



    //  The runner quotes the image path in every refusal, and the two copies
    //  have different paths by construction; everything after the path must
    //  agree word for word.
    static std::string StripImagePath (const std::string & message, const char * imagePath)
    {
        std::string  text = message;
        size_t       at   = 0;

        while ((at = text.find (imagePath)) != std::string::npos)
        {
            text.replace (at, strlen (imagePath), "<image>");
        }

        return text;
    }



    TEST_METHOD (Get_EveryEncoding_ProducesTheSameFile)
    {
        const Encoding  encodings[] = { Encoding::Verbatim, Encoding::Text, Encoding::Basic };
        const char *    names[]     = { "PICTURE", "NOTES", "HELLO" };
        FakeDiskFileIo  io;
        DiskOperations  ops (io);
        size_t          i = 0;

        SeedBoth (io);

        for (i = 0; i < std::size (encodings); i++)
        {
            CommandLineOptions      options   = DiskOperations::MakeOptions (Command::Get, kCliImage);
            DiskCommandResult       cli;
            DiskOperations::Result  facade;
            std::string             cliOut    = std::string (kCliOut) + std::to_string (i);
            std::string             facadeOut = std::string (kFacadeOut) + std::to_string (i);

            options.disk.path     = names[i];
            options.disk.encoding = encodings[i];
            options.disk.hostFile = cliOut;

            cli    = RunCli (io, options);
            facade = ops.Get (kFacadeImage, names[i], encodings[i], facadeOut);

            Assert::AreEqual (DiskCommandResult::kClean, cli.exitStatus);
            Assert::IsTrue   (facade.Succeeded());
            Assert::IsTrue   (io.files[cliOut] == io.files[facadeOut], L"the extracted files must match");
            Assert::AreEqual (cli.diagnostics, facade.message);
        }
    }



    TEST_METHOD (Get_WithoutAFile_YieldsTheSamePayload)
    {
        FakeDiskFileIo          io;
        DiskOperations          ops (io);
        CommandLineOptions      options = DiskOperations::MakeOptions (Command::Get, kCliImage);
        DiskCommandResult       cli;
        DiskOperations::Result  facade;

        SeedBoth (io);

        options.disk.path     = "HELLO";
        options.disk.encoding = Encoding::Basic;

        cli    = RunCli (io, options);
        facade = ops.Get (kFacadeImage, "HELLO", Encoding::Basic, "");

        Assert::IsTrue (cli.hasPayload);
        Assert::IsTrue (cli.payload == facade.payload);
    }



    TEST_METHOD (Put_TextBinaryAndBasic_ProduceTheSameImage)
    {
        FakeDiskFileIo  io;
        DiskOperations  ops (io);

        SeedBoth (io);

        {
            CommandLineOptions  options = DiskOperations::MakeOptions (Command::Put, kCliImage);

            options.disk.hostFile = kHostText;
            options.disk.path     = "NEWTEXT";
            options.disk.encoding = Encoding::Text;

            AssertCleanCli (RunCli (io, options));
            AssertCleanFacade (ops.Put (kFacadeImage, kHostText, "NEWTEXT", "", false, 0, Encoding::Text));
        }

        AssertImagesIdentical (io, kCliImage, kFacadeImage);

        {
            CommandLineOptions  options = DiskOperations::MakeOptions (Command::Put, kCliImage);

            options.disk.hostFile       = kHostBinary;
            options.disk.path           = "NEWBIN";
            options.disk.typeName       = "B";
            options.disk.hasLoadAddress = true;
            options.disk.loadAddress    = 0x6000;

            AssertCleanCli (RunCli (io, options));
            AssertCleanFacade (ops.Put (kFacadeImage, kHostBinary, "NEWBIN", "B", true, 0x6000, Encoding::Verbatim));
        }

        AssertImagesIdentical (io, kCliImage, kFacadeImage);

        {
            CommandLineOptions  options = DiskOperations::MakeOptions (Command::Put, kCliProDos);

            options.disk.hostFile = kHostListing;
            options.disk.path     = "NEWPROG";
            options.disk.encoding = Encoding::Basic;

            AssertCleanCli (RunCli (io, options));
            AssertCleanFacade (ops.Put (kFacadeProDos, kHostListing, "NEWPROG", "", false, 0, Encoding::Basic));
        }

        AssertImagesIdentical (io, kCliProDos, kFacadeProDos);
    }



    TEST_METHOD (Delete_ProducesTheSameImageAndTheSameRefusal)
    {
        FakeDiskFileIo          io;
        DiskOperations          ops (io);
        CommandLineOptions      options = DiskOperations::MakeOptions (Command::Delete, kCliImage);
        DiskCommandResult       cli;
        DiskOperations::Result  facade;

        SeedBoth (io);

        options.disk.path = "NOTES";

        AssertCleanCli (RunCli (io, options));
        AssertCleanFacade (ops.Delete (kFacadeImage, "NOTES"));

        AssertImagesIdentical (io, kCliImage, kFacadeImage);

        options.disk.path = "NOTES";   // gone now

        cli    = RunCli (io, options);
        facade = ops.Delete (kFacadeImage, "NOTES");

        Assert::AreEqual (DiskCommandResult::kNoOutput, cli.exitStatus);
        Assert::IsFalse  (facade.Succeeded());
        Assert::AreEqual (StripImagePath (cli.diagnostics, kCliImage), StripImagePath (facade.message, kFacadeImage),
                          L"the refusal carries the runner's words");
    }



    TEST_METHOD (Boot_RefusesIdentically)
    {
        //  The fixture carries no operating system, so both paths refuse; the
        //  refusal text is what is compared.
        FakeDiskFileIo          io;
        DiskOperations          ops (io);
        CommandLineOptions      options = DiskOperations::MakeOptions (Command::Boot, kCliImage);
        DiskCommandResult       cli;
        DiskOperations::Result  facade;

        SeedBoth (io);

        options.disk.path = "HELLO";

        cli    = RunCli (io, options);
        facade = ops.Boot (kFacadeImage, "HELLO");

        Assert::AreEqual (cli.exitStatus, facade.exitStatus);
        Assert::AreEqual (StripImagePath (cli.diagnostics, kCliImage), StripImagePath (facade.message, kFacadeImage));
        AssertImagesIdentical (io, kCliImage, kFacadeImage);
    }



    //  One format per container that can hold it: a DOS-order container
    //  takes DOS 3.3 or unformatted, a ProDOS-order one takes ProDOS, and
    //  the create path picks the container from the new name's extension.
    struct NewDiskCase
    {
        const char *  format;
        const char *  createCli;
        const char *  createFacade;
        const char *  initCli;
        const char *  initFacade;
    };

    TEST_METHOD (CreateAndInit_EveryFormat_ProduceTheSameImage)
    {
        const NewDiskCase  cases[] =
        {
            { "dos33",  "C:\\new\\cli.dsk", "C:\\new\\facade.dsk", kCliImage,  kFacadeImage  },
            { "prodos", "C:\\new\\cli.po",  "C:\\new\\facade.po",  kCliProDos, kFacadeProDos },
            { "none",   "C:\\new\\cli.woz", "C:\\new\\facade.woz", kCliImage,  kFacadeImage  },
        };
        FakeDiskFileIo     io;
        DiskOperations     ops (io);

        for (const NewDiskCase & one : cases)
        {
            CommandLineOptions              create = DiskOperations::MakeOptions (Command::Create, one.createCli);
            CommandLineOptions              init   = DiskOperations::MakeOptions (Command::Init, one.initCli);
            DiskOperations::NewDiskRequest  request;

            SeedBoth (io);

            request.formatName     = one.format;
            create.disk.formatName = one.format;
            init.disk.formatName   = one.format;

            AssertCleanCli (RunCli (io, create));
            AssertCleanFacade (ops.Create (one.createFacade, request));
            AssertImagesIdentical (io, one.createCli, one.createFacade);

            AssertCleanCli (RunCli (io, init));
            AssertCleanFacade (ops.Init (one.initFacade, request));
            AssertImagesIdentical (io, one.initCli, one.initFacade);

            io.files.clear();
            io.stamps.clear();
        }
    }



    TEST_METHOD (SectorAndBlockReadWrite_ProduceTheSameBytes)
    {
        FakeDiskFileIo  io;
        DiskOperations  ops (io);

        SeedBoth (io);

        {
            CommandLineOptions  options = DiskOperations::MakeOptions (Command::SectorRead, kCliImage);

            options.disk.numbering = Numbering::Logical;
            options.disk.track     = 17;
            options.disk.sector    = 0;
            options.disk.count     = 2;
            options.disk.hostFile  = "C:\\out\\cli.sec";

            AssertCleanCli (RunCli (io, options));
            AssertCleanFacade (ops.SectorRead (kFacadeImage, Numbering::Logical, 17, 0, 2, "C:\\out\\facade.sec"));
            Assert::IsTrue   (io.files["C:\\out\\cli.sec"] == io.files["C:\\out\\facade.sec"]);
        }

        {
            CommandLineOptions  options = DiskOperations::MakeOptions (Command::SectorWrite, kCliImage);

            options.disk.hostFile  = kHostBinary;
            options.disk.numbering = Numbering::Physical;
            options.disk.track     = 3;
            options.disk.sector    = 5;

            AssertCleanCli (RunCli (io, options));
            AssertCleanFacade (ops.SectorWrite (kFacadeImage, kHostBinary, Numbering::Physical, 3, 5));
            AssertImagesIdentical (io, kCliImage, kFacadeImage);
        }

        {
            CommandLineOptions  options = DiskOperations::MakeOptions (Command::BlockRead, kCliProDos);

            options.disk.block    = 2;
            options.disk.count    = 4;
            options.disk.hostFile = "C:\\out\\cli.blk";

            AssertCleanCli (RunCli (io, options));
            AssertCleanFacade (ops.BlockRead (kFacadeProDos, 2, 4, "C:\\out\\facade.blk"));
            Assert::IsTrue   (io.files["C:\\out\\cli.blk"] == io.files["C:\\out\\facade.blk"]);
        }

        {
            CommandLineOptions  options = DiskOperations::MakeOptions (Command::BlockWrite, kCliProDos);

            options.disk.hostFile = kHostBinary;
            options.disk.block    = 100;

            AssertCleanCli (RunCli (io, options));
            AssertCleanFacade (ops.BlockWrite (kFacadeProDos, kHostBinary, 100));
            AssertImagesIdentical (io, kCliProDos, kFacadeProDos);
        }
    }



    TEST_METHOD (WritePayload_MatchesARawPutOfTheSameBytes)
    {
        //  A copy between two disks lays the bytes down through the volume
        //  with no conversion; a command-line put of the same bytes with the
        //  same type and address must land identically.
        FakeDiskFileIo      io;
        DiskOperations      ops (io);
        FilePayload         payload;
        CommandLineOptions  options = DiskOperations::MakeOptions (Command::Put, kCliImage);

        SeedBoth (io);

        Assert::IsTrue (ops.Read (kFacadeImage, "ODD", payload).Succeeded());

        io.files["C:\\host\\raw.bin"]  = payload.bytes;
        io.stamps["C:\\host\\raw.bin"] = FileStamp { payload.bytes.size(), 1 };

        options.disk.hostFile       = "C:\\host\\raw.bin";
        options.disk.path           = "COPY";
        options.disk.typeName       = "B";
        options.disk.hasLoadAddress = true;
        options.disk.loadAddress    = payload.loadAddress;

        AssertCleanCli (RunCli (io, options));
        AssertCleanFacade (ops.WritePayload (kFacadeImage, "COPY", payload));

        AssertImagesIdentical (io, kCliImage, kFacadeImage);
    }



    TEST_METHOD (ListReadAndRename_GoThroughTheVolume)
    {
        FakeDiskFileIo          io;
        DiskOperations          ops (io);
        VolumeListing           listing;
        VolumeKind              kind = VolumeKind::Unknown;
        DiskOperations::Result  result;

        SeedBoth (io);

        AssertCleanFacade (ops.List (kFacadeImage, listing, kind));
        Assert::IsTrue   (kind == VolumeKind::Dos33);
        Assert::AreEqual ((size_t) 7, listing.entries.size());

        AssertCleanFacade (ops.Rename (kFacadeImage, "NOTES", "MEMO"));
        AssertCleanFacade (ops.List (kFacadeImage, listing, kind));
        Assert::AreEqual (std::string ("MEMO"), listing.entries[1].name);

        result = ops.Rename (kFacadeImage, "MEMO", "HELLO");

        Assert::IsFalse (result.Succeeded());
        Assert::IsTrue  (result.message.find ("already the name") != std::string::npos);
        Assert::IsTrue  (io.HasNoTemporaryFiles());
    }
};
