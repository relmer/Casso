#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/MachineSocketStream.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "NetworkTests/FakeHostNetwork.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkUthernetBlockTests
//
//  The block module over an Uthernet II, on an emulated enhanced //e with the
//  card in slot 3 and a UniDisk 3.5 on a Liron card in slot 5: the program
//  built for the W5100 puts each block to the card's transmit memory whole.
//  The 800K volume is read through the link in runs and compared with the
//  image, and the cycles a block are written to the test's output. The
//  volume's boot block jumps to the Disk II in slot 6, which the Apple boots
//  after slot 5, and slot 7 is emptied.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkUthernetBlockTests)
{
public:

    static constexpr const char *  kDiskPath    = "C:\\disks\\800k.po";
    static constexpr uint16_t      kPort        = 26503;
    static constexpr int           kCardSlot    = 3;
    static constexpr int           kLironSlot   = 5;
    static constexpr int           kEmptySlot   = 7;
    static constexpr size_t        kBlock       = 512;
    static constexpr uint32_t      kBlocks      = 1600;
    static constexpr Byte          kRun         = 32;
    static constexpr uint64_t      kBootCycles  = 20'000'000;
    static constexpr uint64_t      kWaitCycles  = 40'000'000;
    static constexpr Byte          kBootSlot6[] = { 0x01, 0x4C, 0x00, 0xC6 };



    //  A blank ProDOS volume, its free blocks pseudo-random bytes when
    //  `random` is set, with a boot block that jumps to slot 6.
    static std::vector<Byte>  MakeDisk (bool random)
    {
        std::vector<Byte>  disk ((size_t) kBlocks * kBlock, 0);
        uint32_t           seed = 0x1234567;



        AssertSucceeded (ProDosSkeleton::Write (disk, "UTHERBLK"));

        for (size_t at = 64 * kBlock; random && at < disk.size(); at++)
        {
            seed     = seed * 1664525 + 1013904223;
            disk[at] = (Byte) (seed >> 24);
        }

        std::copy (std::begin (kBootSlot6), std::end (kBootSlot6), disk.begin());
        return disk;
    }



    TEST_METHOD (TheUniDisk_IsReadThroughAnUthernetII)
    {
        ReadThroughTheCard (true, true);
    }



    //  An empty volume's blocks are sent as runs.
    TEST_METHOD (AnEmptyVolume_IsReadThroughAnUthernetII)
    {
        ReadThroughTheCard (false, true);
    }



    //  With no CRC, as a link over Ethernet reads by default.
    TEST_METHOD (TheUniDisk_IsReadThroughAnUthernetII_WithNoCrc)
    {
        ReadThroughTheCard (true, false);
    }



    //  The volume, its free blocks random when `random` is set, read through
    //  the link, with each block's CRC when `crc` is set, and compared with
    //  the image.
    static void  ReadThroughTheCard (bool random, bool crc)
    {
        std::vector<Byte>                  disk   = MakeDisk (random);
        FakeDiskFileIo                     io;
        FakeHostNetwork                    network;
        TestMachine::BlockCard             card;
        std::unique_ptr<TestMachine>       machine;
        std::vector<Byte>                  boot;
        std::vector<Byte>                  module;
        std::vector<Byte>                  read;
        std::vector<CassoLinkBlockDevice>  devices;
        std::shared_ptr<FakeSocketState>   listener;
        std::shared_ptr<FakeSocketState>   connection;
        InMemoryCassoLinkFolder            folder;
        std::unique_ptr<CassoLinkHost>     host;
        LinkShimOptions                    options;
        const CassoLinkBlockDevice       * device = nullptr;
        uint64_t                           spent  = 0;
        uint64_t                           start  = 0;
        uint64_t                           cycles = 0;



        io.files[kDiskPath]  = disk;
        io.stamps[kDiskPath] = FileStamp { disk.size(), 1 };

        card.slot   = kLironSlot;
        card.device = kpszLironCardDevice;
        card.ports.assign (1, PortConfig {});
        card.ports[0].device = kpszUniDisk35Device;
        card.ports[0].image  = kDiskPath;

        machine = std::make_unique<TestMachine> ("Apple2eEnhanced", TestMachine::Slots::AsShipped, std::vector<TestMachine::BlockCard> { card }, io,
                                                 [&network] (MachineConfig & config, MachineBuildServices & services)
                                                 {
                                                     for (SlotConfig & slot : config.slots)
                                                     {
                                                         if (slot.device == "uthernet2")
                                                         {
                                                             slot.enabled = true;
                                                         }
                                                     }

                                                     std::erase_if (config.slots, [] (const SlotConfig & slot) { return slot.slot == kEmptySlot; });
                                                     services.hostNetwork = &network;
                                                 });

        Assert::IsNotNull (machine->GetRefs().uthernet, L"the card must be installed");

        options.chip            = LinkChip::W5100;
        options.slot            = kCardSlot;
        options.network.address = { 127, 0, 0, 1 };
        options.network.port    = kPort;
        AssertSucceeded (LinkProgram::BuildBootDisk (options, boot));

        GuestSession::Mount (*machine, boot);
        machine->RunCycles (kBootCycles);

        for (listener = network.FindBound (HostSocketKind::Tcp, kPort);
             !(listener && listener->listening && !listener->destroyed) && spent < kWaitCycles;
             listener = network.FindBound (HostSocketKind::Tcp, kPort))
        {
            machine->RunCycles (MachineSocketStream::kStepCycles);
            spent += MachineSocketStream::kStepCycles;
        }

        Assert::IsTrue (listener && listener->listening, L"the card listens");

        connection = network.OfferConnection (listener, HostEndpoint { FakeHostNetwork::kLoopback, 50000 });
        host       = std::make_unique<CassoLinkHost> (std::make_shared<MachineSocketStream> (*machine, connection), folder, [] () { return std::array<Byte, 4> {}; });

        host->SetBlockCrc (crc);

        AssertSucceeded (LinkProgram::BuildBlockModule (module, LinkChip::W5100));
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

        start = machine->GetCpu()->GetTotalCycles();

        for (uint32_t first = 0; first < kBlocks; first += kRun)
        {
            std::vector<Byte>  data;
            CassoLinkStatus    status;

            AssertSucceeded (host->ReadBlocks (*device, first, kRun, false, data, status));
            Assert::AreEqual (kRun, status.done, std::format (L"the run from block {}", first).c_str());
            read.insert (read.end(), data.begin(), data.end());
        }

        cycles = machine->GetCpu()->GetTotalCycles() - start;

        Assert::IsTrue (read == disk, L"the volume read through the link is the image");
        Logger::WriteMessage (std::format ("{} blocks, {} cycles a block\n", kBlocks, cycles / kBlocks).c_str());
    }
};
