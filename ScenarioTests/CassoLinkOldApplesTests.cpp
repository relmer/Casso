#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/LinkBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CassoLinkConnector.h"
#include "CassoLink/Session/RawWriteStream.h"
#include "CassoLink/SteppedByteStream.h"
#include "Core/MachineConfig.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/SectorInterleave.h"
#include "Machines/Apple2/Common/SuperSerialCard.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkOldApplesTests
//
//  CassoLink on the emulated Apple ][ plus and the original Apple ][: 48K,
//  no language card, Apple's Super Serial Card in slot 2 and a Disk II in
//  slot 6. DOS 3.3 is booted from its master, then IN#2 and Ctrl-A 14B are
//  typed, and the PC's lines are written to the card at the session's pace
//  in the machine's cycles. The ][ boots into Integer BASIC, with its >
//  prompt and the old monitor, which ignores the Control-Reset vector.
//
//  These are scenario tests because they boot DOS 3.3's master and run
//  Apple's firmware for the card, which scripts/FetchRoms.ps1 fetches.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkOldApplesTests)
{
public:

    static constexpr uint64_t  kClockHz       = 1'020'484;
    static constexpr uint64_t  kKeyCycles     = 100'000;
    static constexpr uint64_t  kStepCycles    = 17'030;
    static constexpr uint64_t  kCharCycles    = (uint64_t) LinkBootstrap::kCharPauseUs * kClockHz / 1'000'000;
    static constexpr uint64_t  kLineCycles    = (uint64_t) LinkBootstrap::kLinePauseMs * kClockHz / 1'000;
    static constexpr uint64_t  kProbeWait     = (uint64_t) CassoLinkConnector::kPromptProbeWaitMs * kClockHz / 1'000;
    static constexpr uint64_t  kLoaderWait    = (uint64_t) LinkBootstrap::kLoaderReadyWaitMs * kClockHz / 1'000;
    static constexpr DWORD     kBootstrapBaud = 9600;
    static constexpr int       kPort          = 2;
    static constexpr Byte      kCatalogTrack  = 17;
    static constexpr Byte      kTracks        = 35;

    //  Communications mode at 9,600 baud, and the second bank as it ships.
    static constexpr const char *  kCommunicationsSw1 = "off off off on on on on";
    static constexpr const char *  kShippedSw2        = "on on off on off off off";

    static bool  IsIntegerBasic (const std::string & machineId)
    {
        return machineId == "Apple2";
    }

    static std::filesystem::path  RequireFirmwarePath()
    {
        std::filesystem::path  folder  = PathResolver::GetDataFolder();
        std::filesystem::path  path    = folder / L"Devices" / L"SuperSerialCard" / L"SSC.rom";
        std::error_code        ec;
        std::wstring           message = std::format (
            L"Apple's Super Serial Card firmware is REQUIRED by this case and was not found in the "
            L"data folder {}. To get it, run scripts/FetchRoms.ps1", folder.wstring());



        Assert::AreEqual<uintmax_t> (2048, std::filesystem::file_size (path, ec), message.c_str());

        return path;
    }

    static void  ConfigureCard (MachineConfig & config)
    {
        std::filesystem::path  firmware = RequireFirmwarePath();



        for (SlotConfig & slot : config.slots)
        {
            if (slot.device == kpszSuperSerialCardDevice)
            {
                slot.slot           = kPort;
                slot.enabled        = true;
                slot.firmwarePath   = firmware.string();
                slot.firmwareStatus = SlotFirmwareStatus::Present;
                slot.switchesSw1    = kCommunicationsSw1;
                slot.switchesSw2    = kShippedSw2;
                slot.ports.clear();
            }
        }
    }

    //  The machine at DOS 3.3's prompt, IN#2 and Ctrl-A 14B typed, and the
    //  DOS 3.3 fixture in drive 2.
    struct Rig
    {
        std::string                           machineId;
        TestMachine                           machine;
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::vector<Byte>                     disk;

        explicit Rig (const char * id)
            : machineId (id),
              machine   (id, [] (MachineConfig & config) { ConfigureCard (config); })
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;



            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

            GuestSession::BootToPrompt (machine, GuestSession::RequireDos33Master());

            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            disk.assign (bytes.begin(), bytes.end());
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));

            InProcessByteStream::CreatePair (machineEnd, pcEnd);
            AssertSucceeded (machine.AttachSerialStream (kPort, machineEnd));

            Type ("IN#2\r");
        }

        void  Type (const char * text)
        {
            for (size_t i = 0; text[i] != '\0'; i++)
            {
                AssertSucceeded (KeystrokeInjector::InjectKey (machine, (Byte) text[i]));
                machine.RunCycles (kKeyCycles);
            }
        }

        void  Send (const std::vector<std::string> & lines)
        {
            for (const std::string & line : lines)
            {
                for (char c : line)
                {
                    AssertSucceeded (pcEnd->Write (reinterpret_cast<const Byte *> (&c), 1));
                    machine.RunCycles (kCharCycles);
                }

                machine.RunCycles (kLineCycles);
            }
        }

        //  Whether $A5 arrives within `budget` cycles.
        bool  AwaitReady (uint64_t budget)
        {
            Byte      got[64] = {};
            size_t    read    = 0;
            uint64_t  spent   = 0;



            while (spent <= budget)
            {
                AssertSucceeded (pcEnd->Read (got, sizeof (got), 0, read));

                if (std::find (got, got + read, LinkBootstrap::kLoaderReady) != got + read)
                {
                    return true;
                }

                machine.RunCycles (kStepCycles);
                spent += kStepCycles;
            }

            return false;
        }

        //  The loader typed into the monitor, its $A5, and the shim sent.
        void  Bootstrap()
        {
            std::vector<std::string>  lines;
            std::vector<Byte>         transfer;



            AssertSucceeded (LinkBootstrap::BuildLines (lines));
            Send (lines);
            Assert::IsTrue (AwaitReady (kLoaderWait), Widen ("the loader's $A5: " + Screen()).c_str());

            AssertSucceeded (LinkBootstrap::BuildShimTransfer (kBootstrapBaud, transfer));
            AssertSucceeded (pcEnd->Write (transfer.data(), transfer.size()));
        }

        //  A track's 16 physical sectors from drive 2's image, in order.
        std::vector<Byte>  ImageTrack (Byte track)
        {
            std::vector<Byte>  data;



            for (int p = 0; p < 16; p++)
            {
                size_t  offset = (size_t) (track * 16 + SectorInterleave::GetDosFileIndexForPhysicalSector (p)) * 256;

                data.insert (data.end(), disk.begin() + (ptrdiff_t) offset, disk.begin() + (ptrdiff_t) offset + 256);
            }

            return data;
        }

        std::string  Screen()
        {
            std::string  text;



            for (const std::string & row : TextScreenScraper::Scrape (machine))
            {
                text += GuestSession::TrimTrailingBlanks (row) + "|";
            }

            return text;
        }

        //  Whether the screen's last nonblank row is the prompt `glyph`, with
        //  at most the cursor after it.
        bool  IsAtPrompt (char glyph)
        {
            std::vector<std::string>  rows = TextScreenScraper::Scrape (machine);
            std::string               last;



            for (const std::string & row : rows)
            {
                std::string  trimmed = GuestSession::TrimTrailingBlanks (row);

                if (!trimmed.empty())
                {
                    last = trimmed;
                }
            }

            // The flashing cursor after the prompt is scraped as a character.
            return !last.empty() && last[0] == glyph && last.size() <= 2;
        }

        std::wstring  Widen (const std::string & text) const
        {
            return TextEncoding::NarrowToWide (machineId + ": " + text);
        }
    };

    static std::vector<Byte>  AllSectors()
    {
        std::vector<Byte>  sectors (16);



        for (size_t i = 0; i < sectors.size(); i++)
        {
            sectors[i] = (Byte) i;
        }

        return sectors;
    }

    static std::unique_ptr<CassoLinkHost>  MakeHost (Rig & rig, InMemoryCassoLinkFolder & folder)
    {
        return std::make_unique<CassoLinkHost> (std::make_shared<SteppedByteStream> (rig.machine, rig.pcEnd), folder, [] () { return std::array<Byte, 4> {}; });
    }



    //  The PC's prompt probe, at Applesoft's ] prompt and at Integer BASIC's >,
    //  under DOS 3.3 with IN#2 typed, sends $A5 within the probe's wait.
    static void  RunProbe (const char * machineId)
    {
        Rig                       rig (machineId);
        std::vector<std::string>  lines (std::begin (CassoLinkConnector::kPromptProbe), std::end (CassoLinkConnector::kPromptProbe));



        Assert::IsTrue (rig.IsAtPrompt (IsIntegerBasic (machineId) ? '>' : ']'), rig.Widen (rig.Screen()).c_str());

        lines.insert (lines.begin(), LinkBootstrap::kCancelLine);
        rig.Send (lines);

        Assert::IsTrue (rig.AwaitReady (kProbeWait), rig.Widen ("the probe's $A5: " + rig.Screen()).c_str());
    }



    //  The typed bootstrap, then over the link: drive 2's catalog, every
    //  track read as a Full Send reads it, a sector written and read back,
    //  a raw capture of a track, and a raw write of a track. The PC's Exit
    //  then returns to the machine's BASIC at its prompt, with DOS working.
    static void  RunLink (const char * machineId)
    {
        Rig                             rig (machineId);
        InMemoryCassoLinkFolder         folder;
        std::unique_ptr<CassoLinkHost>  host;
        std::vector<Byte>               sectors = AllSectors();
        std::vector<Byte>               data;
        std::vector<Byte>               pattern (256, 0x5A);
        std::vector<Byte>               module;
        std::vector<Byte>               bytes;
        std::vector<uint8_t>            bits;
        RawWriteStream                  stream;
        CassoLinkStatus                 status;
        CaptureTiming                   timing;
        const Byte                      sector[] = { 3 };
        char                            prompt   = IsIntegerBasic (machineId) ? '>' : ']';



        rig.Bootstrap();
        host = MakeHost (rig, folder);

        //  The catalog, then every track.
        AssertSucceeded (host->ReadSectors (2, kCatalogTrack, sectors, data, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, rig.Widen ("the catalog").c_str());
        Assert::IsTrue (data == rig.ImageTrack (kCatalogTrack), rig.Widen ("drive 2's catalog track").c_str());

        for (Byte track = 0; track < kTracks; track++)
        {
            AssertSucceeded (host->ReadSectors (2, track, sectors, data, status));
            Assert::IsTrue (data == rig.ImageTrack (track), rig.Widen (std::format ("track {}", track)).c_str());
        }

        //  A sector written and read back.
        AssertSucceeded (host->WriteSectors (2, 20, sector, pattern, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, rig.Widen ("the write").c_str());
        AssertSucceeded (host->ReadSectors (2, 20, sector, data, status));
        Assert::IsTrue (data == pattern, rig.Widen ("the sector written reads back").c_str());

        //  A raw capture of track 0.
        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (host->EnsureModule (module));
        AssertSucceeded (host->CaptureQuarterTrack (2, 0, 0, 0, 100, CassoLinkHost::kNoSync, {}, bytes, status, timing));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, rig.Widen ("the raw capture").c_str());
        Assert::IsTrue (bytes.size() > 6000, rig.Widen (std::format ("the capture's {} bytes", bytes.size())).c_str());

        //  A raw write of track 30: a sync run, then bytes.
        bits.assign (400, 0);

        for (int i = 0; i < 5000; i++)
        {
            Byte  value = (Byte) (0x96 + (i % 0x69));

            for (int bit = 7; bit >= 0; bit--)
            {
                bits.push_back ((uint8_t) (((value | 0x80) >> bit) & 1));
            }
        }

        RawWriteStream::FromBits (bits, stream);
        AssertSucceeded (LinkProgram::BuildThirteenSectorModule (module));
        AssertSucceeded (host->EnsureModule (module));
        AssertSucceeded (host->WriteRawTrack (2, 120, stream, CassoLinkHost::GetTrackPercents (30), status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, rig.Widen ("the raw write").c_str());

        //  The PC's Exit: the machine's BASIC, with DOS's CATALOG working.
        AssertSucceeded (host->Exit());
        host.reset();
        rig.machine.RunCycles (2 * kClockHz);

        Assert::IsTrue (rig.IsAtPrompt (prompt), rig.Widen ("BASIC's prompt after Exit: " + rig.Screen()).c_str());

        //  BASIC's program is empty: LIST prints nothing.
        rig.Type ("LIST\r");
        rig.machine.RunCycles (kClockHz);
        {
            std::string  screen = rig.Screen();
            size_t       listed = screen.rfind (std::string (1, prompt) + "LIST|");

            size_t       next   = screen.find_first_not_of ('|', listed + 6);

            Assert::IsTrue (listed != std::string::npos && next != std::string::npos && screen[next] == prompt, rig.Widen ("LIST after Exit: " + screen).c_str());
        }

        rig.Type ("CATALOG\r");
        rig.machine.RunCycles (5 * kClockHz);
        Assert::AreEqual (std::string::npos, rig.Screen().find ("ERROR"), rig.Widen (rig.Screen()).c_str());
        Assert::AreNotEqual (std::string::npos, rig.Screen().find ("HELLO"), rig.Widen (rig.Screen()).c_str());
    }



    //  Control-Reset during a link. On the ][ plus, the autostart ROM jumps
    //  through the vector at $03F2, and CassoLink ends at Applesoft's prompt.
    //  The ]['s old monitor ignores the vector, so the reset lands at the
    //  monitor's * prompt; 3D0G, DOS's warm start, returns to Integer BASIC, and
    //  DOS's CATALOG works.
    static void  RunControlReset (const char * machineId)
    {
        Rig                             rig (machineId);
        InMemoryCassoLinkFolder         folder;
        std::unique_ptr<CassoLinkHost>  host;
        std::vector<Byte>               sectors = AllSectors();
        std::vector<Byte>               data;
        CassoLinkStatus                 status;
        bool                            integer = IsIntegerBasic (machineId);



        rig.Bootstrap();
        host = MakeHost (rig, folder);

        AssertSucceeded (host->ReadSectors (2, kCatalogTrack, sectors, data, status));
        host.reset();

        rig.machine.SoftReset();
        rig.machine.RunCycles (2 * kClockHz);

        Assert::IsTrue (rig.IsAtPrompt (integer ? '*' : ']'), rig.Widen ("the prompt after Control-Reset: " + rig.Screen()).c_str());

        if (integer)
        {
            rig.Type ("3D0G\r");
            rig.machine.RunCycles (kClockHz);
            Assert::IsTrue (rig.IsAtPrompt ('>'), rig.Widen ("Integer BASIC's prompt: " + rig.Screen()).c_str());
        }

        rig.Type ("CATALOG\r");
        rig.machine.RunCycles (5 * kClockHz);
        Assert::AreEqual (std::string::npos, rig.Screen().find ("ERROR"), rig.Widen (rig.Screen()).c_str());
        Assert::AreNotEqual (std::string::npos, rig.Screen().find ("HELLO"), rig.Widen (rig.Screen()).c_str());
    }



    TEST_METHOD (TheProbe_AtApplesoftsPrompt_SendsA5_OnAnApple2Plus)                 { RunProbe        ("Apple2Plus"); }
    TEST_METHOD (TheProbe_AtIntegerBasicsPrompt_SendsA5_OnAnApple2)                   { RunProbe        ("Apple2"); }
    TEST_METHOD (TheBootstrap_AndALink_ReadWriteCaptureAndRawWrite_AndExit_OnAnApple2Plus) { RunLink         ("Apple2Plus"); }
    TEST_METHOD (TheBootstrap_AndALink_ReadWriteCaptureAndRawWrite_AndExit_OnAnApple2)     { RunLink         ("Apple2"); }
    TEST_METHOD (ControlReset_DuringALink_EndsAtApplesoft_OnAnApple2Plus)             { RunControlReset ("Apple2Plus"); }
    TEST_METHOD (ControlReset_DuringALink_EndsAtTheOldMonitor_OnAnApple2)             { RunControlReset ("Apple2"); }
};