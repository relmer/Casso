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
//  CassoLinkBootDiskTests
//
//  The disks Casso makes, booted on an emulated //c: the link disk, the
//  universal disk with both ports, and the link disk for a Wi-Fi modem. Each
//  boots into the shim, and the host on the test's thread loads the program
//  through a SteppedByteStream. For the modem, the test is the modem: it
//  reads the dial command and replies CONNECT.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkBootDiskTests)
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

            GuestSession::Mount (machine, boot);
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, std::vector<Byte> (bytes.begin(), bytes.end())));

            InProcessByteStream::CreateVirtualPair (machineEnd, pcEnd);
            AssertSucceeded (machine.GetConnectors().AttachSerialStream (port, machineEnd));

            stream = std::make_shared<SteppedByteStream> (machine, pcEnd);
        }

        void  Boot()
        {
            machine.GetRunner().RunCycles (kBootCycles);
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

            machine.GetRunner().RunCycles (kBootCycles / 40);
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
        AssertRowStarts (rig, 0, "CassoLink");
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
        AssertRowStarts (rig, 0, std::format ("CASSOLINK LOADER V1.{:02}", LinkProgram::kShimVersion));
        AssertRowStarts (rig, 2, "WAITING FOR THE PC ON:");
        AssertRowStarts (rig, 4, "SERIAL PORT 1, 115200");
        AssertRowStarts (rig, 5, "SERIAL PORT 2, 115200");

        rig.Link (true);
        Assert::AreEqual<Byte> (0x10, rig.machine.GetCpu()->ReadByte (0x0006), L"the program's 6551 is port 1's");
    }



    //  A boot disk written before the shim changed, its shim version 11 at
    //  19,200 baud: the host sends it the current shim, with the disk's
    //  settings, and then the current program.
    TEST_METHOD (AnOlderBootDisk_IsSentTheCurrentShim_AndLinks)
    {
        FixtureProvider       fixtures;
        std::vector<uint8_t>  bytes;



        AssertSucceeded (fixtures.OpenFixture ("CassoLink/link-shim11-19200.dsk", bytes));

        Rig  rig (std::vector<Byte> (bytes.begin(), bytes.end()), 2);

        rig.Boot();
        AssertRowStarts (rig, 2, "WAITING FOR THE PC ON:");

        rig.Link (true);
        AssertRowStarts (rig, 0, "CassoLink");
        AssertRowStarts (rig, 1, "Serial port 2");
    }


    //  A universal disk written before its first step took any byte as the
    //  PC can miss the start byte of an Idle on a //c's port 1, so the
    //  connection sends Idle again each second until the shim's Ready comes.
    TEST_METHOD (AnOlderUniversalDisk_OnPort1_LinksAfterRepeatedIdles)
    {
        static constexpr uint64_t  kSecondCycles = 1'020'484;

        FixtureProvider       fixtures;
        std::vector<uint8_t>  bytes;
        const Byte            sector[] = { 0 };
        std::vector<Byte>     data;
        CassoLinkStatus       status;



        AssertSucceeded (fixtures.OpenFixture ("CassoLink/universal-stage-before-port1-fix.dsk", bytes));

        Rig  rig (std::vector<Byte> (bytes.begin(), bytes.end()), 1);

        rig.Boot();
        AssertRowStarts (rig, 4, "SERIAL PORT 1");

        rig.host = std::make_unique<CassoLinkHost> (rig.stream, rig.folder, [] () { return std::array<Byte, 4> {}; });

        for (int prompt = 0; prompt < 5 && rig.machine.GetCpu()->GetPC() < LinkProgram::kShimAddress; prompt++)
        {
            AssertSucceeded (rig.host->PromptReady());
            rig.machine.GetRunner().RunCycles (kSecondCycles);
        }

        AssertSucceeded (rig.host->ReadSectors (2, kTrack, sector, data, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, L"drive 2's catalog");
    }


    //  The updater run over a shim of each layout the 6551 shim has had: at
    //  $3D00 with the zero page at $3C00, at $3B00 with it at $3A00, and at
    //  $3A00 with it at $3900. Each old shim's speed, slot and saved zero
    //  page are copied to the current shim, which then runs.
    TEST_METHOD (TheShimUpdate_FindsEachOlderLayout)
    {
        static constexpr Word  kBases[] = { 0x3D00, 0x3B00, 0x3A00 };

        std::vector<Byte>  update;



        AssertSucceeded (LinkProgram::BuildShimUpdate (update));

        for (Word base : kBases)
        {
            TestMachine       machine { "Apple2c" };
            EmuCpu          * cpu     = machine.GetCpu();
            std::wstring      where   = std::format (L"the shim at ${:04X}", base);



            for (Word at = 0x3900; at < 0x4400; at++)
            {
                cpu->WriteByte (at, 0);
            }

            for (int i = 0; i < 256; i++)
            {
                cpu->WriteByte ((Word) (base - 0x100 + i), (Byte) (i ^ 0x5A));
            }

            cpu->WriteByte (base,        0x78);
            cpu->WriteByte (base + 1,    0xA9);
            cpu->WriteByte (base + 2,    0x1F);
            cpu->WriteByte (base + 3,    0x4C);

            for (int i = 0; i < 6; i++)
            {
                cpu->WriteByte ((Word) (base + 6 + i), (Byte) "19200 "[i]);
            }

            cpu->WriteByte (base + 0x34, 1);
            cpu->WriteByte (base + 0x38, 2);

            for (size_t i = 0; i < update.size(); i++)
            {
                cpu->WriteByte ((Word) (LinkProgram::kProgramAddress + i), update[i]);
            }

            cpu->SetPC (LinkProgram::kProgramAddress);

            machine.GetRunner().RunCycles (60'000);

            Assert::AreEqual<Byte> (0x1F, cpu->ReadByte (0x3A02), where.c_str());
            Assert::AreEqual<Byte> ((Byte) '1', cpu->ReadByte (0x3A06), where.c_str());
            Assert::AreEqual<Byte> (0, cpu->ReadByte (0x3A0C), (where + L": no dial command").c_str());
            Assert::AreEqual<Byte> (2, cpu->ReadByte (0x3A38), where.c_str());
            Assert::AreEqual<Byte> ((Byte) (7 ^ 0x5A), cpu->ReadByte (0x3907), (where + L": the saved zero page").c_str());
            Assert::IsTrue (cpu->GetPC() >= LinkProgram::kShimAddress && cpu->GetPC() < 0x4400, (where + L": the current shim runs").c_str());
        }
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
            rig.machine.GetRunner().RunCycles (kStepCycles);
            spent += kStepCycles;

            AssertSucceeded (rig.pcEnd->Read (chunk, sizeof (chunk), 0, got));
            dialed.append (chunk, chunk + got);
        }

        Assert::AreEqual (std::string (kDial) + "\r", dialed, L"the dial command");

        rig.machine.GetRunner().RunCycles (kBootCycles / 20);
        AssertRowStarts (rig, 2, "DIALING CASSO.LOCAL:6400");

        AssertSucceeded (rig.pcEnd->Write (reinterpret_cast<const Byte *> ("CONNECT\r\n"), 9));

        rig.Link();
        AssertRowStarts (rig, 1, "Wi-Fi, port 2, 115200");
        AssertRowStarts (rig, 2, "Dialed:     CASSO.LOCAL:6400");
    }
};
