#include "Pch.h"
#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "HeadlessHost.h"
#include "Devices/Disk/DirectBootBuilder.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/Dos33Skeleton.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleDirectBootTests
//
//  What a real 6502 does with a disk that has no operating system on it.
//
//  Nothing else can answer this. The loader is a hundred and twenty bytes of
//  6502 in one sector, and every claim about it -- that the boot ROM hands
//  control to it, that it finds the payload's sectors in the order it asks
//  for them, that it steps the head, that it jumps where it was told -- is a
//  claim about instructions executing. Our own reader can only restate the
//  bytes we wrote.
//
//  THE CYCLE COUNT IS EMULATED, NOT ELAPSED. The comparison this file exists
//  to make is between two boots of the same program, and a wall clock would
//  measure the host instead: a busier machine would move the ratio without
//  anything about either disk changing. Counting the guest's own cycles makes
//  the answer a property of the disks and identical on every host.
//
//  The cheap questions come first, as they do beside this: the image is
//  decoded through the DRIVE and inspected before any processor starts, so a
//  wrong image is reported in milliseconds rather than by a 6502 executing
//  whatever it managed to load.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleDirectBootTests)
{
public:

    static constexpr const char *  kImagePath = "C:\\disks\\boot.dsk";
    static constexpr const char *  kHostFile  = "C:\\build\\prog.bin";

    static constexpr const char *  kBinaryName   = "PROG";
    static constexpr const char *  kGreetingName = "GREET";

    //  What the payload prints. Nothing on a DOS 3.3 master prints this, so
    //  seeing it means our program ran rather than somebody else's.
    static constexpr const char *  kBanner = "CASSO DIRECT";

    //  Above the loader, above the boot ROM's workspace, clear of DOS 3.3 --
    //  which matters for the comparison, since the same binary is placed on a
    //  DOS disk and BRUN there.
    static constexpr Word  kLoadAddress = 0x6000;

    //  The payload calls NOTHING. It sets the video soft switches itself,
    //  clears the text page itself and stores its banner into screen memory
    //  itself.
    //
    //  THAT IS NOT PURITY, IT IS THE ONLY THING THAT WORKS HERE. A direct
    //  boot enters at the disk controller's ROM and the monitor's cold start
    //  never runs, so zero page holds whatever powering on left. The monitor's
    //  own character output masks every character it prints with INVFLG at
    //  $32 -- measured, and against an uninitialized $32 the banner arrives
    //  with bits missing: CASSO DIRECT came out as B@RRN`DHRDBT. A program
    //  that owns the whole machine has nothing to inherit and has to say so.
    static constexpr Word  k80StoreOff   = 0xC000;
    static constexpr Word  k80ColumnOff  = 0xC00C;
    static constexpr Word  kAltCharsOff  = 0xC00E;
    static constexpr Word  kTextModeOn   = 0xC051;
    static constexpr Word  kTextPageOne  = 0xC054;

    //  The text page, and the start of its second row -- the interleave puts
    //  row 1 at $0480 rather than $0428.
    static constexpr Word  kTextPageBase = 0x0400;
    static constexpr Word  kBannerRow    = 0x0480;
    static constexpr Byte  kBlankCell    = 0xA0;

    static constexpr Byte  kHighBit    = 0x80;

    //  Two bytes at an address nothing on either disk keeps anything in.
    //  Their presence afterwards says the payload RAN, where the screen only
    //  says something wrote to it.
    static constexpr Word  kSignatureAddress = 0x0300;
    static constexpr Byte  kSignatureFirst   = 0x5A;
    static constexpr Byte  kSignatureSecond  = 0xA5;

    //  Where the payload stops and where its text begins, relative to the
    //  program's own first byte. Asserted against the assembled length, so an
    //  edit cannot silently move either.
    static constexpr Word  kHangOffset = 0x39;
    static constexpr Word  kTextOffset = 0x3C;

    //  A three-byte trap ahead of the program, for the case that asks whether
    //  the entry address is honored: entering at the load address lands in an
    //  infinite loop that writes nothing.
    static constexpr Word  kTrapBytes = 3;

    //  Applesoft, for the greeting a booting DOS 3.3 actually runs.
    static constexpr Byte  kPrintToken   = 0xBA;
    static constexpr Byte  kChrToken     = 0xE7;
    static constexpr Byte  kLineNumber   = 10;
    static constexpr Word  kProgramStart = 0x0801;

    //  A ceiling, not a target: what a machine executing nonsense is allowed
    //  to spend before a case gives up and says so. A cold DOS 3.3 boot
    //  reaches a BRUN'd binary in under a third of it, which is the headroom
    //  worth having and no more -- a machine that has gone wrong emits an
    //  illegal-opcode trace line per bad instruction, so a generous cap is
    //  paid for in gigabytes of log by whoever next breaks this on purpose.
    static constexpr uint64_t  kMeasureCap = 20'000'000ULL;

    //  Where the disk controller's ROM hands control to whatever it read off
    //  track 0 -- the same address for every 5.25-inch disk ever made for
    //  this machine, ours and DOS 3.3's alike.
    static constexpr Word  kBootRomHandsOver = 0x0801;

    //  SC-007's bar, as a ratio rather than a number of cycles, because the
    //  cost of a DOS boot is not ours to fix.
    static constexpr uint64_t  kDirectMustBeThisMuchFaster = 4;


    //
    //  ------------------------------------------------------------------
    //  The payload.
    //  ------------------------------------------------------------------
    //

    static void Absolute (std::vector<Byte> & code, Byte opcode, Word operand)
    {
        code.push_back (opcode);
        code.push_back ((Byte) (operand & 0xFF));
        code.push_back ((Byte) (operand >> 8));
    }

    static void Immediate (std::vector<Byte> & code, Byte opcode, Byte operand)
    {
        code.push_back (opcode);
        code.push_back (operand);
    }

    //  6502 that puts a banner on the screen, leaves a signature in memory
    //  and then stops. It owns the machine and behaves like it: video soft
    //  switches, a cleared text page and a banner stored straight into screen
    //  memory, with no call into the monitor and no reliance on zero page.
    //
    //  Stopping matters twice over: whatever is on the screen afterwards is
    //  what this program put there, and the address it stops at is what the
    //  cycle measurement watches for.
    static std::vector<Byte> MakeProgram (Word programAddress)
    {
        std::vector<Byte>  code;
        Word               textAt = (Word) (programAddress + kTextOffset);
        Word               hangAt = (Word) (programAddress + kHangOffset);
        std::string        banner = kBanner;
        size_t             i      = 0;

        Absolute  (code, 0x8D, k80StoreOff);                       // STA 80STOREOFF
        Absolute  (code, 0x8D, k80ColumnOff);                      // STA 80COLOFF
        Absolute  (code, 0x8D, kAltCharsOff);                      // STA ALTCHARSETOFF
        Absolute  (code, 0xAD, kTextModeOn);                       // LDA TXTSET
        Absolute  (code, 0xAD, kTextPageOne);                      // LDA TXTPAGE1

        Immediate (code, 0xA9, kBlankCell);                        // LDA #$A0
        Immediate (code, 0xA2, 0x00);                              // LDX #$00
        Absolute  (code, 0x9D, (Word) (kTextPageBase + 0x0000));   // clear: STA $0400,X
        Absolute  (code, 0x9D, (Word) (kTextPageBase + 0x0100));   //        STA $0500,X
        Absolute  (code, 0x9D, (Word) (kTextPageBase + 0x0200));   //        STA $0600,X
        Absolute  (code, 0x9D, (Word) (kTextPageBase + 0x0300));   //        STA $0700,X
        code.push_back (0xE8);                                     //        INX
        Immediate (code, 0xD0, 0xF1);                              //        BNE clear

        Immediate (code, 0xA2, 0x00);                              // LDX #$00
        Absolute  (code, 0xBD, textAt);                            // show: LDA text,X
        Immediate (code, 0xF0, 0x06);                              //       BEQ done
        Absolute  (code, 0x9D, kBannerRow);                        //       STA row1,X
        code.push_back (0xE8);                                     //       INX
        Immediate (code, 0xD0, 0xF5);                              //       BNE show

        Immediate (code, 0xA9, kSignatureFirst);                   // done: LDA #
        Absolute  (code, 0x8D, kSignatureAddress);                 //       STA sig
        Immediate (code, 0xA9, kSignatureSecond);                  //       LDA #
        Absolute  (code, 0x8D, (Word) (kSignatureAddress + 1));    //       STA sig+1

        Assert::AreEqual (size_t (kHangOffset), code.size(),
            L"the hang has to be where the jump targets");

        Absolute  (code, 0x4C, hangAt);                            // JMP self

        Assert::AreEqual (size_t (kTextOffset), code.size(),
            L"and the text has to start where the code places it");

        for (i = 0; i < banner.size(); i++)
        {
            code.push_back ((Byte) (banner[i] | kHighBit));
        }

        code.push_back (0);

        return code;
    }

    static Word HangAddressFor (Word programAddress)
    {
        return (Word) (programAddress + kHangOffset);
    }

    //  The program with a trap in front of it, so entering at the load
    //  address and entering three bytes later have visibly different
    //  outcomes.
    static std::vector<Byte> MakeTrappedProgram (Word loadAddress)
    {
        std::vector<Byte>  payload;
        std::vector<Byte>  program = MakeProgram ((Word) (loadAddress + kTrapBytes));

        Absolute (payload, 0x4C, loadAddress);                      // JMP loadAddress

        Assert::AreEqual (size_t (kTrapBytes), payload.size(),
            L"the trap is exactly the three bytes the entry offset assumes");

        payload.insert (payload.end(), program.begin(), program.end());

        return payload;
    }

    //  The program followed by enough filler to run off the end of a track,
    //  every page stamped with its own number so a page that arrived in the
    //  wrong place is visible wherever it is looked at.
    static std::vector<Byte> MakeProgramPaddedToPages (Word programAddress, size_t pages)
    {
        constexpr size_t  kPageBytes = 256;

        std::vector<Byte>  payload = MakeProgram (programAddress);
        size_t             at      = 0;

        Assert::IsTrue (payload.size() < kPageBytes,
            L"the program itself has to fit in the first page for the stamping below to "
            L"mean anything");

        for (at = payload.size(); at < pages * kPageBytes; at++)
        {
            payload.push_back ((Byte) (at / kPageBytes));
        }

        return payload;
    }

    //  Tokenized by hand: 10 PRINT: PRINT CHR$(4);"BRUN PROG". There is no
    //  tokenizer in this build, and one line is small enough to read. The
    //  bare PRINT ahead of the command guarantees the cursor is at the start
    //  of a line, which is where DOS looks for its command character.
    static void AppendText (std::vector<Byte> & inOutProgram, const std::string & text)
    {
        size_t  i = 0;

        for (i = 0; i < text.size(); i++)
        {
            inOutProgram.push_back ((Byte) text[i]);
        }
    }

    static std::vector<Byte> MakeBrunGreeting()
    {
        std::vector<Byte>  program;
        Word               nextAt = 0;

        program.push_back (0);   program.push_back (0);         // link, filled in below
        program.push_back (kLineNumber);   program.push_back (0);

        program.push_back (kPrintToken);
        AppendText (program, ":");
        program.push_back (kPrintToken);
        program.push_back (kChrToken);
        AppendText (program, std::string ("(4);\"BRUN ") + kBinaryName + "\"");

        program.push_back (0);                                  // end of line

        //  Applesoft chains its lines by ADDRESS, so the link is where this
        //  line ends once loaded at the start of program space.
        nextAt = (Word) (kProgramStart + program.size());

        program[0] = (Byte) (nextAt & 0xFF);
        program[1] = (Byte) (nextAt >> 8);

        program.push_back (0);   program.push_back (0);         // end of program

        return program;
    }


    //
    //  ------------------------------------------------------------------
    //  Building the two disks.
    //  ------------------------------------------------------------------
    //

    //  THE HAZARD BOUND, asked before any processor starts.
    //
    //  An image whose payload is not where the loader will look for it makes
    //  a 6502 execute whatever it managed to read, and this build emits one
    //  trace line per illegal opcode: a single mis-placed payload measured
    //  here wrote over three gigabytes of log before anything noticed. The
    //  container is decoded through the DRIVE and the first sector the loader
    //  will ask for is compared against the payload's first page, which costs
    //  milliseconds and turns that into an assertion failure.
    static void AssertThePayloadIsWhereTheLoaderWillLookForIt (
        const std::vector<Byte>  & image,
        const std::vector<Byte>  & payload)
    {
        DiskImage           decoded;
        SectorDecodeReport  report;
        std::vector<Byte>   sectors;
        size_t              at   = 0;
        size_t              span = 0;
        size_t              i    = 0;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (image, decoded));
        AssertSucceeded (NibblizationLayer::Denibblize (decoded, DiskFormat::Dsk,
                                                        sectors, report));

        Assert::AreEqual (1, (int) sectors[0],
            L"the boot ROM must find the loader in track 0's first sector, and be told to "
            L"stop after it rather than reading payload over the top of it");

        at   = Dos33Skeleton::GetSectorOffset (
                   DirectBootBuilder::kFirstPayloadTrack,
                   NibblizationLayer::GetDosFileIndexForPhysicalSector (0));

        span = (std::min) (payload.size(), (size_t) NibblizationLayer::kSectorByteSize);

        for (i = 0; i < span; i++)
        {
            Assert::AreEqual ((int) payload[i], (int) sectors[at + i],
                L"and the first sector the loader reads must hold the payload's first "
                L"page, or the guest is about to execute something else entirely");
        }
    }

    static std::vector<Byte> BuildDirectBootImage (const std::vector<Byte>  & payload,
                                                   Word                       loadAddress,
                                                   Word                       entryAddress)
    {
        std::vector<Byte>  built;
        std::string        refusal;
        DirectBootSpec     spec;

        spec.loadAddress  = loadAddress;
        spec.entryAddress = entryAddress;

        AssertSucceeded (DirectBootBuilder::Build (payload, spec, built, refusal),
            L"the direct-boot image must build before there is anything to boot");

        Assert::AreEqual (std::string(), refusal, L"with nothing to complain about");

        //  The cheap questions, before a processor is involved: this disk is
        //  supposed to carry no operating system, and a builder that quietly
        //  laid one down would still boot.
        Assert::IsTrue (VolumeKind::Unknown == VolumeImage::DetectFilesystem (built),
            L"and must carry no filesystem at all");

        AssertThePayloadIsWhereTheLoaderWillLookForIt (built, payload);

        return built;
    }

    static CommandLineOptions MakeOptions (CommandLineOptions::DiskOptions::Command command)
    {
        CommandLineOptions  options;

        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = command;
        options.disk.imagePath = kImagePath;

        return options;
    }

    static std::vector<Byte> PlaceFile (const std::vector<Byte>  & image,
                                        const std::vector<Byte>  & bytes,
                                        const char               * name,
                                        const char               * typeName,
                                        Word                       loadAddress,
                                        bool                       hasLoadAddress)
    {
        FakeDiskFileIo      io;
        CommandLineOptions  put = MakeOptions (CommandLineOptions::DiskOptions::Command::Put);

        io.files[kImagePath]  = image;
        io.stamps[kImagePath] = FileStamp { image.size(), 100 };
        io.files[kHostFile]   = bytes;
        io.stamps[kHostFile]  = FileStamp { bytes.size(), 100 };

        put.disk.hostFile       = kHostFile;
        put.disk.path           = name;
        put.disk.typeName       = typeName;
        put.disk.loadAddress    = loadAddress;
        put.disk.hasLoadAddress = hasLoadAddress;

        {
            DiskCommandRunner  runner (io);

            Assert::AreEqual (DiskCommandResult::kClean, runner.Run (put).exitStatus,
                L"the placement must succeed before there is anything to boot into");
        }

        Assert::IsTrue (io.HasNoTemporaryFiles(), L"leaving no temporary beside the image");

        return io.files[kImagePath];
    }

    static std::vector<Byte> SetStartupTo (const std::vector<Byte> & image, const char * name)
    {
        FakeDiskFileIo      io;
        CommandLineOptions  boot = MakeOptions (CommandLineOptions::DiskOptions::Command::Boot);

        io.files[kImagePath]  = image;
        io.stamps[kImagePath] = FileStamp { image.size(), 100 };

        boot.disk.path = name;

        {
            DiskCommandRunner  runner (io);

            Assert::AreEqual (DiskCommandResult::kClean, runner.Run (boot).exitStatus,
                L"setting the startup program must succeed");
        }

        return io.files[kImagePath];
    }

    //  The same program, delivered the way it would be without this feature:
    //  a binary on a bootable DOS 3.3 disk, with a greeting that BRUNs it.
    //  That is the whole of the alternative, and it is what SC-007 compares
    //  against.
    static std::vector<Byte> BuildDos33Equivalent (const std::vector<Byte> & program)
    {
        std::vector<Byte>  image = GuestSession::RequireDos33Master();

        image = PlaceFile   (image, program, kBinaryName, "B", kLoadAddress, true);
        image = PlaceFile   (image, MakeBrunGreeting(), kGreetingName, "A", 0, false);
        image = SetStartupTo (image, kGreetingName);

        return image;
    }


    //
    //  ------------------------------------------------------------------
    //  Driving the machine, and counting what it spends.
    //  ------------------------------------------------------------------
    //

    //  Runs the guest instruction by instruction until the processor is
    //  sitting on `hangAddress`, and hands back the cycles that took.
    //
    //  THE WITNESS IS THE PROGRAM COUNTER, not the screen and not a byte in
    //  memory. The payload's last instruction jumps to itself, so the
    //  processor being there means the guest reached the developer's code and
    //  nothing else can put it there. A memory sentinel polled during a boot
    //  would be satisfied the moment the boot ROM's own decode buffer
    //  happened to hold those two bytes.
    static uint64_t CyclesToReach (EmulatorCore & core, Word hangAddress, uint64_t cap)
    {
        uint64_t  start = core.cpu->GetTotalCycles();
        uint64_t  spent = 0;
        Byte      step  = 0;

        while (spent < cap && core.cpu->GetPC() != hangAddress)
        {
            core.cpu->StepOne();

            step = core.cpu->GetLastInstructionCycles();
            core.cpu->AddCycles (step);
            core.diskController->Tick (step);

            spent = core.cpu->GetTotalCycles() - start;
        }

        return spent;
    }

    struct BootOutcome
    {
        //  Cycles from power-on to the payload, and the part of that spent
        //  before the disk's own first byte executed.
        uint64_t                  cycles     = 0;
        uint64_t                  handoff    = 0;
        bool                      reached    = false;
        bool                      signature  = false;
        std::vector<std::string>  rows;
        std::vector<Byte>         loaded;
    };

    static BootOutcome Boot (const std::vector<Byte>  & image,
                             Word                       hangAddress,
                             Word                       loadAddress,
                             size_t                     loadedBytes)
    {
        HeadlessHost       host;
        EmulatorCore       core;
        BootOutcome        outcome;
        std::vector<Byte>  signature;
        uint64_t           afterHandoff = 0;

        GuestSession::Mount (host, core, image);

        //  Every 5.25-inch disk in this machine is entered the same way: the
        //  controller ROM recalibrates the head, reads track 0's first sector
        //  into $0800 and jumps to $0801. Measuring that separately is what
        //  lets a comparison be about the two DISKS rather than about the
        //  ROM they share.
        outcome.handoff = CyclesToReach (core, kBootRomHandsOver, kMeasureCap);

        afterHandoff    = CyclesToReach (core, hangAddress, kMeasureCap);
        outcome.cycles  = outcome.handoff + afterHandoff;
        outcome.reached = afterHandoff < kMeasureCap;

        GuestSession::CollectRows (core, outcome.rows);

        signature = GuestSession::GuestBytesAt (core, kSignatureAddress, 2);
        outcome.signature = signature[0] == kSignatureFirst && signature[1] == kSignatureSecond;

        if (loadedBytes > 0)
        {
            outcome.loaded = GuestSession::GuestBytesAt (core, loadAddress, loadedBytes);
        }

        return outcome;
    }


    //
    //  ------------------------------------------------------------------
    //  The gate.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (ADirectBootImage_RunsThePayloadWithNoOperatingSystemAnywhereOnTheDisk)
    {
        std::vector<Byte>  payload = MakeProgram (kLoadAddress);
        std::vector<Byte>  image   = BuildDirectBootImage (payload, kLoadAddress, kLoadAddress);
        BootOutcome        outcome = Boot (image, HangAddressFor (kLoadAddress),
                                           kLoadAddress, payload.size());



        Assert::IsTrue (outcome.reached,
            L"the guest must arrive at the payload's own last instruction: there is no "
            L"operating system on this disk, so the boot ROM and the loader are the only "
            L"things that could have got it there");

        Assert::IsTrue (outcome.signature,
            L"and the payload's own two bytes must be in memory, which proves it EXECUTED "
            L"rather than merely got loaded");

        Assert::IsTrue (GuestSession::AnyRowIs (outcome.rows, kBanner),
            L"printing what it prints, on the guest's own screen");

        Assert::IsTrue (outcome.loaded == payload,
            L"and every byte of it must be at its load address");
    }

    TEST_METHOD (APayloadLongerThanOneTrack_ArrivesWholeAndInOrder)
    {
        //  Twenty pages, so the loader has to step the head and read a second
        //  track. A payload that fits on one track cannot tell a loader that
        //  seeks from one that does not, and a wrong sector order inside a
        //  track produces pages in the wrong places rather than a failure.
        constexpr size_t  kPages = 20;

        std::vector<Byte>  payload = MakeProgramPaddedToPages (kLoadAddress, kPages);
        std::vector<Byte>  image   = BuildDirectBootImage (payload, kLoadAddress, kLoadAddress);
        BootOutcome        outcome = Boot (image, HangAddressFor (kLoadAddress),
                                           kLoadAddress, payload.size());



        Assert::IsTrue (outcome.reached, L"the guest must reach the payload");

        Assert::IsTrue (outcome.signature, L"and run it");

        Assert::AreEqual (kPages * 256, payload.size(),
            L"the payload must actually span more than one track, or nothing above needed "
            L"a seek");

        Assert::IsTrue (outcome.loaded == payload,
            L"and all twenty pages must be in memory in the order they were given, which "
            L"is what a wrong sector skew or a missed seek would break");
    }

    TEST_METHOD (AnEntryAwayFromTheLoadAddress_IsWhereTheGuestStarts)
    {
        std::vector<Byte>  payload    = MakeTrappedProgram (kLoadAddress);
        Word               entry      = (Word) (kLoadAddress + kTrapBytes);
        std::vector<Byte>  configured = BuildDirectBootImage (payload, kLoadAddress, entry);
        std::vector<Byte>  control    = BuildDirectBootImage (payload, kLoadAddress, kLoadAddress);
        BootOutcome        ran        = Boot (configured, HangAddressFor (entry), 0, 0);
        BootOutcome        trapped    = Boot (control, kLoadAddress, 0, 0);



        Assert::IsTrue (ran.reached,
            L"entering three bytes past the load address must reach the program");

        Assert::IsTrue (ran.signature, L"and run it");

        //  THE CONTROL IS THE SAME PAYLOAD ENTERED AT ITS LOAD ADDRESS, and
        //  it has to be. Without it, the case above is satisfied by a loader
        //  that ignores the entry address entirely and jumps to the load
        //  address, since the program is in memory either way.
        Assert::IsTrue (trapped.reached,
            L"entering at the load address must reach the trap that sits there");

        Assert::IsFalse (trapped.signature,
            L"and must never reach the program, which is what makes the case above about "
            L"the entry address rather than about the payload being present");

        Assert::IsFalse (GuestSession::AnyRowIs (trapped.rows, kBanner),
            L"nor print anything the program prints");
    }

    TEST_METHOD (APayloadTooLargeForTheBootPath_IsRefusedBeforeAnyDiskExists)
    {
        std::vector<Byte>  payload (DirectBootBuilder::GetCapacity (kLoadAddress) + 1, 0xEA);
        std::vector<Byte>  built;
        std::string        refusal;
        DirectBootSpec     spec;



        spec.loadAddress  = kLoadAddress;
        spec.entryAddress = kLoadAddress;

        AssertFailed (DirectBootBuilder::Build (payload, spec, built, refusal),
            L"a payload the loader could not pull must be refused rather than producing a "
            L"disk that boots into whatever it managed to read");

        Assert::AreEqual (std::string ("the payload is 24577 bytes and a direct-boot image "
                                       "loading at $6000 can carry 24576 (96 sectors)"),
            refusal,
            L"recording what the boot path can carry from this load address");

        Assert::IsTrue (built.empty(), L"and producing no image");
    }

    //
    //  ------------------------------------------------------------------
    //  What the two disks cost, measured rather than asserted.
    //
    //  READ THIS BEFORE THE ASSERTIONS. The bar is a quarter, and it is
    //  applied to what the two DISKS spend, which is the whole boot minus the
    //  part the controller ROM spends before either disk's first byte
    //  executes. That part is measured here rather than assumed, and it is
    //  measured on BOTH images and required to be identical, because the
    //  subtraction is only legitimate if it is the same constant on both
    //  sides.
    //
    //  It has to be excluded, because it is bigger than the bar. The ROM
    //  recalibrates the head with eighty half-steps, each waiting through the
    //  monitor's own delay, and then reads a sector -- about 1.6 million
    //  cycles, a second and a half of a real machine's life, and more than a
    //  quarter of everything a DOS 3.3 boot spends. Applied to the whole
    //  boot, a bar of a quarter is therefore not merely missed but
    //  unreachable: no disk can beat it, including one holding nothing at
    //  all. The case below asserts that arithmetic rather than describing it,
    //  so the reinterpretation is a measurement in the suite instead of a
    //  claim in a document.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (ADirectBoot_ReachesTheProgramInUnderAQuarterOfWhatTheDos33RouteSpends)
    {
        //  The same binary, twice: once as the only thing on a disk, and once
        //  the way it would be delivered without this feature -- placed on a
        //  bootable DOS 3.3 master with a greeting that BRUNs it.
        std::vector<Byte>  program    = MakeProgram (kLoadAddress);
        std::vector<Byte>  direct     = BuildDirectBootImage (program, kLoadAddress, kLoadAddress);
        std::vector<Byte>  viaDos     = BuildDos33Equivalent (program);
        Word               hangAt     = HangAddressFor (kLoadAddress);
        BootOutcome        fast       = Boot (direct, hangAt, 0, 0);
        BootOutcome        slow       = Boot (viaDos, hangAt, 0, 0);
        char               note[512]  = {};
        std::string        text;
        std::wstring       verdict;
        uint64_t           fromDirect = 0;
        uint64_t           fromDos    = 0;
        size_t             i          = 0;



        Assert::IsTrue (slow.reached,
            L"the DOS 3.3 route must reach the program, or there is nothing to compare "
            L"against and the ratio below would be a comparison with a ceiling");

        Assert::IsTrue (slow.signature,
            L"and must have run it -- the same two bytes, from the same binary");

        Assert::IsTrue (fast.reached, L"the direct-boot disk must reach the program too");

        Assert::IsTrue (fast.signature, L"and run it");

        Assert::AreEqual (fast.handoff, slow.handoff,
            L"the two boots must spend exactly the same getting to the point where the "
            L"disk's own code starts -- same ROM, same recalibrate, same first sector -- "
            L"which is what makes it a shared constant that can be taken off both sides "
            L"rather than a convenient subtraction");

        fromDirect = fast.cycles - fast.handoff;
        fromDos    = slow.cycles - slow.handoff;

        snprintf (note, sizeof (note),
                  "SC-007: the controller ROM spends %llu emulated cycles before either "
                  "disk runs. Past that, the direct-boot image reached the payload in %llu "
                  "and the same binary BRUN from DOS 3.3 took %llu (%.1f%%). Whole boots: "
                  "%llu against %llu (%.1f%%)",
                  (unsigned long long) fast.handoff,
                  (unsigned long long) fromDirect,
                  (unsigned long long) fromDos,
                  100.0 * (double) fromDirect / (double) fromDos,
                  (unsigned long long) fast.cycles,
                  (unsigned long long) slow.cycles,
                  100.0 * (double) fast.cycles / (double) slow.cycles);

        Logger::WriteMessage (note);

        //  The measurement itself goes into the failure text, so a run that
        //  misses the bar says by how much rather than only that it did.
        text = std::string ("a direct boot must reach the payload in under a quarter of "
                            "what the equivalent DOS 3.3 route spends -- ") + note;

        for (i = 0; i < text.size(); i++)
        {
            verdict.push_back ((wchar_t) text[i]);
        }

        Assert::IsTrue (fromDirect * kDirectMustBeThisMuchFaster < fromDos, verdict.c_str());

        //  And the arithmetic that makes the whole-boot form of the same bar
        //  unreachable. If this ever stops holding, the exclusion above is no
        //  longer necessary and the gate should be tightened onto the whole
        //  boot -- which is why it is asserted rather than written down.
        Assert::IsTrue (fast.handoff * kDirectMustBeThisMuchFaster > slow.cycles,
            L"the controller ROM alone costs more than a quarter of a DOS 3.3 boot, so no "
            L"disk of any kind can reach its payload in under a quarter of one");
    }
};
