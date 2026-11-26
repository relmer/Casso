#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "Machines/Apple2/Common/SectorInterleave.h"
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
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"

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
                size_t  offset = (size_t) (track * 16 + SectorInterleave::GetDosFileIndexForPhysicalSector (p)) * 256;

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
    //  With asAppleWin, the fake CS8900A has AppleWin's differences from the
    //  datasheet (FakeCs8900a::SetAsAppleWin).
    static void  ReadsADisk (Chip chip, const char * name, bool asAppleWin = false)
    {
        Rig                rig (chip, false);
        LinkAnnouncement   announced;
        std::string        screen;



        if (asAppleWin)
        {
            static_cast<FakeCs8900a *> (rig.card.get())->SetAsAppleWin();
        }

        rig.peer.SetHold (true);
        rig.machine.GetRunner().RunCycles (kBootCycles);
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


    //  On AppleWin's CS8900A, the universal disk took the card for an
    //  Uthernet II, and the Load program packet was dropped when a page came
    //  right behind it.
    TEST_METHOD (AnUthernetAsAppleWinHasIt_ReadsADiskOverUdp)
    {
        ReadsADisk (Chip::Cs8900a, "UTHERNET", true);
    }

    //  With no address in the header, the shim takes one from DHCP, and the
    //  announcement has it.
    TEST_METHOD (AnUthernet_TakesItsAddressFromDhcp)
    {
        TakesItsAddressFromDhcp (Chip::Cs8900a);
    }

    TEST_METHOD (ALanceGs_TakesItsAddressFromDhcp)
    {
        TakesItsAddressFromDhcp (Chip::Lan91c96);
    }

    static void  TakesItsAddressFromDhcp (Chip chip)
    {
        Rig               rig (chip, true);
        LinkAnnouncement  announced;



        rig.machine.GetRunner().RunCycles (kBootCycles);

        Assert::AreEqual (1, rig.peer.GetDhcpAcks(), L"one ACK");
        Assert::IsFalse (rig.peer.GetAnnouncements().empty(), L"an announcement");
        Assert::IsTrue (LinkAnnouncement::Parse (rig.peer.GetAnnouncements().back(), announced));
        Assert::AreEqual (std::string ("10.0.0.2:6502"), announced.GetAddressText());

        AssertSucceeded (rig.host->PromptReady());
        Assert::IsTrue (rig.ReadTrack (kTrack) == rig.ImageTrack (kTrack), L"drive 2's catalog at the offered address");
    }

    //  An 800K UniDisk 3.5 volume on a Liron card in slot 5, its free blocks
    //  random when `random` is set and its boot block a jump to slot 6, read
    //  over UDP with each block's CRC when `crc` is set and compared with the
    //  image. The cycles a block, and how many of them the block module spent
    //  sending blocks read in place in the datagram, are written to the
    //  test's output.
    static void  ReadsABlockDevice (Chip chip, bool random, bool crc)
    {
        static constexpr const char *  kDiskPath = "C:\\disks\\800k.po";
        static constexpr uint32_t      kBlocks   = 1600;
        static constexpr size_t        kBlock    = 512;
        static constexpr Byte          kRun      = 32;
        static constexpr int           kLiron    = 5;

        std::vector<Byte>                      volume ((size_t) kBlocks * kBlock, 0);
        FakeDiskFileIo                         io;
        TestMachine::BlockCard                 liron;
        std::unique_ptr<TestMachine>           machine;
        LinkNetworkConfig                      network;
        FakeNetworkPeer                        peer (network.mac, network.port);
        std::unique_ptr<FakeNetworkCard>       card;
        std::shared_ptr<SteppedByteStream>     stream;
        InMemoryCassoLinkFolder                folder;
        std::unique_ptr<CassoLinkHost>         host;
        std::vector<Byte>                      boot;
        std::vector<Byte>                      module;
        std::vector<Byte>                      read;
        std::vector<CassoLinkBlockDevice>      devices;
        std::unordered_map<Word, uint64_t>     profile;
        std::unordered_map<std::string, Word>  symbols;
        LinkShimOptions                        options;
        const CassoLinkBlockDevice           * device   = nullptr;
        uint64_t                               start    = 0;
        uint64_t                               cycles   = 0;
        uint64_t                               inPlace  = 0;
        uint32_t                               seed     = 0x2468ACE;



        AssertSucceeded (ProDosSkeleton::Write (volume, "NETBLOCKS"));

        for (size_t at = 64 * kBlock; random && at < volume.size(); at++)
        {
            seed       = seed * 1664525 + 1013904223;
            volume[at] = (Byte) (seed >> 24);
        }

        volume[0] = 0x01;                   // the boot block: JMP $C600
        volume[1] = 0x4C;
        volume[2] = 0x00;
        volume[3] = 0xC6;

        io.files[kDiskPath]  = volume;
        io.stamps[kDiskPath] = FileStamp { volume.size(), 1 };

        liron.slot   = kLiron;
        liron.device = kpszLironCardDevice;
        liron.ports.assign (1, PortConfig {});
        liron.ports[0].device = kpszUniDisk35Device;
        liron.ports[0].image  = kDiskPath;

        machine = std::make_unique<TestMachine> ("Apple2eEnhanced", TestMachine::Slots::AsShipped, std::vector<TestMachine::BlockCard> { liron }, io,
                                                 [] (MachineConfig & config, MachineBuildServices &)
                                                 {
                                                     std::erase_if (config.slots, [] (const SlotConfig & slot) { return slot.slot == kSlot || slot.slot == 7 || slot.device == kpszSuperSerialCardDevice; });
                                                 });

        card = (chip == Chip::Cs8900a) ? std::unique_ptr<FakeNetworkCard> (std::make_unique<FakeCs8900a> (kSlot, peer))
                                       : std::unique_ptr<FakeNetworkCard> (std::make_unique<FakeLan91c96> (kSlot, peer));
        machine->GetMemoryBus().AddDevice (card.get());

        network.address = FakeNetworkPeer::kAppleIp;
        options.network = network;
        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, boot));
        GuestSession::Mount (*machine, boot);

        stream = std::make_shared<SteppedByteStream> (*machine, peer.GetPcEnd());
        host   = std::make_unique<CassoLinkHost> (stream, folder, [] () { return std::array<Byte, 4> {}; });
        host->SetBlockCrc (crc);

        peer.SetHold (true);
        machine->GetRunner().RunCycles (kBootCycles);
        peer.Connect();
        AssertSucceeded (host->PromptReady());

        AssertSucceeded (LinkProgram::BuildBlockModule (module, LinkChip::Network));
        AssertSucceeded (host->EnsureModule (module));
        AssertSucceeded (host->ListBlockDevices (devices));

        for (const CassoLinkBlockDevice & listed : devices)
        {
            if (listed.blocks == kBlocks)
            {
                device = &listed;
            }
        }

        Assert::IsNotNull (device, L"the UniDisk is listed");

        stream->SetProfile (&profile);
        start = stream->GetCycles();

        for (uint32_t first = 0; first < kBlocks; first += kRun)
        {
            std::vector<Byte>  data;
            CassoLinkStatus    status;

            AssertSucceeded (host->ReadBlocks (*device, first, kRun, false, data, status));
            Assert::AreEqual (kRun, status.done, std::format (L"the run from block {}", first).c_str());
            read.insert (read.end(), data.begin(), data.end());
        }

        cycles = stream->GetCycles() - start;
        stream->SetProfile (nullptr);

        AssertSucceeded (LinkProgram::GetBlockModuleSymbols (LinkChip::Network, symbols));

        for (const auto & [pc, spent] : profile)
        {
            if (pc >= symbols["sendraw"] && pc < symbols["cpblk"])
            {
                inPlace += spent;
            }
        }

        Assert::IsTrue (read == volume, L"the volume read over UDP is the image");
        Assert::IsTrue (inPlace > 0, L"the blocks are sent from where they were read");
        Logger::WriteMessage (std::format ("{} blocks, {} cycles a block\n", kBlocks, cycles / kBlocks).c_str());

        machine->GetMemoryBus().RemoveDevice (card.get());
    }

    TEST_METHOD (AnUthernet_ReadsABlockDevice_WithNoCrc)
    {
        ReadsABlockDevice (Chip::Cs8900a, true, false);
    }

    TEST_METHOD (AnUthernet_ReadsABlockDevice_WithItsCrc)
    {
        ReadsABlockDevice (Chip::Cs8900a, true, true);
    }

    TEST_METHOD (AnUthernet_ReadsAnEmptyBlockDevice)
    {
        ReadsABlockDevice (Chip::Cs8900a, false, false);
    }

    TEST_METHOD (ALanceGs_ReadsABlockDevice_WithNoCrc)
    {
        ReadsABlockDevice (Chip::Lan91c96, true, false);
    }


    //  An ARP request for the Apple's address gets a reply with its MAC
    //  address, while the program runs.
    TEST_METHOD (ALanceGs_SendsAnArpReplyForItsAddress)
    {
        Rig  rig (Chip::Lan91c96, false);



        rig.machine.GetRunner().RunCycles (kBootCycles);
        AssertSucceeded (rig.host->PromptReady());

        rig.peer.SendArpRequest();
        Assert::IsTrue (rig.ReadTrack (kTrack) == rig.ImageTrack (kTrack));

        Assert::AreEqual<size_t> (1, rig.peer.GetArpReplies().size(), L"one reply");
        Assert::IsTrue (std::equal (rig.network.mac.begin(), rig.network.mac.end(), rig.peer.GetArpReplies().front().begin() + 22), L"the sender's MAC address");
        Assert::IsTrue (std::equal (FakeNetworkPeer::kAppleIp.begin(), FakeNetworkPeer::kAppleIp.end(), rig.peer.GetArpReplies().front().begin() + 28), L"the sender's address");
    }
};
