#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "HeadlessHost.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/Dos33Skeleton.h"
#include "Devices/Disk/Dos33Volume.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/ProDosVolume.h"
#include "MachineIdle.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleBootTests
//
//  What a real 6502 does with a disk this tool was asked to boot into a
//  program.
//
//  Nothing else can answer this. The DOS 3.3 mechanism is thirty bytes inside
//  DOS's own image and the ProDOS one is the order of two directory records;
//  neither appears in a listing, neither is a file, and both are read by code
//  we did not write. Our own reader can only restate what we wrote. So the
//  witness is the machine: boot the image, TYPE NOTHING, and see whose program
//  is on the screen.
//
//  TYPING NOTHING IS THE POINT. US4's whole claim is that the program runs
//  unattended, so a test that helped the guest along would be testing something
//  else. That also sidesteps the pager hazard the placement gate carries -- no
//  keystroke is ever sent here.
//
//  The cheap questions come first, as they do beside this: the written
//  container is decoded through the DRIVE and re-read before any machine
//  starts, so an image written wrongly is reported in milliseconds rather than
//  by a 6502 executing whatever it managed to load.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (GuestVisibleBootTests)
{
public:

    static constexpr const char *  kImagePath = "C:\\disks\\boot.dsk";
    static constexpr const char *  kHostFile  = "C:\\build\\greet.bin";

    //  /APPLESOFT: the ProDOS fixture that boots PRODOS and then the first
    //  system program its volume directory reaches, which is BASIC.SYSTEM
    //  until this feature moves something in front of it.
    static constexpr const char *  kProDosFixture = "Disks/Merlin-proProdos2.33-b.dsk";

    static constexpr const char *  kDosProgram = "GREET";
    static constexpr const char *  kProProgram = "CASSO.SYSTEM";

    //  What the placed program prints. Nothing on either disk prints this, so
    //  seeing it means our code ran rather than somebody else's.
    static constexpr const char *  kBanner = "CASSO BOOT";

    //  DOS 3.3 BRUNs a binary greeting at the address stored in the file;
    //  ProDOS loads a system program at $2000 and jumps to it.
    static constexpr Word  kDosLoadAddress = 0x6000;
    static constexpr Word  kProLoadAddress = 0x2000;

    //  Where a booted DOS reads the name of the program it runs, restated from
    //  the published layout rather than borrowed from the code under test.
    static constexpr int     kGreetingTrack  = 1;
    static constexpr int     kGreetingSector = 9;
    static constexpr size_t  kGreetingOffset = 0x75;
    static constexpr size_t  kNameFieldBytes = 30;

    //  What each disk runs when nothing has been set: the stock master's own
    //  greeting, and the system program /APPLESOFT reaches first.
    static constexpr const char *  kMasterGreetingName = "HELLO";
    static constexpr const char *  kProDosLaunchesNow  = "BASIC.SYSTEM";

    //  Monitor entry points the placed program calls. SETTXT and HOME because
    //  it has to make itself a screen to write on; COUT1 rather than COUT
    //  because COUT goes through an output hook that nothing has set up.
    static constexpr Word  kSetTextScreen = 0xFB39;
    static constexpr Word  kHomeScreen    = 0xFC58;
    static constexpr Word  kCharacterOut  = 0xFDF0;

    static constexpr Byte  kReturnChar = 0x8D;
    static constexpr Byte  kHighBit    = 0x80;

    //  Two bytes at an address neither operating system uses for itself. Their
    //  presence afterwards is the evidence that the placed program RAN.
    static constexpr Word  kSignatureAddress = 0x0300;
    static constexpr Byte  kSignatureFirst   = 0x5A;
    static constexpr Byte  kSignatureSecond  = 0xA5;

    //  Where the program stops, and where its text begins, both relative to
    //  wherever it was loaded. Asserted against the assembled length below, so
    //  an edit to the code cannot silently move either.
    static constexpr Word  kHangOffset = 0x1D;
    static constexpr Word  kTextOffset = 0x20;

    //  Applesoft, for the greeting DOS 3.3 actually runs: two tokens, a line
    //  number, and where a program sits once loaded.
    static constexpr Byte  kPokeToken    = 0xB9;
    static constexpr Byte  kPrintToken   = 0xBA;
    static constexpr Byte  kLineNumber   = 10;
    static constexpr Word  kProgramStart = 0x0801;

    //  6502 that puts a banner on the screen, leaves a signature in memory and
    //  then stops. This is the PRODOS half's program: the kernel loads a system
    //  program at $2000 and jumps to it, so machine code is what it launches.
    //
    //  IT INITIALIZES THE TEXT SCREEN ITSELF, which is not decoration. The
    //  harness enters at the boot ROM rather than through RESET, so the
    //  monitor's cold start has never run and the text window, the cursor and
    //  the output hook are all whatever powering on left in zero page. A
    //  program that printed through COUT would write through an uninitialized
    //  hook into memory that is not the screen -- measured, and the screen then
    //  shows nothing but the noise it powered up with.
    //
    //  THE SIGNATURE IS THE PRIMARY WITNESS. What is on the screen can be put
    //  there by anything; two bytes at a fixed address are put there only by
    //  the instructions that write them, so they say the program EXECUTED
    //  rather than merely got loaded.
    //
    //  Stopping matters too: whatever is on the screen afterwards is what this
    //  program put there, with nothing running after it to add or clear.
    static std::vector<Byte> MakeSystemProgram (Word loadAddress)
    {
        std::vector<Byte>  code;
        Word               textAt = (Word) (loadAddress + kTextOffset);
        Word               hangAt = (Word) (loadAddress + kHangOffset);
        std::string        banner = kBanner;
        size_t             i      = 0;

        code.push_back (0x20);   code.push_back ((Byte) (kSetTextScreen & 0xFF));
        code.push_back ((Byte) (kSetTextScreen >> 8));                       // JSR SETTXT
        code.push_back (0x20);   code.push_back ((Byte) (kHomeScreen & 0xFF));
        code.push_back ((Byte) (kHomeScreen >> 8));                          // JSR HOME

        code.push_back (0xA2);   code.push_back (0x00);                      // LDX #$00
        code.push_back (0xBD);   code.push_back ((Byte) (textAt & 0xFF));    // LDA text,X
        code.push_back ((Byte) (textAt >> 8));
        code.push_back (0xF0);   code.push_back (0x06);                      // BEQ done
        code.push_back (0x20);   code.push_back ((Byte) (kCharacterOut & 0xFF));
        code.push_back ((Byte) (kCharacterOut >> 8));                        // JSR COUT1
        code.push_back (0xE8);                                               // INX
        code.push_back (0xD0);   code.push_back (0xF5);                      // BNE loop

        code.push_back (0xA9);   code.push_back (kSignatureFirst);           // done: LDA #
        code.push_back (0x8D);   code.push_back ((Byte) (kSignatureAddress & 0xFF));
        code.push_back ((Byte) (kSignatureAddress >> 8));                    // STA sig
        code.push_back (0xA9);   code.push_back (kSignatureSecond);          // LDA #
        code.push_back (0x8D);   code.push_back ((Byte) ((kSignatureAddress + 1) & 0xFF));
        code.push_back ((Byte) ((kSignatureAddress + 1) >> 8));              // STA sig+1

        Assert::AreEqual (size_t (kHangOffset), code.size(),
            L"the hang has to be where the jump says it is");

        code.push_back (0x4C);   code.push_back ((Byte) (hangAt & 0xFF));    // JMP self
        code.push_back ((Byte) (hangAt >> 8));

        Assert::AreEqual (size_t (kTextOffset), code.size(),
            L"and the text has to start where the code says it does");

        code.push_back (kReturnChar);

        for (i = 0; i < banner.size(); i++)
        {
            code.push_back ((Byte) (banner[i] | kHighBit));
        }

        code.push_back (kReturnChar);
        code.push_back (0);

        return code;
    }

    //  The DOS 3.3 half's program, and it is Applesoft rather than machine code
    //  for a measured reason: a booting DOS 3.3 RUNs its greeting. A binary
    //  named in that field is never executed -- see the case below, where the
    //  guest is the one that says so.
    //
    //  Tokenized by hand. There is no tokenizer in this build, and one line is
    //  small enough to read: a link to the next line, the line number, PRINT
    //  with a quoted string, and two POKEs that leave the same signature the
    //  system program leaves. The on-disk form is what the shipped writer's own
    //  HELLO uses -- two length bytes ahead of the program, which the placement
    //  path adds -- so only the program itself is built here.
    static void AppendText (std::vector<Byte> & inOutProgram, const std::string & text)
    {
        size_t  i = 0;

        for (i = 0; i < text.size(); i++)
        {
            inOutProgram.push_back ((Byte) text[i]);
        }
    }

    static std::vector<Byte> MakeBasicGreeting()
    {
        std::vector<Byte>  program;
        std::string        quote  = "\"";
        Word               nextAt = 0;

        //  10 PRINT "CASSO BOOT": POKE 768,90: POKE 769,165
        program.push_back (0);   program.push_back (0);         // link, filled in below
        program.push_back (kLineNumber);   program.push_back (0);

        program.push_back (kPrintToken);
        AppendText (program, quote + kBanner + quote + ":");

        program.push_back (kPokeToken);
        AppendText (program, std::to_string (kSignatureAddress) + ","
                           + std::to_string (kSignatureFirst) + ":");

        program.push_back (kPokeToken);
        AppendText (program, std::to_string (kSignatureAddress + 1) + ","
                           + std::to_string (kSignatureSecond));

        program.push_back (0);                                  // end of line

        //  Applesoft chains its lines by ADDRESS, not by length, so the link is
        //  where this line would end once loaded at the start of program space.
        nextAt = (Word) (kProgramStart + program.size());

        program[0] = (Byte) (nextAt & 0xFF);
        program[1] = (Byte) (nextAt >> 8);

        program.push_back (0);   program.push_back (0);         // end of program

        return program;
    }

    static std::vector<Byte> ProDosFixtureBytes()
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;

        AssertSucceeded (fixtures.OpenFixture (kProDosFixture, bytes));

        Assert::AreEqual (GuestSession::kImageBytes, bytes.size(),
            L"the ProDOS fixture must be a whole 5.25-inch sector image");

        return bytes;
    }


    //
    //  ------------------------------------------------------------------
    //  The command path a user actually travels: put, then boot.
    //  ------------------------------------------------------------------
    //

    static CommandLineOptions MakeOptions (CommandLineOptions::DiskOptions::Command command)
    {
        CommandLineOptions  options;

        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = command;
        options.disk.imagePath = kImagePath;

        return options;
    }

    //  Places the program, and optionally tells the disk to boot into it.
    //  Both through the runner, over an in-memory platform; the bytes handed
    //  back are what a commit actually wrote.
    //
    //  THE OPTION IS THE CONTROL. Booting the same disk WITHOUT the boot command
    //  is what proves the evidence below comes from the setting rather than
    //  from anything else on the disk: the program is present either way, so a
    //  test that only ever booted the configured image could not tell "the
    //  startup program was set" from "this disk runs that program anyway".
    static std::vector<Byte> CommitProgram (const std::vector<Byte>  & image,
                                            const std::vector<Byte>  & program,
                                            const char               * name,
                                            const char               * typeName,
                                            Word                       loadAddress,
                                            bool                       hasLoadAddress,
                                            bool                       alsoSetStartup,
                                            int                        bootStatus)
    {
        FakeDiskFileIo      io;
        CommandLineOptions  put  = MakeOptions (CommandLineOptions::DiskOptions::Command::Put);
        CommandLineOptions  boot = MakeOptions (CommandLineOptions::DiskOptions::Command::Boot);

        io.files[kImagePath]  = image;
        io.stamps[kImagePath] = FileStamp { image.size(), 100 };
        io.files[kHostFile]   = program;
        io.stamps[kHostFile]  = FileStamp { program.size(), 100 };

        put.disk.hostFile       = kHostFile;
        put.disk.path           = name;
        put.disk.typeName       = typeName;
        put.disk.loadAddress    = loadAddress;
        put.disk.hasLoadAddress = hasLoadAddress;

        boot.disk.path = name;

        {
            DiskCommandRunner  runner (io);

            Assert::AreEqual (DiskCommandRunner::kClean, runner.Run (put).exitStatus,
                L"the placement must succeed before there is anything to boot into");
        }

        if (alsoSetStartup)
        {
            DiskCommandRunner  runner (io);
            DiskCommandResult  result = runner.Run (boot);

            Assert::AreEqual (bootStatus, result.exitStatus,
                L"and setting the startup program must end the way the case expects");
        }

        Assert::IsTrue (io.HasNoTemporaryFiles(), L"leaving no temporary beside the image");

        return io.files[kImagePath];
    }


    //
    //  ------------------------------------------------------------------
    //  The cheap questions, asked before any machine starts.
    //  ------------------------------------------------------------------
    //

    static std::vector<Byte> DecodeThroughTheDrive (const std::vector<Byte> & bytes)
    {
        DiskImage           image;
        SectorDecodeReport  report;
        std::vector<Byte>   sectors;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (bytes, image));
        AssertSucceeded (NibblizationLayer::Denibblize (image, DiskFormat::Dsk, sectors, report));

        return sectors;
    }

    //  The name a booted DOS will read, off the container the DRIVE sees.
    //  The greeting field written straight into the image, going around the
    //  runner.
    //
    //  THE RUNNER REFUSES TO PRODUCE THIS DISK, which is the whole point: what
    //  a real machine does with one is what makes refusing correct, and only a
    //  real machine can establish it. A .dsk is already sector-ordered, so the
    //  field is at the offset GreetingNameIn reads from, stored the way DOS
    //  stores a name -- high ASCII, padded with spaces to the full field.
    static std::vector<Byte> ForceGreetingTo (const std::vector<Byte> & image, const char * name)
    {
        std::vector<Byte>  forced = image;
        size_t             at     = Dos33Skeleton::SectorOffset (kGreetingTrack, kGreetingSector)
                                  + kGreetingOffset;
        std::string        text   = name;
        size_t             i      = 0;

        for (i = 0; i < kNameFieldBytes; i++)
        {
            char  c = (i < text.size()) ? text[i] : ' ';

            forced[at + i] = (Byte) (c | 0x80);
        }

        return forced;
    }




    static std::string GreetingNameIn (const std::vector<Byte> & sectors)
    {
        size_t       at = Dos33Skeleton::SectorOffset (kGreetingTrack, kGreetingSector)
                        + kGreetingOffset;
        std::string  name;
        size_t       i  = 0;

        for (i = 0; i < kNameFieldBytes; i++)
        {
            name += (char) (sectors[at + i] & 0x7F);
        }

        while (!name.empty() && name.back() == ' ')
        {
            name.pop_back();
        }

        return name;
    }

    static std::vector<std::string> NamesIn (const VolumeListing & listing)
    {
        std::vector<std::string>  names;

        for (const FileEntry & entry : listing.entries)
        {
            names.push_back (entry.name);
        }

        return names;
    }

    static size_t IndexOf (const std::vector<std::string> & names, const std::string & wanted)
    {
        size_t  i = 0;

        for (i = 0; i < names.size(); i++)
        {
            if (names[i] == wanted)
            {
                return i;
            }
        }

        Assert::Fail (L"the volume must carry the file being located");

        return 0;
    }


    //
    //  ------------------------------------------------------------------
    //  Booting, and what the machine had to say for itself afterwards.
    //  ------------------------------------------------------------------
    //

    //  Boots the image, types nothing, and reports whether the placed program
    //  ran -- by its signature, which only its own instructions can write.
    static bool DidThePlacedProgramRun (const std::vector<Byte>   & image,
                                        std::vector<std::string>  & outRows)
    {
        HeadlessHost       host;
        EmulatorCore       core;
        std::vector<Byte>  signature;

        GuestSession::MountAndBoot (host, core, image);
        GuestSession::CollectRows (core, outRows);

        signature = GuestSession::GuestBytesAt (core, kSignatureAddress, 2);

        return signature[0] == kSignatureFirst && signature[1] == kSignatureSecond;
    }


    //  Runs `disk boot` over an image and hands back what was committed. The
    //  ProDOS case needs this on its own, because its control is the same disk
    //  pointed at the OTHER program rather than a disk left alone.
    static std::vector<Byte> SetStartupTo (const std::vector<Byte> & image, const char * name)
    {
        FakeDiskFileIo      io;
        CommandLineOptions  boot = MakeOptions (CommandLineOptions::DiskOptions::Command::Boot);

        io.files[kImagePath]  = image;
        io.stamps[kImagePath] = FileStamp { image.size(), 100 };

        boot.disk.path = name;

        {
            DiskCommandRunner  runner (io);

            Assert::AreEqual (DiskCommandRunner::kClean, runner.Run (boot).exitStatus,
                L"setting the startup program must succeed");
        }

        Assert::IsTrue (io.HasNoTemporaryFiles(), L"leaving no temporary beside the image");

        return io.files[kImagePath];
    }


    //
    //  ------------------------------------------------------------------
    //  The gate.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (Dos33_ADiskToldToBootAProgram_RunsItWithNothingTyped)
    {
        std::vector<Byte>         master     = GuestSession::RequireDos33Master();
        std::vector<Byte>         greeting   = MakeBasicGreeting();
        std::vector<Byte>         placedOnly = CommitProgram (master, greeting, kDosProgram, "A",
                                                              0, false, false,
                                                              DiskCommandRunner::kClean);
        std::vector<Byte>         configured = CommitProgram (master, greeting, kDosProgram, "A",
                                                              0, false, true,
                                                              DiskCommandRunner::kClean);
        std::vector<Byte>         sectors    = DecodeThroughTheDrive (configured);
        std::vector<std::string>  rows;
        std::vector<std::string>  controlRows;
        VolumeListing             listing;
        FilePayload               placed;



        //  The cheap questions first: is this still a disk, is the program on
        //  it, and does the field a booted DOS reads name it.
        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
            AssertSucceeded (volume.Read (FilePath::Parse (kDosProgram), placed));
        }

        Assert::IsTrue (placed.bytes == greeting,
            L"the program must read back byte for byte off the container the drive sees");

        Assert::AreEqual (std::string (kMasterGreetingName), GreetingNameIn (master),
            L"the disk this started from names its own greeting");

        Assert::AreEqual (std::string (kDosProgram), GreetingNameIn (sectors),
            L"and afterwards the field names the program that was set, in the place the "
            L"published layout puts it");

        Assert::IsTrue (GuestSession::AnyRowIs (NamesIn (listing), kMasterGreetingName),
            L"while the greeting the disk shipped with is still on it: this changes what "
            L"runs, not what is on the volume");

        //  The control. Same disk, same program on it, boot NOT set.
        Assert::IsFalse (DidThePlacedProgramRun (placedOnly, controlRows),
            L"a disk that was never told to boot the program must not run it -- without "
            L"this the assertion below is satisfied by a disk that would have run it "
            L"anyway");

        Assert::IsTrue (DidThePlacedProgramRun (configured, rows),
            L"and the disk that was told to must run it, with nothing typed at it");

        Assert::IsTrue (GuestSession::AnyRowIs (rows, kBanner),
            L"printing what it prints, on the guest's own screen");

        Assert::IsFalse (GuestSession::AnyRowIs (controlRows, kBanner),
            L"which the unconfigured disk did not");
    }

    TEST_METHOD (Dos33_ABinaryForcedIntoTheGreeting_NeverRuns_WhichIsWhyItIsRefused)
    {
        //  The measurement the REFUSAL rests on, kept as a case because nothing
        //  else can establish it: a booting DOS 3.3 RUNs its greeting, so a
        //  binary in that field leaves a disk that boots and does nothing. Our
        //  own reader cannot tell that. The machine can.
        //
        //  The greeting is forced in directly, because `disk boot` will no
        //  longer write one -- it refuses a type DOS cannot run rather than
        //  configuring a disk that starts nothing. This is the disk that
        //  refusal prevents, and this is what it would have done.
        std::vector<Byte>         master   = GuestSession::RequireDos33Master();
        std::vector<Byte>         program  = MakeSystemProgram (kDosLoadAddress);
        std::vector<Byte>         placed   = CommitProgram (master, program, kDosProgram, "B",
                                                            kDosLoadAddress, true, false,
                                                            DiskCommandRunner::kClean);
        std::vector<Byte>         forced   = ForceGreetingTo (placed, kDosProgram);
        std::vector<Byte>         sectors  = DecodeThroughTheDrive (forced);
        std::vector<std::string>  rows;



        Assert::AreEqual (std::string (kDosProgram), GreetingNameIn (sectors),
            L"the greeting really does name the binary");

        Assert::IsFalse (DidThePlacedProgramRun (forced, rows),
            L"and the machine never runs it, which is what the refusal spares the user");
    }

    TEST_METHOD (ProDos_ADiskToldToBootASystemProgram_LaunchesItInsteadOfTheOldOne)
    {
        std::vector<Byte>         fixture    = ProDosFixtureBytes();
        std::vector<Byte>         program    = MakeSystemProgram (kProLoadAddress);
        std::vector<Byte>         placed     = CommitProgram (fixture, program, kProProgram, "SYS",
                                                              0, false, false,
                                                              DiskCommandRunner::kClean);
        std::vector<Byte>         theOldWay  = SetStartupTo (placed, kProDosLaunchesNow);
        std::vector<Byte>         configured = SetStartupTo (theOldWay, kProProgram);
        std::vector<Byte>         sectors    = DecodeThroughTheDrive (configured);
        std::vector<std::string>  rows;
        std::vector<std::string>  controlRows;
        std::vector<std::string>  names;
        VolumeListing             listing;
        FilePayload               onDisk;



        {
            ProDosVolume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
            AssertSucceeded (volume.Read (FilePath::Parse (kProProgram), onDisk));
        }

        names = NamesIn (listing);

        Assert::IsTrue (onDisk.bytes == program,
            L"the program must read back byte for byte off the container the drive sees");

        Assert::IsTrue (IndexOf (names, kProProgram) < IndexOf (names, kProDosLaunchesNow),
            L"and must sit in front of the program the disk launched before");

        //  THE CONTROL IS THE SAME DISK POINTED THE OTHER WAY, and it has to be.
        //  A placement alone can land the new file in front of BASIC.SYSTEM,
        //  because ProDOS reuses directory slots in place and this volume has a
        //  hole early in its directory -- measured: the first draft used "placed
        //  but not nominated" as the control and the kernel launched it anyway,
        //  which would have made the whole case pass without the reorder doing
        //  anything. Nominating the OTHER program is what leaves two images
        //  differing by this feature and nothing else.
        Assert::IsFalse (DidThePlacedProgramRun (theOldWay, controlRows),
            L"with the old program nominated, the kernel must launch that one");

        Assert::IsTrue (DidThePlacedProgramRun (configured, rows),
            L"and with ours nominated it must launch ours, with nothing typed at it");

        Assert::IsTrue (GuestSession::AnyRowIs (rows, kBanner),
            L"printing what it prints, on the guest's own screen");

        Assert::IsFalse (GuestSession::AnyRowIs (controlRows, kBanner),
            L"which the disk pointed the other way did not");
    }
};
