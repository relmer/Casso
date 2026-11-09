#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "../EmuTests/GuestSession.h"
#include "../EmuTests/TestMachine.h"
#include "../NetworkTests/FakeHostNetwork.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkAnnouncement.h"
#include "CassoLink/LinkFault.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Network/VirtualSubnet.h"
#include "Devices/RamDevice.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Seams/IByteStream.h"
#include "InMemoryCassoLinkFolder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetProgramTests
//
//  The shim and program built for an Uthernet II's W5100, on an enhanced //e
//  with the card in slot 3 and a FakeHostNetwork behind it. The test is the
//  PC's end of each TCP connection: it offers a connection to the card's
//  listener, and the real host talks to the program through it, with the
//  machine run only while the host waits for bytes. Nothing waits on the
//  PC's clock or opens a real socket.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (UthernetProgramTests)
{
public:

    static constexpr uint64_t  kBootCycles  = 20'000'000;
    static constexpr uint64_t  kStepCycles  = 17'030;
    static constexpr uint64_t  kWaitCycles  = 40'000'000;
    static constexpr uint16_t  kPort        = 26502;
    static constexpr int       kSlot        = 3;
    static constexpr Byte      kTrack       = 17;
    static constexpr int       kRowStatus   = 0;
    static constexpr int       kRowCard     = 1;
    static constexpr int       kRowAddress  = 2;
    static constexpr int       kRowNetwork  = 4;
    static constexpr Word      kSaltAt      = 0x5000;   // memory the universal disk salts its MAC address with

    //  The PC's end of a connection the card accepted: what the test writes
    //  is the card's inbound data, and what the card sends is read. A read
    //  with nothing waiting runs the machine a frame at a time, its timeout
    //  counted in the machine's cycles.
    class SocketStream : public IByteStream
    {
    public:
        SocketStream (TestMachine & machine, std::shared_ptr<FakeSocketState> state) : m_machine (machine), m_state (std::move (state)) {}

        HRESULT  Write (const Byte * data, size_t count) override
        {
            m_state->inbound.insert (m_state->inbound.end(), data, data + count);
            return S_OK;
        }

        HRESULT  Read (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override
        {
            uint64_t  budget = (uint64_t) ((double) timeoutMs * CassoLinkHost::kCpuCyclesPerMs);
            uint64_t  spent  = 0;



            while (m_state->sent.empty() && !IsClosed() && spent < budget)
            {
                m_machine.RunCycles (kStepCycles);
                spent += kStepCycles;
            }

            outRead = std::min (capacity, m_state->sent.size());
            std::copy (m_state->sent.begin(), m_state->sent.begin() + (ptrdiff_t) outRead, buffer);
            m_state->sent.erase (m_state->sent.begin(), m_state->sent.begin() + (ptrdiff_t) outRead);
            return S_OK;
        }

        size_t  GetReadableCount () override         { return m_state->sent.size(); }
        bool    IsClosed         () const override   { return m_state->destroyed || m_state->shutdownSent; }
        void    Close            () override         { m_state->inboundEof = true; }

    private:
        TestMachine                       & m_machine;
        std::shared_ptr<FakeSocketState>    m_state;
    };

    struct Rig
    {
        FakeHostNetwork                   network;
        TestMachine                       machine;
        InMemoryCassoLinkFolder           folder;
        std::vector<Byte>                 dos;
        std::shared_ptr<FakeSocketState>  connection;
        std::unique_ptr<CassoLinkHost>    host;
        std::vector<std::string>          sent;   // the host's packets, as text

        //  The card enabled in slot 3, `boot` in drive 1 and the DOS 3.3
        //  fixture in drive 2, with `salt` at $5000, booted.
        Rig (const std::vector<Byte> & boot, Byte salt = 0)
            : machine ("Apple2eEnhanced", TestMachine::Slots::AsShipped,
                       [this] (MachineConfig & config, MachineBuildServices & services)
                       {
                           for (SlotConfig & slot : config.slots)
                           {
                               if (slot.device == "uthernet2")
                               {
                                   slot.enabled = true;
                               }
                           }

                           services.hostNetwork = &network;
                       })
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;



            Assert::IsNotNull (machine.GetRefs().uthernet, L"the card must be installed");

            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            dos.assign (bytes.begin(), bytes.end());

            GuestSession::Mount (machine, boot);
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, dos));
            machine.GetMemoryBus().WriteByte (kSaltAt, salt);
            machine.RunCycles (kBootCycles);
        }

        //  The card's listener on `port`, once it is listening.
        std::shared_ptr<FakeSocketState>  AwaitListener (uint16_t port)
        {
            std::shared_ptr<FakeSocketState>  listener;
            uint64_t                          spent = 0;



            for (listener = network.FindBound (HostSocketKind::Tcp, port);
                 !(listener && listener->listening && !listener->destroyed) && spent < kWaitCycles;
                 listener = network.FindBound (HostSocketKind::Tcp, port))
            {
                machine.RunCycles (kStepCycles);
                spent += kStepCycles;
            }

            Assert::IsTrue (listener && listener->listening, std::format (L"a listener on port {}", port).c_str());
            return listener;
        }

        //  A connection from the PC to the card's listener, and a host on it.
        void  Connect (uint16_t port = kPort)
        {
            static constexpr uint16_t  kPcPort = 50000;

            connection = network.OfferConnection (AwaitListener (port), HostEndpoint { FakeHostNetwork::kLoopback, kPcPort });
            host       = std::make_unique<CassoLinkHost> (std::make_shared<SocketStream> (machine, connection), folder, [] () { return std::array<Byte, 4> {}; });
            host->SetTraceSink ([this] (bool isSent, const std::string & text) { if (isSent) { sent.push_back (text); } });
        }

        //  The PC's end closed, and the machine run until the card's socket
        //  is closed too.
        void  Disconnect()
        {
            uint64_t  spent = 0;



            connection->inboundEof = true;

            while (!(connection->destroyed || connection->shutdownSent) && spent < kWaitCycles)
            {
                machine.RunCycles (kStepCycles);
                spent += kStepCycles;
            }

            host.reset();
        }

        std::string  Row (int row)
        {
            std::string  text;



            for (int i = 0; i < 40; i++)
            {
                text += (char) (machine.GetCpu()->ReadByte ((Word) (TextRowAddress (row) + i)) & 0x7F);
            }

            return text.substr (0, text.find_last_not_of (' ') + 1);
        }

        //  A row in 80 columns, each auxiliary memory byte ahead of the main
        //  memory byte at the same offset.
        std::string  Row80 (int row)
        {
            const Byte  * mainRam = machine.GetRefs().mainRamDev->GetData();
            const Byte  * auxRam  = machine.GetMmu()->GetAuxBuffer();
            Word          address = TextRowAddress (row);
            std::string   text;



            for (int i = 0; i < 40; i++)
            {
                text += (char) (auxRam[address + i] & 0x7F);
                text += (char) (mainRam[address + i] & 0x7F);
            }

            return text.substr (0, text.find_last_not_of (' ') + 1);
        }

        static Word  TextRowAddress (int row)
        {
            return (Word) (0x0400 + (row % 8) * 0x80 + (row / 8) * 0x28);
        }

        std::vector<Byte>  ReadTrack (Byte drive, Byte track)
        {
            std::vector<Byte>  sectors (16);
            std::vector<Byte>  data;
            CassoLinkStatus    status;



            for (size_t i = 0; i < sectors.size(); i++)
            {
                sectors[i] = (Byte) i;
            }

            AssertSucceeded (host->ReadSectors (drive, track, sectors, data, status));
            Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, std::format (L"drive {} track {}", drive, track).c_str());
            return data;
        }

        //  A physical sector's 256 bytes in drive 2's image.
        std::vector<Byte>  ImageTrack (Byte track)
        {
            std::vector<Byte>  data;



            for (int p = 0; p < 16; p++)
            {
                size_t  offset = (size_t) (track * 16 + NibblizationLayer::GetDosFileIndexForPhysicalSector (p)) * 256;

                data.insert (data.end(), dos.begin() + (ptrdiff_t) offset, dos.begin() + (ptrdiff_t) offset + 256);
            }

            return data;
        }

        //  The announcements the card has broadcast, in order.
        std::vector<LinkAnnouncement>  Announcements()
        {
            std::vector<LinkAnnouncement>  found;



            for (const auto & socket : network.GetSockets())
            {
                for (const auto & datagram : socket->sentDatagrams)
                {
                    LinkAnnouncement  announcement;

                    if (datagram.peer.port == LinkProgram::kAnnouncePort && LinkAnnouncement::Parse (datagram.bytes, announcement))
                    {
                        found.push_back (announcement);
                    }
                }
            }

            return found;
        }

        std::array<Byte, 6>  Mac()
        {
            std::array<Byte, 6>  mac = {};



            for (size_t at = 0; at < mac.size(); at++)
            {
                mac[at] = machine.GetCpu()->ReadByte ((Word) (LinkProgram::kUniversalW5100 + LinkProgram::kNetMacOffset + at));
            }

            return mac;
        }

        void  RunUntil (const std::function<bool()> & done, const wchar_t * what)
        {
            uint64_t  spent = 0;



            while (!done() && spent < kWaitCycles)
            {
                machine.RunCycles (kStepCycles);
                spent += kStepCycles;
            }

            Assert::IsTrue (done(), what);
        }
    };

    static std::vector<Byte>  LinkDisk (uint16_t port = kPort)
    {
        std::vector<Byte>  boot;
        LinkShimOptions    options;



        options.chip            = LinkChip::W5100;
        options.slot            = kSlot;
        options.network.address = { 127, 0, 0, 1 };
        options.network.port    = port;

        AssertSucceeded (LinkProgram::BuildBootDisk (options, boot));
        return boot;
    }

    static std::vector<Byte>  UniversalDisk (LinkShimOptions & outOptions)
    {
        std::vector<Byte>  boot;



        outOptions.network.address = { 192, 168, 77, 77 };
        outOptions.network.mac     = { 0x00, 0x08, 0xDC, 0x11, 0x22, 0x33 };
        outOptions.network.port    = kPort;

        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (outOptions, boot));
        return boot;
    }

    static bool  AnyLoad (const std::vector<std::string> & sent)
    {
        return std::any_of (sent.begin(), sent.end(), [] (const std::string & text) { return text.starts_with ("Load program"); });
    }



    TEST_METHOD (TheLinkDisk_Listens_AndShowsItsAddress)
    {
        Rig  rig (LinkDisk());



        rig.AwaitListener (kPort);
        Assert::AreEqual (std::string ("WAITING FOR PC AT 127.0.0.1:26502"), rig.Row (kRowAddress));
    }



    //  A connection loads the W5100 program, which lists, writes and reads
    //  drive 2 and draws the connection.
    TEST_METHOD (AConnection_LoadsTheProgram_AndReadsAndWritesTheDisk)
    {
        Rig                rig (LinkDisk());
        const Byte         sector[] = { 0 };
        std::vector<Byte>  pattern (256);
        std::vector<Byte>  data;
        CassoLinkStatus    status;



        for (size_t i = 0; i < pattern.size(); i++)
        {
            pattern[i] = (Byte) (i * 7 + 3);
        }

        rig.Connect();

        Assert::IsTrue (rig.ReadTrack (2, kTrack) == rig.ImageTrack (kTrack), L"drive 2's catalog track");
        Assert::IsTrue (AnyLoad (rig.sent), L"the program was loaded");

        AssertSucceeded (rig.host->WriteSectors (2, 20, sector, pattern, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        AssertSucceeded (rig.host->ReadSectors (2, 20, sector, data, status));
        Assert::IsTrue (data == pattern, L"the sector written reads back");

        rig.machine.RunCycles (kWaitCycles / 40);
        Assert::AreEqual (std::string ("CASSOLINK: CONNECTED"),         rig.Row (kRowStatus));
        Assert::AreEqual (std::string ("UTHERNET II, SLOT 3"),          rig.Row (kRowCard));
        Assert::AreEqual (std::format ("127.0.0.1:{}", kPort),          rig.Row (kRowAddress), L"the address and port under the connection");
    }



    //  Once the PC disconnects, the program listens again, and a new
    //  connection is served by the program as it is, with no load.
    TEST_METHOD (AfterThePcLeaves_TheProgramListensAgain_WithNoLoad)
    {
        Rig  rig (LinkDisk());



        rig.Connect();
        rig.ReadTrack (2, kTrack);
        rig.Disconnect();

        rig.sent.clear();
        rig.Connect();

        Assert::IsTrue (rig.ReadTrack (2, kTrack) == rig.ImageTrack (kTrack));
        Assert::IsFalse (AnyLoad (rig.sent), L"the program served the connection as it is");
    }



    //  A program of another version is sent Load program and goes back to the
    //  shim, which loads the current program over the same connection.
    TEST_METHOD (AnOlderProgram_IsLoadedAgain_OverTheSameConnection)
    {
        static constexpr Byte  kHeader[]  = { 0xC1, 0x0B, 0x00, CassoLinkCodec::kReady };
        static constexpr int   kVersionAt = 5;
        static constexpr int   kCheckAt   = 16;

        Rig   rig (LinkDisk());
        Word  ready   = 0;
        Byte  version = 0;



        rig.Connect();
        rig.ReadTrack (2, kTrack);
        rig.Disconnect();

        for (Word at = LinkProgram::kProgramAddress; at < LinkProgram::kZeroPageSave && ready == 0; at++)
        {
            bool  match = true;

            for (size_t i = 0; i < std::size (kHeader); i++)
            {
                match = match && rig.machine.GetCpu()->ReadByte ((Word) (at + i)) == kHeader[i];
            }

            ready = match ? at : 0;
        }

        Assert::AreNotEqual ((Word) 0, ready, L"the program's Ready packet");

        version = rig.machine.GetCpu()->ReadByte ((Word) (ready + kVersionAt));
        rig.machine.GetCpu()->WriteByte ((Word) (ready + kVersionAt), (Byte) (version - 1));
        rig.machine.GetCpu()->WriteByte ((Word) (ready + kCheckAt), (Byte) (rig.machine.GetCpu()->ReadByte ((Word) (ready + kCheckAt)) ^ version ^ (Byte) (version - 1)));

        rig.sent.clear();
        rig.Connect();

        Assert::IsTrue (rig.ReadTrack (2, kTrack) == rig.ImageTrack (kTrack));
        Assert::IsTrue (AnyLoad (rig.sent), L"the program was loaded again");
        Assert::AreEqual<int> (version, rig.machine.GetCpu()->ReadByte ((Word) (ready + kVersionAt)), L"the current program");
        Assert::IsFalse (rig.connection->destroyed || rig.connection->shutdownSent, L"the connection stays open while the program loads");
    }



    //  In 80 columns the connection is at the top row's right edge, and the
    //  address and port at the right edge of the row below it.
    TEST_METHOD (EightyColumns_PutTheAddressUnderTheConnection)
    {
        Rig          rig (LinkDisk());
        std::string  address = std::format ("127.0.0.1:{}", kPort);



        rig.Connect();
        AssertSucceeded (rig.host->SetColumns (true));
        rig.machine.RunCycles (kWaitCycles / 40);

        Assert::IsTrue   (rig.Row80 (kRowStatus).ends_with ("UTHERNET II, SLOT 3"), TextEncoding::NarrowToWide (rig.Row80 (kRowStatus)).c_str());
        Assert::AreEqual (std::string (80 - address.size(), ' ') + address, rig.Row80 (kRowCard), L"the address right-justified under the connection");
        Assert::AreEqual (std::string(), rig.Row80 (kRowAddress), L"and nothing on the row the address was on");
    }



    //  A host whose link is for a 6551: the shim's Ready indicates the W5100,
    //  so the program sent is the W5100 build.
    TEST_METHOD (AHostForA6551_LoadsTheW5100Program_ForAW5100Shim)
    {
        Rig                rig (LinkDisk());
        std::vector<Byte>  program;
        std::string        errors;



        AssertSucceeded (LinkProgram::AssembleProgram (program, errors, LinkChip::W5100));

        rig.Connect();
        rig.host->SetProgramChip (LinkChip::Acia);

        rig.ReadTrack (2, kTrack);
        Assert::AreEqual<int> (CassoLinkCodec::ComputeCrc (program), rig.host->GetClient().programCrc, L"the W5100 build");
    }



    //  A byte of the disk the Apple sends, flipped, and a byte the PC sends,
    //  flipped: the track is read again, and the sector sent again.
    TEST_METHOD (FlipsFromEitherEnd_AreRecoveredFrom)
    {
        Rig                rig (LinkDisk());
        const Byte         sector[] = { 0 };
        std::vector<Byte>  pattern (256, 0x6C);
        std::vector<Byte>  data;
        CassoLinkStatus    status;
        LinkFault          fault;
        std::vector<std::string>  activity;



        rig.Connect();
        rig.host->SetFaultProgram (true);
        rig.host->SetActivitySink ([&] (const std::string & text) { activity.push_back (text); });
        rig.ReadTrack (2, kTrack);

        AssertSucceeded (LinkFault::ParseOne ("apple:flip@1000", fault));
        AssertSucceeded (rig.host->ArmFault (fault));
        Assert::IsTrue (rig.ReadTrack (2, kTrack) == rig.ImageTrack (kTrack), L"the track, read again");
        Assert::IsTrue (std::any_of (activity.begin(), activity.end(), [] (const std::string & a) { return a.find ("damaged") != std::string::npos; }), L"the damaged sector was reported");

        AssertSucceeded (LinkFault::ParseOne ("pc:flip@300", fault));
        AssertSucceeded (rig.host->ArmFault (fault));
        AssertSucceeded (rig.host->WriteSectors (2, 20, sector, pattern, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        AssertSucceeded (rig.host->ReadSectors (2, 20, sector, data, status));
        Assert::IsTrue (data == pattern, L"the sector sent again reads back");
    }



    //  The universal disk takes the address the virtual DHCP server offers
    //  over the one it was built with, salts the MAC address's last three
    //  bytes with its memory, and broadcasts an announcement of its address,
    //  port and MAC address; with the PC gone, the program broadcasts one again.
    TEST_METHOD (TheUniversalDisk_TakesADhcpAddress_SaltsItsMac_AndAnnounces)
    {
        LinkShimOptions                     options;
        std::vector<Byte>                   boot   = UniversalDisk (options);
        std::array<std::array<Byte, 6>, 2>  macs   = {};
        std::string                         dhcp   = std::format ("UTHERNET II, SLOT 3, {}.{}.{}.{}:{}",
                                                                  VirtualSubnet::kGuestIp >> 24, (VirtualSubnet::kGuestIp >> 16) & 0xFF,
                                                                  (VirtualSubnet::kGuestIp >> 8) & 0xFF, VirtualSubnet::kGuestIp & 0xFF, kPort);



        for (size_t apple = 0; apple < macs.size(); apple++)
        {
            Rig     rig (boot, (Byte) (0x11 * (apple + 1)));
            size_t  before = 0;

            rig.RunUntil ([&] () { return rig.Row (kRowNetwork).find (':') != std::string::npos; }, L"the address on the screen");
            Assert::AreEqual (dhcp, rig.Row (kRowNetwork), L"the address from DHCP, not the one built in");

            rig.RunUntil ([&] () { return !rig.Announcements().empty(); }, L"an announcement");
            macs[apple] = rig.Mac();

            Assert::IsTrue (rig.Announcements().front().mac == macs[apple], L"the announcement has the card's MAC address");
            Assert::AreEqual<int> (kPort, rig.Announcements().front().port, L"and its listener's port");
            Assert::IsTrue (std::equal (options.network.mac.begin(), options.network.mac.begin() + 3, macs[apple].begin()), L"the first three bytes as built");

            rig.Connect();
            Assert::IsTrue (rig.ReadTrack (2, kTrack) == rig.ImageTrack (kTrack), L"drive 2 over the universal disk");

            before = rig.Announcements().size();
            rig.Disconnect();
            rig.RunUntil ([&] () { return rig.Announcements().size() > before; }, L"an announcement after the PC disconnects");
            Assert::AreEqual (LinkProgram::kProgramVersion, (Word) (rig.machine.GetCpu()->ReadByte (0x0803) | (rig.machine.GetCpu()->ReadByte (0x0804) << 8)), L"from the program, not the disk's first stage");
        }

        Assert::IsTrue (macs[0] != macs[1], L"two Apples whose memory differs have two MAC addresses");
    }
};
