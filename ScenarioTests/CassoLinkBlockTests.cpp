#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CassoLinkBlockTransfer.h"
#include "CassoLink/SteppedByteStream.h"
#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockScenarioTests
//
//  The block module on an emulated //c with a UniDisk 3.5 on its disk port,
//  through the //c's ROM 4 SmartPort firmware, over the link on port 2: the
//  unit listed with its size and volume, blocks read, and blocks written,
//  verified on the Apple and read back. The disk is the ProDOS Users Disk's
//  blocks padded to 800K, with a pattern in the blocks past them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkBlockScenarioTests)
{
public:

    static constexpr const char *  kDiskPath      = "C:\\disks\\800k.po";
    static constexpr DWORD         kLinkBaud      = 115200;
    static constexpr int           kPort          = 2;
    static constexpr size_t        kSector        = 256;
    static constexpr size_t        kBlock         = 512;
    static constexpr size_t        kBlocks800K    = 1600;
    static constexpr size_t        kBlocks140K    = 280;
    static constexpr size_t        kTrackBlocks   = 8;
    static constexpr int           kDosSector[]   = { 0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15 };
    static constexpr Byte          kSmartPortSlot = 5;
    static constexpr Byte          kKind35Disk    = 1;
    static constexpr uint64_t      kBootCap       = 80'000'000ULL;



    //  The Users Disk's blocks in ProDOS order, then a pattern to 800K.
    static std::vector<Byte>  MakeDisk()
    {
        std::vector<Byte>  dosOrder = GuestSession::RequireProDosUsersDisk();
        std::vector<Byte>  out (kBlocks800K * kBlock, 0);



        for (size_t block = 0; block < kBlocks140K; block++)
        {
            size_t  track = block / kTrackBlocks;

            for (size_t half = 0; half < 2; half++)
            {
                size_t  from = (track * 16 + (size_t) kDosSector[(block % kTrackBlocks) * 2 + half]) * kSector;

                std::copy_n (dosOrder.begin() + (ptrdiff_t) from, kSector, out.begin() + (ptrdiff_t) ((block * 2 + half) * kSector));
            }
        }

        for (size_t at = kBlocks140K * kBlock; at < out.size(); at++)
        {
            out[at] = (Byte) (at * 7 + at / kBlock);
        }

        return out;
    }



    static bool  RunUntilScreenShows (MachineHost & host, const std::string & needle, uint64_t cycleCap)
    {
        constexpr uint64_t  kSlice = 100'000ULL;

        std::vector<std::string>  rows;
        uint64_t                  spent    = 0;
        bool                      onScreen = false;



        while (!onScreen && spent < cycleCap)
        {
            spent += host.RunCycles (kSlice);

            rows.clear();
            GuestSession::CollectRows (host, rows);
            onScreen = GuestSession::AnyRowContains (rows, needle);
        }

        return onScreen;
    }



    //  An emulated //c with the disk in a UniDisk 3.5 on its disk port, and
    //  the shim on port 2 with the host on the test's thread.
    struct Rig
    {
        FakeDiskFileIo                        io;
        std::unique_ptr<TestMachine>          machine;
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::shared_ptr<SteppedByteStream>    stream;
        InMemoryCassoLinkFolder               folder;
        std::unique_ptr<CassoLinkHost>        host;

        explicit Rig (const std::vector<Byte> & disk)
        {
            std::vector<PortConfig>  diskPort (1);
            std::vector<Byte>        shim;
            Word                     at = LinkProgram::kShimAddress;



            io.files[kDiskPath]  = disk;
            io.stamps[kDiskPath] = FileStamp { disk.size(), 1 };

            diskPort[0].name = "disk";
            diskPort[0].chain.resize (1);
            diskPort[0].chain[0].device = kpszUniDisk35Device;
            diskPort[0].chain[0].image  = kDiskPath;

            machine = std::make_unique<TestMachine> ("Apple2c", diskPort, io);
            machine->SetVideoTiming (std::make_unique<VideoTiming>());
            machine->GetCpu()->SetVideoTiming (machine->GetVideoTiming());
            machine->PowerCycle();

            //  The ROM boots ProDOS from the UniDisk first, as a real //c has
            //  booted before the link starts: its SmartPort firmware assigns
            //  the bus's units their numbers when it boots.
            GuestSession::StartFromPowerOn (*machine);
            Assert::IsTrue (RunUntilScreenShows (*machine, "PRODOS", kBootCap), L"ProDOS boots from the UniDisk");

            InProcessByteStream::CreatePair (machineEnd, pcEnd);
            AssertSucceeded (machine->AttachSerialStream (kPort, machineEnd));

            stream = std::make_shared<SteppedByteStream> (*machine, pcEnd);
            host   = std::make_unique<CassoLinkHost> (stream, folder, [] () { return std::array<Byte, 4> {}; });

            AssertSucceeded (LinkProgram::BuildShim (kLinkBaud, shim));

            for (Byte b : shim)
            {
                machine->GetCpu()->WriteByte (at++, b);
            }

            machine->GetCpu()->SetPC (LinkProgram::kShimAddress);
        }

        CassoLinkBlockDevice  ListOne()
        {
            std::vector<Byte>                  module;
            std::vector<CassoLinkBlockDevice>  devices;



            AssertSucceeded (LinkProgram::BuildBlockModule (module));
            AssertSucceeded (host->EnsureModule (module));
            AssertSucceeded (host->ListBlockDevices (devices));
            Assert::AreEqual ((size_t) 1, devices.size(), L"The UniDisk is the one block device");

            return devices[0];
        }
    };



    //  The volume name block 2's header has.
    static std::string  GetVolumeName (const std::vector<Byte> & disk)
    {
        const Byte  * header = disk.data() + 2 * kBlock + 4;



        return std::string ((const char *) header + 1, header[0] & 0x0F);
    }



    TEST_METHOD (TheUniDisk_IsListedWithItsSizeAndVolume)
    {
        std::vector<Byte>      disk   = MakeDisk();
        Rig                    rig (disk);
        CassoLinkBlockDevice   device = rig.ListOne();



        Assert::AreEqual (kSmartPortSlot, device.slot);
        Assert::AreEqual ((Byte) 1, device.unit);
        Assert::IsTrue   (device.isSmartPort);
        Assert::AreEqual ((uint32_t) kBlocks800K, device.blocks);
        Assert::AreEqual (kKind35Disk, device.kind);
        Assert::AreEqual (GetVolumeName (disk), device.volume);
    }



    TEST_METHOD (Blocks_AreReadAsTheDiskHasThem)
    {
        static constexpr uint32_t  kFirst = 275;     // across the Users Disk's end into the pattern
        static constexpr Byte      kCount = 10;

        std::vector<Byte>      disk   = MakeDisk();
        Rig                    rig (disk);
        CassoLinkBlockDevice   device = rig.ListOne();
        std::vector<Byte>      data;
        CassoLinkStatus        status;



        AssertSucceeded (rig.host->ReadBlocks (device, kFirst, kCount, true, data, status));

        Assert::AreEqual ((Byte) 0, status.error, TextEncoding::NarrowToWide (CassoLinkHost::DescribeBlockError (status.error, kFirst + status.done)).c_str());
        Assert::AreEqual (kCount, status.done);
        Assert::IsTrue   (std::equal (data.begin(), data.end(), disk.begin() + (ptrdiff_t) (kFirst * kBlock)) && data.size() == kCount * kBlock, L"The blocks are the disk's");
    }



    TEST_METHOD (Blocks_AreWrittenVerified_AndReadBack)
    {
        static constexpr uint32_t  kFirst = 1500;
        static constexpr size_t    kCount = 4;

        std::vector<Byte>      disk   = MakeDisk();
        Rig                    rig (disk);
        CassoLinkBlockDevice   device = rig.ListOne();
        std::vector<Byte>      written (kCount * kBlock);
        std::vector<Byte>      read;
        CassoLinkStatus        status;



        for (size_t at = 0; at < written.size(); at++)
        {
            written[at] = (Byte) (0xA5 ^ at ^ (at >> 9));
        }

        AssertSucceeded (rig.host->WriteBlocks (device, kFirst, written, true, status));
        Assert::AreEqual ((Byte) 0, status.error, TextEncoding::NarrowToWide (CassoLinkHost::DescribeBlockError (status.error, kFirst + status.done)).c_str());
        Assert::AreEqual ((Byte) kCount, status.done);

        AssertSucceeded (rig.host->ReadBlocks (device, kFirst, (Byte) kCount, false, read, status));
        Assert::IsTrue  (read == written, L"The blocks read back are those written");
    }



    //  A whole-disk read stopped at block 1,000 saves the partial image and
    //  its record; the next read of the same disk continues after the
    //  record's last block, and the image is the disk.
    TEST_METHOD (AWholeDiskRead_StoppedPartway_ContinuesToTheWholeDisk)
    {
        static constexpr uint32_t  kStopAt = 1000;
        static constexpr const char *  kImage = "C:\\Images\\UniDisk.po";

        std::vector<Byte>               disk    = MakeDisk();
        Rig                             rig (disk);
        CassoLinkBlockDevice            device  = rig.ListOne();
        CassoLinkBlockTransfer::Result  result;
        uint32_t                        done    = 0;
        uint32_t                        next    = 0;



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED),
                          CassoLinkBlockTransfer::ReadDevice (*rig.host, rig.io, device, kImage, true, true,
                                                              [&done] (uint32_t blocks, uint32_t) { done = blocks; }, [&done] () { return done >= kStopAt; }, result));

        Assert::IsTrue (CassoLinkBlockTransfer::FindResume (*rig.host, rig.io, device, kImage, true, next), L"The offer to continue");
        Assert::IsTrue (next >= kStopAt - CassoLinkResume::kEveryBlocks && next <= done, std::format (L"From block {}, {} done", next, done).c_str());

        AssertSucceeded (CassoLinkBlockTransfer::ReadDevice (*rig.host, rig.io, device, kImage, true, true, nullptr, nullptr, result));

        Assert::AreEqual (next, result.firstBlock);
        Assert::IsTrue   (rig.io.files[kImage] == disk, L"The image is the disk");
    }
};
