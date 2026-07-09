#include "Pch.h"
#include "HeadlessHost.h"
#include "TextScreenScraper.h"
#include "FixtureProvider.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk2Controller.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cBootTests
//
//  Spec 016 / US2. These exercise the Apple //c wiring against the real ROM 4.
//
//  The //c now cold-boots its firmware end-to-end (ColdBootsToCheckDiskDrive):
//  with no disk it clears the screen, shows the "Apple //c" banner, and reaches
//  the correct "Check Disk Drive." no-disk state. The bring-up chain that got
//  here, in boot order:
//
//    1. Rockwell bit ops (RMB/SMB/BBR/BBS): Cpu65C02 models the Rockwell R65C02
//       (ROM 4 runs BBS0 at $D010).
//    2. //c $C100-$CFFF routing: the //c has no card slots, so the whole window
//       (incl. the $C800 expansion space) always reads internal firmware --
//       CxxxRomRouter::SetNoExternalSlots(true).
//    3. $C028 ROM-bank toggle: the Apple2eKeyboard front device owns $C000-$C063
//       and forwards sub-ranges to its siblings; it now forwards $C028 to the
//       soft-switch bank (which drives the ROM-bank flip-flop). Paired with the
//       CPU store no longer pre-reading its target (a dummy read + the write
//       would double-toggle the flip-flop back).
//    4. Slot-6 IWM: the //c's built-in drive is an Integrated Woz Machine, so
//       Disk2Controller gains an IWM mode/status register (SetIwmMode) that the
//       reset firmware writes then reads back to confirm the drive is present.
//
//  Booting actual software still needs a bootable disk image (the //c has no
//  cassette / BASIC-on-cold-boot; "Check Disk Drive." is its terminal no-disk
//  state). The ROM fixture is copyrighted + uncommitted; tests skip when it is
//  absent so CI never needs a machine ROM.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr size_t     kRomSize = 0x8000;      // 32K, two 16K banks
    static constexpr Word       kMonitorReset = 0xFA62; // ROM 4 RESET vector target

    bool Apple2cRomAvailable ()
    {
        FixtureProvider        fp;
        std::vector<uint8_t>   bytes;
        return SUCCEEDED (fp.OpenFixture ("Apple2c.rom", bytes)) && bytes.size () == kRomSize;
    }
}


TEST_CLASS (Apple2cBootTests)
{
public:

    // The //c cold-boots its firmware end-to-end. With no disk inserted it
    // clears the screen, prints the "Apple //c" banner, probes the built-in IWM
    // drive, and -- finding no bootable disk -- displays "Check Disk Drive.",
    // the correct no-disk terminal state on real hardware. This exercises the
    // whole bring-up chain: the Rockwell 65C02, the $C028 ROM-bank switch, the
    // no-slots $Cxxx routing, and the slot-6 IWM mode/status register.
    TEST_METHOD (ColdBootsToCheckDiskDrive)
    {
        if (!Apple2cRomAvailable ())
        {
            Logger::WriteMessage ("SKIPPED: no Apple2c.rom fixture");
            return;
        }

        HeadlessHost host; EmulatorCore core;
        Assert::IsTrue (SUCCEEDED (host.BuildApple2c (core)), L"BuildApple2c");
        core.PowerCycle ();
        core.RunCycles (15'000'000);

        std::string screen;
        for (const auto & row : TextScreenScraper::Scrape (core))
        {
            screen += row;
            screen += '\n';
        }

        Assert::IsTrue (screen.find ("Apple //c") != std::string::npos,
            L"boot screen must show the Apple //c banner");
        Assert::IsTrue (screen.find ("Check Disk Drive") != std::string::npos,
            L"no-disk boot must reach the Check Disk Drive prompt");
    }

    // Regression for the $C028 ROM-bank flip-flop end-to-end through the CPU
    // and the full bus dispatch. Guards two fixes that must hold together:
    //   (a) Apple2eKeyboard (the $C000-$C063 front device) forwards $C028 to
    //       the soft-switch bank that drives the flip-flop, and
    //   (b) a CPU store no longer pre-reads its target -- a dummy read plus the
    //       store's write would toggle the flop twice (net no switch).
    // If either regresses, the bank stays on 0 and this fails.
    TEST_METHOD (StaC028TogglesRomBankExactlyOnce)
    {
        if (!Apple2cRomAvailable ())
        {
            Logger::WriteMessage ("SKIPPED: no Apple2c.rom fixture");
            return;
        }

        HeadlessHost host; EmulatorCore core;
        Assert::IsTrue (SUCCEEDED (host.BuildApple2c (core)), L"BuildApple2c");
        core.PowerCycle ();

        Assert::AreEqual (0, core.romBank->CurrentBank (), L"reset selects bank 0");

        auto execAt = [&] (Word at, std::initializer_list<Byte> bytes)
        {
            Word a = at;
            for (Byte b : bytes) core.cpu->WriteByte (a++, b);
            core.cpu->SetPC (at);
            core.cpu->StepOne ();
        };

        execAt (0x0300, { 0x8D, 0x28, 0xC0 });   // STA $C028
        Assert::AreEqual (1, core.romBank->CurrentBank (), L"STA $C028 toggles once");

        execAt (0x0300, { 0x8D, 0x28, 0xC0 });   // STA $C028 again
        Assert::AreEqual (0, core.romBank->CurrentBank (), L"second STA toggles back");

        execAt (0x0300, { 0xAD, 0x28, 0xC0 });   // LDA $C028 (any access flips it)
        Assert::AreEqual (1, core.romBank->CurrentBank (), L"LDA $C028 toggles once");
    }

    // US5 / T032 + T035: the //c boots from its built-in slot-6 drive through
    // the IWM. This is the real disk-read path (Q6L/Q7L RDDATA on the same
    // Disk2Controller the //e uses -- IWM mode only added the MODE/STATUS
    // registers, not a fork), so a mounted bootable disk must feed nibbles to
    // the boot ROM. We stamp a 100%-in-house 18-byte boot sector into track 0
    // sector 0 that writes the marker "IWM" to $0300 and self-loops; on a real
    // cold boot the //c reads T0S0 into $0800, JMPs $0801, and runs it. If the
    // IWM read path is broken the //c instead reaches "Check Disk Drive." and
    // the marker never lands.
    TEST_METHOD (BootsFromInternalDriveViaIwm)
    {
        if (!Apple2cRomAvailable ())
        {
            Logger::WriteMessage ("SKIPPED: no Apple2c.rom fixture");
            return;
        }

        // 18-byte boot sector assembled by hand (only base-6502 opcodes so it
        // runs on the //e/][/][+ too). Loaded by the boot ROM at $0801:
        //   $0801  A9 49     LDA #$49  ; 'I'
        //   $0803  8D 00 03  STA $0300
        //   $0806  A9 57     LDA #$57  ; 'W'
        //   $0808  8D 01 03  STA $0301
        //   $080B  A9 4D     LDA #$4D  ; 'M'
        //   $080D  8D 02 03  STA $0302
        //   $0810  4C 10 08  JMP $0810 ; halt (self-loop)
        static const Byte kBootSector[] = {
            0xA9, 0x49, 0x8D, 0x00, 0x03,
            0xA9, 0x57, 0x8D, 0x01, 0x03,
            0xA9, 0x4D, 0x8D, 0x02, 0x03,
            0x4C, 0x10, 0x08
        };

        // Raw DOS-order .dsk; boot ROM reads T0S0 (256 bytes) into $0800 and
        // JMPs $0801, so the code sits at file offset 1 (byte $0800 is unused).
        std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);
        for (size_t i = 0; i < sizeof (kBootSector); i++)
        {
            raw[1 + i] = kBootSector[i];
        }

        HeadlessHost host; EmulatorCore core;
        Assert::IsTrue (SUCCEEDED (host.BuildApple2c (core)), L"BuildApple2c");

        // PowerCycle first (it re-seeds DRAM + rebinds the drive to its empty
        // internal disk), THEN mount -- matching the production ordering.
        core.PowerCycle ();

        HRESULT hrMount = core.diskStore->MountFromBytes (6, 0, "iwm-boot.dsk",
                                                          DiskFormat::Dsk, raw);
        Assert::IsTrue (SUCCEEDED (hrMount), L"MountFromBytes must succeed");

        DiskImage * img = core.diskStore->GetImage (6, 0);
        Assert::IsNotNull (img, L"mounted image must be retrievable");
        core.diskController->SetExternalDisk (0, img);   // drive 1 = internal

        // Cold-boot: memory test (~14M cycles) then the IWM boot read. 20M is
        // ample -- ColdBootsToCheckDiskDrive reaches the post-read state in 15M.
        core.RunCycles (20'000'000);

        Assert::AreEqual<Byte> (0x49, core.cpu->ReadByte (0x0300),
            L"boot sector must run and write 'I' to $0300 (IWM read failed?)");
        Assert::AreEqual<Byte> (0x57, core.cpu->ReadByte (0x0301),
            L"boot sector must write 'W' to $0301");
        Assert::AreEqual<Byte> (0x4D, core.cpu->ReadByte (0x0302),
            L"boot sector must write 'M' to $0302");
        Assert::AreEqual<Word> (0x0810, core.cpu->GetPC (),
            L"CPU must be spinning in the booted sector's halt loop, not the "
            L"ROM's Check-Disk-Drive self-loop");
    }

    // US5 / T032 + T034: the //c's external (second) drive is drive 2 of the
    // same slot-6 IWM. A disk mounted there must be reachable by selecting
    // drive 2 ($C0EB) and reading the data register ($C0EC). Driven the same
    // way DiskReadbackTests exercises drive 1: detach the CPU cycle source,
    // select-drive-then-motor-on into read mode, drain the spin-up window, then
    // Tick the nibble engine and sample $C0EC -- a valid disk nibble (MSB set)
    // proves the external drive engine streams data. If drive 2 were unwired we
    // would only ever read the empty-drive floating bus.
    TEST_METHOD (ExternalDriveIsReadableViaDriveSelect)
    {
        if (!Apple2cRomAvailable ())
        {
            Logger::WriteMessage ("SKIPPED: no Apple2c.rom fixture");
            return;
        }

        // Any non-blank disk works -- the nibblizer frames every track with
        // sync + address marks, so even a mostly-zero image streams valid bytes.
        std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);
        raw[1] = 0xEA;

        HeadlessHost host; EmulatorCore core;
        Assert::IsTrue (SUCCEEDED (host.BuildApple2c (core)), L"BuildApple2c");
        core.PowerCycle ();

        HRESULT hrMount = core.diskStore->MountFromBytes (6, 1, "ext.dsk",
                                                          DiskFormat::Dsk, raw);
        Assert::IsTrue (SUCCEEDED (hrMount), L"external MountFromBytes must succeed");
        DiskImage * img = core.diskStore->GetImage (6, 1);
        Assert::IsNotNull (img, L"external image must be retrievable");
        core.diskController->SetExternalDisk (1, img);   // drive 2 = external

        // Pump the engine via Tick(N) rather than CPU cycles (the read path's
        // catch-up needs a live, advancing CPU counter we are not providing).
        core.diskController->SetCpuCycleSource (nullptr);

        core.bus->ReadByte (0xC0EB);   // select drive 2 (external)
        core.bus->ReadByte (0xC0E9);   // motor on -> drive 2's engine spins
        core.bus->ReadByte (0xC0ED);   // Q6 high first
        core.bus->ReadByte (0xC0EC);   // Q6 low
        core.bus->ReadByte (0xC0EE);   // Q7 low -> read mode
        core.diskController->Tick (Disk2Controller::kMotorSpinupCycles);

        bool sawValidNibble = false;
        for (int i = 0; i < 4000 && !sawValidNibble; i++)
        {
            if (core.bus->ReadByte (0xC0EC) & 0x80)
            {
                sawValidNibble = true;
            }
            core.diskController->Tick (8);   // advance ~one disk bit-time
        }

        Assert::IsTrue (sawValidNibble,
            L"drive 2 must stream a valid disk nibble (MSB set) once selected");
    }

    // Verifies the parts of the //c that ARE working end-to-end: the 65C02 +
    // 32K two-bank firmware wire up, and a cold reset lands on the monitor
    // entry with the ROM correctly mapped through the language card.
    TEST_METHOD (BuildsAndResetsToMonitorEntry)
    {
        if (!Apple2cRomAvailable ())
        {
            Logger::WriteMessage (
                "SKIP: UnitTest/Fixtures/Apple2c.rom absent "
                "(copyrighted //c ROM 4, provisioned on demand).");
            return;
        }

        HeadlessHost   host;
        EmulatorCore   core;

        Assert::IsTrue (SUCCEEDED (host.BuildApple2c (core)),
            L"BuildApple2c must succeed when the ROM is present");
        Assert::IsTrue (core.HasApple2e (),
            L"//c wiring (65C02 + MMU) must be complete");

        // Reset vector (read through the bus -> language card ROM, bank 0)
        // must point at the ROM 4 monitor entry.
        Word resetVec = static_cast<Word> (core.cpu->ReadByte (0xFFFC)) |
                        (static_cast<Word> (core.cpu->ReadByte (0xFFFD)) << 8);
        Assert::AreEqual<Word> (kMonitorReset, resetVec,
            L"RESET vector must select the bank-0 monitor entry $FA62");

        // $FA62 must decode as CLD ($D8): the classic monitor reset preamble,
        // i.e. the LC is serving bank-0 ROM at $D000-$FFFF.
        Assert::AreEqual<Byte> (0xD8, core.cpu->ReadByte (0xFA62),
            L"$FA62 must read CLD from the mapped monitor ROM");

        // The CPU powers on at the reset entry.
        core.PowerCycle ();
        Assert::AreEqual<Word> (kMonitorReset, core.cpu->GetPC (),
            L"Cold reset must enter the monitor at $FA62");
    }
};
