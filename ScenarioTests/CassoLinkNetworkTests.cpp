#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/FakeNetworkCards.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/SteppedByteStream.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkAnnouncement.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkNetworkScenarioTests
//
//  The link over UDP on a //e with an original Uthernet's CS8900A or a
//  LANceGS's LAN91C96 in slot 3, each a fake with its frames to and from a
//  fake network: the universal boot disk finds the card, the network shim
//  sets it up, with an address from DHCP or the header's, and broadcasts
//  its announcement, and the program reads a disk over it. The host runs on
//  the test's thread through a SteppedByteStream.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkNetworkScenarioTests)
{
public:

    static constexpr uint64_t  kBootCycles = 30'000'000;
    static constexpr int       kSlot       = 3;
    static constexpr Byte      kTrack      = 17;

    enum class Chip { Cs8900a, Lan91c96 };

    //  A //e with no serial card and slot 3 empty for the fake, the
    //  universal disk in drive 1 and the DOS 3.3 fixture in drive 2, and
    //  the host on the fake network's PC.
    struct Rig
    {
        TestMachine                        machine;
        LinkNetworkConfig                  network;
        FakeNetworkPeer                    peer;
        std::unique_ptr<FakeNetworkCard>   card;
        std::shared_ptr<SteppedByteStream> stream;
        InMemoryCassoLinkFolder            folder;
        std::unique_ptr<CassoLinkHost>     host;
        std::vector<Byte>                  disk;

        Rig (Chip chip, bool dhcp)
            : machine ("Apple2e", [] (MachineConfig & config)
              {
                  std::erase_if (config.slots, [] (const SlotConfig & slot) { return slot.slot == kSlot || slot.device == kpszSuperSerialCardDevice; });
              }),
              peer (LinkNetworkConfig().mac, LinkNetworkConfig().port)
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;
            std::vector<Byte>     boot;
            LinkShimOptions       options;



            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            disk.assign (bytes.begin(), bytes.end());

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

            if (chip == Chip::Cs8900a)
            {
                card = std::make_unique<FakeCs8900a> (kSlot, peer);
            }
            else
            {
                card = std::make_unique<FakeLan91c96> (kSlot, peer);
            }

            machine.GetMemoryBus().AddDevice (card.get());

            network.address = dhcp ? std::array<Byte, 4> {} : FakeNetworkPeer::kAppleIp;
            options.network = network;

            AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, boot));

            GuestSession::Mount (machine, boot);
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));

            stream = std::make_shared<SteppedByteStream> (machine, peer.GetPcEnd());
            host   = std::make_unique<CassoLinkHost> (stream, folder, [] () { return std::array<Byte, 4> {}; });
        }

        ~Rig()
        {
            machine.GetMemoryBus().RemoveDevice (card.get());
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

        std::string  ScreenText()
        {
            std::string  joined;



            for (const std::string & row : TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400))
            {
                joined += row + "|";
            }

            return joined;
        }
    };

    //  The universal disk finds the card, the shim sets it up for the
    //  header's address and broadcasts it, and the program reads a track.
    static void  ReadsADisk (Chip chip, const char * name)
    {
        Rig                rig (chip, false);
        LinkAnnouncement   announced;
        std::string        screen;



        rig.peer.SetHold (true);
        rig.machine.RunCycles (kBootCycles);
        screen = rig.ScreenText();

        Assert::IsTrue (screen.find (std::format ("{}, SLOT 3, 10.0.0.2:6502", name)) != std::string::npos, TextEncoding::NarrowToWide (screen).c_str());
        Assert::IsTrue (rig.card->GetAddress() == rig.network.mac, L"the card's individual address");
        Assert::IsFalse (rig.peer.GetAnnouncements().empty(), L"an announcement");
        Assert::IsTrue (LinkAnnouncement::Parse (rig.peer.GetAnnouncements().front(), announced), L"the announcement parses");
        Assert::IsTrue (announced.udp, L"for UDP");
        Assert::AreEqual (std::string ("10.0.0.2:6502"), announced.GetAddressText());

        rig.peer.Connect();
        AssertSucceeded (rig.host->PromptReady());
        Assert::IsTrue (rig.ReadTrack (kTrack) == rig.ImageTrack (kTrack), L"drive 2's catalog over UDP");
        Assert::IsTrue (rig.ScreenText().find (std::format ("{}, slot 3", chip == Chip::Cs8900a ? "Uthernet" : "LANceGS")) != std::string::npos,
                        TextEncoding::NarrowToWide (rig.ScreenText()).c_str());
    }

    TEST_METHOD (AnUthernet_ReadsADiskOverUdp)
    {
        ReadsADisk (Chip::Cs8900a, "UTHERNET");
    }

    TEST_METHOD (ALanceGs_ReadsADiskOverUdp)
    {
        ReadsADisk (Chip::Lan91c96, "LANCEGS");
    }

    //  With no address in the header, the shim takes one from DHCP, and the
    //  announcement has it.
    TEST_METHOD (AnUthernet_TakesItsAddressFromDhcp)
    {
        Rig               rig (Chip::Cs8900a, true);
        LinkAnnouncement  announced;



        rig.machine.RunCycles (kBootCycles);

        Assert::AreEqual (1, rig.peer.GetDhcpAcks(), L"one ACK");
        Assert::IsFalse (rig.peer.GetAnnouncements().empty(), L"an announcement");
        Assert::IsTrue (LinkAnnouncement::Parse (rig.peer.GetAnnouncements().back(), announced));
        Assert::AreEqual (std::string ("10.0.0.2:6502"), announced.GetAddressText());

        AssertSucceeded (rig.host->PromptReady());
        Assert::IsTrue (rig.ReadTrack (kTrack) == rig.ImageTrack (kTrack), L"drive 2's catalog at the offered address");
    }

    //  An ARP request for the Apple's address gets a reply with its MAC
    //  address, while the program runs.
    TEST_METHOD (ALanceGs_SendsAnArpReplyForItsAddress)
    {
        Rig  rig (Chip::Lan91c96, false);



        rig.machine.RunCycles (kBootCycles);
        AssertSucceeded (rig.host->PromptReady());

        rig.peer.SendArpRequest();
        Assert::IsTrue (rig.ReadTrack (kTrack) == rig.ImageTrack (kTrack));

        Assert::AreEqual<size_t> (1, rig.peer.GetArpReplies().size(), L"one reply");
        Assert::IsTrue (std::equal (rig.network.mac.begin(), rig.network.mac.end(), rig.peer.GetArpReplies().front().begin() + 22), L"the sender's MAC address");
        Assert::IsTrue (std::equal (FakeNetworkPeer::kAppleIp.begin(), FakeNetworkPeer::kAppleIp.end(), rig.peer.GetArpReplies().front().begin() + 28), L"the sender's address");
    }
};
