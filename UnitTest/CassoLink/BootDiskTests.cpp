#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "../EmuTests/GuestSession.h"
#include "../EmuTests/TestMachine.h"
#include "../EmuTests/TextScreenScraper.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Seams/InProcessByteStream.h"
#include "InMemoryCassoLinkFolder.h"
#include "SteppedByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BootDiskTests
//
//  The disks Casso makes, booted on an emulated //c: the link disk, the
//  universal disk with both ports, and the link disk for a Wi-Fi modem. Each
//  boots into the shim, and the host on the test's thread loads the program
//  through a SteppedByteStream. For the modem, the test is the modem: it
//  reads the dial command and replies CONNECT.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BootDiskTests)
{
public:

    static constexpr uint64_t  kBootCycles  = 20'000'000;
    static constexpr uint64_t  kStepCycles  = 17'030;
    static constexpr uint64_t  kDialBudget  = 20'000'000;
    static constexpr Byte      kTrack       = 17;

    struct Rig
    {
        TestMachine                           machine { "Apple2c" };
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::shared_ptr<SteppedByteStream>    stream;
        InMemoryCassoLinkFolder               folder;
        std::unique_ptr<CassoLinkHost>        host;

        //  `boot` in drive 1 and the DOS 3.3 fixture in drive 2, the PC on
        //  `port`, booted.
        Rig (const std::vector<Byte> & boot, int port)
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;



            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

            GuestSession::Mount (machine, boot);
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, std::vector<Byte> (bytes.begin(), bytes.end())));

            InProcessByteStream::CreatePair (machineEnd, pcEnd);
            AssertSucceeded (machine.AttachSerialStream (port, machineEnd));

            stream = std::make_shared<SteppedByteStream> (machine, pcEnd);
        }

        void  Boot()
        {
            machine.RunCycles (kBootCycles);
        }

        std::string  Row (int row)
        {
            std::string  text = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400)[(size_t) row];



            return text.substr (0, text.find_last_not_of (' ') + 1);
        }

        //  A host on the line, and drive 2's catalog read through it, which
        //  loads the program into the shim. With `prompt`, the host first sends
        //  Idle, as a session's connection does, for a shim waiting on the PC.
        void  Link (bool prompt = false)
        {
            const Byte         sector[] = { 0 };
            std::vector<Byte>  data;
            CassoLinkStatus    status;



            host = std::make_unique<CassoLinkHost> (stream, folder, [] () { return std::array<Byte, 4> {}; });

            if (prompt)
            {
                AssertSucceeded (host->PromptReady());
            }

            AssertSucceeded (host->ReadSectors (2, kTrack, sector, data, status));
            Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, L"drive 2's catalog");
            Assert::AreEqual (LinkProgram::kProgramVersion, (Word) (machine.GetCpu()->ReadByte (0x0803) | (machine.GetCpu()->ReadByte (0x0804) << 8)), L"the program was loaded");

            machine.RunCycles (kBootCycles / 40);
        }
    };

    static void  AssertRowStarts (Rig & rig, int row, const std::string & text)
    {
        Assert::IsTrue (rig.Row (row).starts_with (text), TextEncoding::NarrowToWide (rig.Row (row)).c_str());
    }



    //  The disk Save link to disk makes boots into the shim at the link's
    //  speed; the shim has no program, so the host loads it.
    TEST_METHOD (TheLinkDisk_BootsIntoTheShim_AndTheProgramIsLoaded)
    {
        std::vector<Byte>  boot;



        AssertSucceeded (LinkProgram::BuildBootDisk (CassoLinkSettings::kDefaultBaud, boot));

        Rig  rig (boot, 2);

        rig.Boot();
        AssertRowStarts (rig, 2, "WAITING FOR THE PC ON:");
        AssertRowStarts (rig, 4, "SERIAL PORT 2, 115200");

        rig.Link();
        AssertRowStarts (rig, 0, "CASSOLINK");
    }



    //  The universal disk lists both of the //c's ports and waits on both;
    //  the PC on port 1 has the shim started there.
    TEST_METHOD (TheUniversalDisk_ListsBothPorts_AndLinksOverPort1)
    {
        std::vector<Byte>  boot;
        LinkShimOptions    options;



        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, boot));

        Rig  rig (boot, 1);

        rig.Boot();
        AssertRowStarts (rig, 2, "WAITING FOR THE PC ON:");
        AssertRowStarts (rig, 4, "SERIAL PORT 1, 115200");
        AssertRowStarts (rig, 5, "SERIAL PORT 2, 115200");

        rig.Link (true);
        Assert::AreEqual<Byte> (0x10, rig.machine.GetCpu()->ReadByte (0x0006), L"the program's 6551 is port 1's");
    }



    //  The link disk for a Wi-Fi modem sends its dial command and draws the
    //  target while it waits; after CONNECT, the link is over the modem.
    TEST_METHOD (AWifiModemLinkDisk_DialsThePc_AndConnects)
    {
        static constexpr const char *  kDial = "ATDT casso.local:6400";

        std::vector<Byte>  boot;
        std::string        dialed;
        Byte               chunk[64] = {};
        size_t             got       = 0;
        uint64_t           spent     = 0;



        AssertSucceeded (LinkProgram::BuildBootDisk (CassoLinkSettings::kDefaultBaud, boot, kDial));

        Rig  rig (boot, 2);

        while (dialed.find ('\r') == std::string::npos && spent < kDialBudget)
        {
            rig.machine.RunCycles (kStepCycles);
            spent += kStepCycles;

            AssertSucceeded (rig.pcEnd->Read (chunk, sizeof (chunk), 0, got));
            dialed.append (chunk, chunk + got);
        }

        Assert::AreEqual (std::string (kDial) + "\r", dialed, L"the dial command");

        rig.machine.RunCycles (kBootCycles / 20);
        AssertRowStarts (rig, 2, "DIALING CASSO.LOCAL:6400");

        AssertSucceeded (rig.pcEnd->Write (reinterpret_cast<const Byte *> ("CONNECT\r\n"), 9));

        rig.Link();
        AssertRowStarts (rig, 1, "WI-FI, PORT 2, 115200");
        AssertRowStarts (rig, 2, "DIALED:     CASSO.LOCAL:6400");
    }
};
