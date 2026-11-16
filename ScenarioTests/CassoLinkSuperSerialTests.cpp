#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/SteppedByteStream.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/LinkBootstrap.h"
#include "CassoLink/LinkFault.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CassoLinkConnector.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Tape/ITapeAudioDecoder.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSuperSerialScenarioTests
//
//  The link on a //e with a Super Serial Card in slot 5, over the in-process
//  bridge to the card's 6551: the link disk, the universal disk, the typed
//  bootstrap through the card's firmware, the screen holes after Escape, and
//  the cassette bootstrap with and without a flipped byte. The host runs on
//  the test's thread through a SteppedByteStream, and the bootstrap is typed
//  at the session's pace in the machine's cycles, so nothing waits on the
//  PC's clock.
//
//  These are scenario tests because all but the link disk run Apple's
//  firmware for the card, from
//  %LOCALAPPDATA%\Casso\Devices\SuperSerialCard\SSC.rom, which is not
//  committed; they fail without it, as the card's other guest-visible cases
//  do.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkSuperSerialScenarioTests)
{
public:

    static constexpr uint64_t  kClockHz       = 1'020'484;
    static constexpr uint64_t  kStepCycles    = 17'030;
    static constexpr uint64_t  kBootCycles    = 20'000'000;
    static constexpr uint64_t  kCharCycles    = (uint64_t) LinkBootstrap::kCharPauseUs * kClockHz / 1'000'000;
    static constexpr uint64_t  kLineCycles    = (uint64_t) LinkBootstrap::kLinePauseMs * kClockHz / 1'000;
    static constexpr uint64_t  kLoaderWait    = (uint64_t) LinkBootstrap::kLoaderReadyWaitMs * kClockHz / 1'000;
    static constexpr DWORD     kBootstrapBaud = 9600;
    static constexpr int       kSlot          = 5;
    static constexpr Byte      kTrack         = 17;
    static constexpr int       kTracks        = 35;

    //  Communications mode at 9,600 baud, and the second bank as it ships.
    static constexpr const char *  kCommunicationsSw1 = "off off off on on on on";
    static constexpr const char *  kShippedSw2        = "on on off on off off off";

    //  The card turned on in slot 5, with nothing on its connector: the
    //  test's bridge is attached once the machine is built. With firmware,
    //  Apple's, and the switches for IN# and Ctrl-A commands.
    static void  ConfigureCard (MachineConfig & config, bool firmware)
    {
        // The //e's default card in slot 5 is taken out for this one.
        std::erase_if (config.slots, [] (const SlotConfig & slot) { return slot.slot == kSlot && slot.device != kpszSuperSerialCardDevice; });

        for (SlotConfig & slot : config.slots)
        {
            if (slot.device == kpszSuperSerialCardDevice)
            {
                slot.slot    = kSlot;
                slot.enabled = true;
                slot.ports.clear();

                if (firmware)
                {
                    slot.firmwarePath   = RequireFirmwarePath().string();
                    slot.firmwareStatus = SlotFirmwareStatus::Present;
                    slot.switchesSw1    = kCommunicationsSw1;
                    slot.switchesSw2    = kShippedSw2;
                }
            }
        }
    }

    static std::filesystem::path  RequireFirmwarePath()
    {
        char *                 root = nullptr;
        size_t                 len  = 0;
        std::filesystem::path  path;
        std::error_code        ec;



        if (_dupenv_s (&root, &len, "LOCALAPPDATA") == 0 && root != nullptr)
        {
            path = std::filesystem::path (root) / "Casso" / "Devices" / "SuperSerialCard" / "SSC.rom";
            free (root);
        }

        Assert::AreEqual<uintmax_t> (2048, std::filesystem::file_size (path, ec),
            L"Apple's Super Serial Card firmware is REQUIRED by this case and was not found at "
            L"%LOCALAPPDATA%\\Casso\\Devices\\SuperSerialCard\\SSC.rom. To get it, turn the card on "
            L"under Settings > Machine and accept the download");

        return path;
    }

    //  A //e with the card, drive 2's DOS 3.3 fixture, the PC on the card,
    //  and the host on the test's thread.
    struct Rig
    {
        TestMachine                           machine;
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::shared_ptr<SteppedByteStream>    stream;
        InMemoryCassoLinkFolder               folder;
        std::unique_ptr<CassoLinkHost>        host;
        std::vector<Byte>                     disk;

        explicit Rig (bool firmware)
            : machine ("Apple2e", [firmware] (MachineConfig & config) { ConfigureCard (config, firmware); })
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;



            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            disk.assign (bytes.begin(), bytes.end());

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

            InProcessByteStream::CreatePair (machineEnd, pcEnd);
            AssertSucceeded (machine.AttachSerialStream (kSlot, machineEnd));

            stream = std::make_shared<SteppedByteStream> (machine, pcEnd);
            host   = std::make_unique<CassoLinkHost> (stream, folder, [] () { return std::array<Byte, 4> {}; });
        }

        void  MountDisk()
        {
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));
        }

        //  At the BASIC prompt, as the user finds it.
        void  StartAtThePrompt()
        {
            machine.PowerCycle();
            machine.RunCycles (2 * kClockHz);
            machine.GetCpu()->SetPC (0xE000);
            machine.RunCycles (2 * kClockHz);
        }

        void  Type (const char * text)
        {
            for (size_t i = 0; text[i] != '\0'; i++)
            {
                machine.GetRefs().iieKeyboard->PressKey ((Byte) text[i]);
                machine.RunCycles (100'000ULL);
            }
        }

        //  The loader typed through the card at the session's pace, its $A5,
        //  and the shim sent to it.
        void  Bootstrap()
        {
            std::vector<std::string>  lines;
            std::vector<Byte>         transfer;
            Byte                      got[64] = {};
            size_t                    read    = 0;
            uint64_t                  spent   = 0;
            bool                      ready   = false;



            AssertSucceeded (LinkBootstrap::BuildLines (lines));

            for (const std::string & line : lines)
            {
                for (char c : line)
                {
                    AssertSucceeded (pcEnd->Write (reinterpret_cast<const Byte *> (&c), 1));
                    machine.RunCycles (kCharCycles);
                }

                machine.RunCycles (kLineCycles);
            }

            while (!ready && spent <= kLoaderWait)
            {
                AssertSucceeded (pcEnd->Read (got, sizeof (got), 0, read));
                ready = std::find (got, got + read, LinkBootstrap::kLoaderReady) != got + read;

                machine.RunCycles (kStepCycles);
                spent += kStepCycles;
            }

            Assert::IsTrue (ready, L"the loader's $A5, within the session's wait");

            AssertSucceeded (LinkBootstrap::BuildShimTransfer (kBootstrapBaud, transfer));
            AssertSucceeded (pcEnd->Write (transfer.data(), transfer.size()));
        }

        std::vector<Byte>  ReadTrack (Byte track)
        {
            std::vector<Byte>  sectors (16);
            std::vector<Byte>  data;
            CassoLinkStatus    status;



            for (size_t i = 0; i < sectors.size(); i++)
            {
                sectors[i] = (Byte) i;
            }

            AssertSucceeded (host->ReadSectors (2, track, sectors, data, status));
            Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, std::format (L"track {}", track).c_str());
            return data;
        }

        //  A track's 16 physical sectors from drive 2's image, in order.
        std::vector<Byte>  ImageTrack (Byte track)
        {
            std::vector<Byte>  data;



            for (int p = 0; p < 16; p++)
            {
                size_t  offset = (size_t) (track * 16 + NibblizationLayer::GetDosFileIndexForPhysicalSector (p)) * 256;

                data.insert (data.end(), disk.begin() + (ptrdiff_t) offset, disk.begin() + (ptrdiff_t) offset + 256);
            }

            return data;
        }

        std::vector<std::string>  Screen()
        {
            return TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
        }
    };

    //  Every slot's screen holes in main memory, slot by slot: slot n's are
    //  $478 + n, $4F8 + n, and so on to $7F8 + n.
    static std::vector<Byte>  ReadScreenHoles (TestMachine & machine)
    {
        std::vector<Byte>  holes;



        for (Word slot = 0; slot < 8; slot++)
        {
            for (Word at = 0x0478 + slot; at < 0x0800; at += 0x80)
            {
                holes.push_back (machine.GetMemoryBus().ReadByte (at));
            }
        }

        return holes;
    }

    static std::string  JoinRows (const std::vector<std::string> & rows)
    {
        std::string  text;



        for (const std::string & row : rows)
        {
            text += row + " | ";
        }

        return text;
    }

    //  The bootstrap's audio fed to the cassette input, with the
    //  monitor's read command typed first when `typeCommand` is true.
    static void  PlayCassette (Rig & rig, bool typeCommand)
    {
        std::vector<Byte>     wav;
        std::string           command;
        LinkShimOptions       options;
        NullTapeAudioDecoder  mp3;
        TapeImage             image;
        std::string           error;
        double                seconds = 0.0;



        AssertSucceeded (LinkAudioBootstrap::BuildWav (options, wav, command));
        Assert::AreEqual (std::string ("3AA0.3E7AR 3AA0G\r"), command, L"the slot finder ahead of the shim");
        Assert::AreEqual (S_OK, TapeImageLoader::Load (wav, "bootstrap.wav", false, mp3, image, error), TextEncoding::NarrowToWide (error).c_str());

        seconds = (double) image.signal.lengthSamples / image.signal.sampleRate;

        if (typeCommand)
        {
            rig.Type (command.c_str());
        }

        rig.machine.GetTapeDeck().Insert (std::move (image));
        rig.machine.GetTapeDeck().Play (*rig.machine.GetCpu()->GetBusCyclePtr());
        rig.machine.RunCycles ((uint64_t) ((seconds + 1.0) * rig.machine.GetConfig().clockSpeed));
        rig.machine.GetTapeDeck().Update (*rig.machine.GetCpu()->GetBusCyclePtr());
    }

    //  Faults on for the test, and off with none left armed after it.
    struct FaultsEnabled
    {
        FaultsEnabled()  { LinkFaults::Enable(); }
        ~FaultsEnabled() { LinkFaults::Disable(); }
    };



    //  The link disk for the card in slot 5, with no firmware: the shim
    //  waits on the card, and the program reads and writes drive 2 over it.
    TEST_METHOD (TheLinkDisk_OverACardInSlot5_ReadsAndWritesTheDisk)
    {
        Rig                rig (false);
        std::vector<Byte>  boot;
        LinkShimOptions    options;
        const Byte         sector[] = { 0 };
        std::vector<Byte>  pattern (256, 0x5A);
        std::vector<Byte>  data;
        CassoLinkStatus    status;



        options.slot = kSlot;
        AssertSucceeded (LinkProgram::BuildBootDisk (options, boot));

        GuestSession::Mount (rig.machine, boot);
        rig.MountDisk();
        rig.machine.RunCycles (kBootCycles);

        Assert::IsTrue (rig.Screen()[2].starts_with ("WAITING FOR THE PC ON:"), TextEncoding::NarrowToWide (JoinRows (rig.Screen())).c_str());
        Assert::IsTrue (rig.Screen()[4].starts_with ("SSC, SLOT 5, 115200"), TextEncoding::NarrowToWide (rig.Screen()[4]).c_str());

        for (Byte track = 0; track < kTracks; track++)
        {
            Assert::IsTrue (rig.ReadTrack (track) == rig.ImageTrack (track), std::format (L"track {} over the card", track).c_str());
        }

        AssertSucceeded (rig.host->WriteSectors (2, 20, sector, pattern, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        AssertSucceeded (rig.host->ReadSectors (2, 20, sector, data, status));
        Assert::IsTrue (data == pattern, L"a sector written over the card reads back");

        rig.machine.RunCycles (kBootCycles / 40);
        Assert::IsTrue (rig.Screen()[1].find ("SSC, SLOT 5, 115200") != std::string::npos, TextEncoding::NarrowToWide (rig.Screen()[1]).c_str());
        Assert::AreEqual<Byte> (0x50, rig.machine.GetCpu()->ReadByte (0x0006), L"the program's 6551 is slot 5's");
    }



    //  The universal disk finds the card in slot 5 by its firmware's
    //  identification bytes and links over it.
    TEST_METHOD (TheUniversalDisk_FindsACardInSlot5_AndReadsADisk)
    {
        Rig                rig (true);
        std::vector<Byte>  boot;
        LinkShimOptions    options;



        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, boot));

        GuestSession::Mount (rig.machine, boot);
        rig.MountDisk();
        rig.machine.RunCycles (kBootCycles);

        Assert::IsTrue (rig.Screen()[2].starts_with ("WAITING FOR THE PC ON:"), TextEncoding::NarrowToWide (JoinRows (rig.Screen())).c_str());
        Assert::IsTrue (rig.Screen()[4].starts_with ("SSC, SLOT 5, 115200"), TextEncoding::NarrowToWide (rig.Screen()[4]).c_str());

        AssertSucceeded (rig.host->PromptReady());
        Assert::IsTrue (rig.ReadTrack (kTrack) == rig.ImageTrack (kTrack), L"drive 2's catalog over the card");
        Assert::AreEqual<Byte> (0x50, rig.machine.GetCpu()->ReadByte (0x0006), L"the program's 6551 is slot 5's");
    }



    //  The card's firmware in slot 5 has the //c's IN# and Ctrl-A 14B, except
    //  that it executes the command at the Return after it. The loader is
    //  typed in through the card, and the slot finder takes slot 5 from the
    //  input hook IN#5 set.
    TEST_METHOD (ABareIIe_IsBootstrappedThroughTheCardsFirmware_AndReadsADisk)
    {
        Rig  rig (true);



        rig.StartAtThePrompt();
        rig.Type ("IN#5\r");
        rig.Type ("\x01" "14B\r");
        rig.MountDisk();

        rig.Bootstrap();

        Assert::IsTrue (rig.ReadTrack (kTrack) == rig.ImageTrack (kTrack), L"drive 2's catalog over the bootstrapped link");
        Assert::IsTrue (rig.Screen()[0].starts_with ("CASSOLINK"), TextEncoding::NarrowToWide (JoinRows (rig.Screen())).c_str());
    }



    //  The screen holes, the 8 bytes after each 120 of the text page, have
    //  each card's settings for its slot: the Super Serial Card's in slot 5,
    //  as IN#5 and Ctrl-A 14B set them, and known values in slot 6's. After a
    //  bootstrap, a link in 80 columns and Escape, every slot's holes match
    //  their values before the bootstrap.
    TEST_METHOD (AfterEscape_EverySlotsScreenHoles_MatchTheirValuesBeforeTheBootstrap)
    {
        Rig                rig (true);
        std::vector<Byte>  before;
        std::vector<Byte>  after;



        rig.StartAtThePrompt();
        rig.Type ("IN#5\r");
        rig.Type ("\x01" "14B\r");

        //  Slot 6's holes, each a different value.
        for (Word at = 0x047E; at < 0x0800; at += 0x80)
        {
            rig.machine.GetMemoryBus().WriteByte (at, (Byte) (0x11 + (at >> 7)));
        }

        rig.MountDisk();

        before = ReadScreenHoles (rig.machine);
        Assert::IsTrue (std::any_of (before.begin() + 5 * 8, before.begin() + 6 * 8, [] (Byte b) { return b != 0xA0; }), L"IN#5 set the card's holes");

        rig.Bootstrap();
        AssertSucceeded (rig.host->SetColumns (true));
        rig.ReadTrack (kTrack);

        rig.machine.GetRefs().iieKeyboard->PressKey (0x1B);
        rig.machine.RunCycles (2 * kClockHz);
        Assert::IsFalse (rig.Screen()[0].starts_with ("CASSOLINK"), L"Escape ended CassoLink");

        after = ReadScreenHoles (rig.machine);

        for (size_t i = 0; i < before.size(); i++)
        {
            Assert::AreEqual<int> (before[i], after[i], std::format (L"slot {}'s screen hole at ${:03X}", i / 8, 0x478 + (i / 8) + (i % 8) * 0x80).c_str());
        }
    }



    //  The monitor reads the slot finder and the shim from the cassette input
    //  as audio, and the shim then talks to the card's 6551 directly.
    TEST_METHOD (ABareIIe_IsBootstrappedThroughTheCassetteInput_AndReadsADisk)
    {
        Rig  rig (true);



        rig.StartAtThePrompt();
        rig.MountDisk();
        rig.Type ("CALL -151\r");
        PlayCassette (rig, true);

        Assert::IsTrue (rig.Screen()[4].starts_with ("SSC, SLOT 5, 115200"), TextEncoding::NarrowToWide (JoinRows (rig.Screen())).c_str());
        Assert::IsTrue (rig.ReadTrack (kTrack) == rig.ImageTrack (kTrack), L"drive 2's catalog over the link the cassette started");
    }



    //  A byte of the cassette audio's data flipped: the monitor's checksum
    //  fails and it reports ERR, the shim doesn't start, and the slot finder
    //  has the monitor read the tape again. The audio sent again, with
    //  nothing typed, starts it.
    TEST_METHOD (ACassetteFlip_FailsTheMonitorsCheck_AndTheAudioSentAgainLoads)
    {
        FaultsEnabled             enabled;
        Rig                       rig (true);
        std::vector<std::string>  reports;
        std::vector<LinkFault>    faults;
        std::string               screen;



        LinkFaults::SetReporter ([&reports] (const std::string & text) { reports.push_back (text); });
        AssertSucceeded (LinkFault::Parse ("cassette:flip@100", faults));
        LinkFaults::Arm (faults);

        rig.StartAtThePrompt();
        rig.Type ("CALL -151\r");
        PlayCassette (rig, true);

        screen = JoinRows (rig.Screen());
        LinkFaults::SetReporter (nullptr);

        Assert::AreEqual ((size_t) 1, reports.size(), L"the fault was injected");
        Assert::IsTrue (screen.find ("ERR") != std::string::npos, TextEncoding::NarrowToWide (screen).c_str());
        Assert::IsTrue (screen.find ("SSC, SLOT 5") == std::string::npos, L"the shim didn't start");

        PlayCassette (rig, false);
        Assert::IsTrue (rig.Screen()[4].starts_with ("SSC, SLOT 5, 115200"), TextEncoding::NarrowToWide (rig.Screen()[4]).c_str());
    }
};
