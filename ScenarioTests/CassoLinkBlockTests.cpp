#include "Pch.h"

#include "EhmTestHelper.h"
#include "EmulatorLink.h"
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
#include "Devices/Disk/DiskImageStore.h"
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
            spent += host.GetRunner().RunCycles (kSlice);

            rows.clear();
            GuestSession::CollectRows (host, rows);
            onScreen = GuestSession::AnyRowContains (rows, needle);
        }

        return onScreen;
    }



    //  An emulated //c with the disk in a UniDisk 3.5 on its disk port, or
    //  with nothing on it, and the shim on port 2 with the host on the test's
    //  thread.
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



            io.files[kDiskPath]  = disk;
            io.stamps[kDiskPath] = FileStamp { disk.size(), 1 };

            diskPort[0].name = "disk";
            diskPort[0].chain.resize (1);
            diskPort[0].chain[0].device = kpszUniDisk35Device;
            diskPort[0].chain[0].image  = kDiskPath;

            Start (diskPort);

            //  The ROM boots ProDOS from the UniDisk first, as a real //c has
            //  booted before the link starts: its SmartPort firmware assigns
            //  the bus's units their numbers when it boots.
            GuestSession::StartFromPowerOn (*machine);
            Assert::IsTrue (RunUntilScreenShows (*machine, "PRODOS", kBootCap), L"ProDOS boots from the UniDisk");

            StartShim();
        }

        //  A //c with nothing on its disk port, started from power on.
        Rig()
        {
            Start (std::vector<PortConfig>());
            GuestSession::StartFromPowerOn (*machine);
            StartShim();
        }

        void  Start (const std::vector<PortConfig> & ports)
        {
            machine = std::make_unique<TestMachine> ("Apple2c", ports, io);
            machine->PowerCycle();
        }

        void  StartShim()
        {
            std::vector<Byte>  shim;
            Word               at = LinkProgram::kShimAddress;



            InProcessByteStream::CreatePair (machineEnd, pcEnd);
            AssertSucceeded (machine->GetConnectors().AttachSerialStream (kPort, machineEnd));

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



    //  A //c with no memory card, right after a floppy read, as a session
    //  lists its block devices: the list is empty, and the RAM disk firmware
    //  in slot 4 isn't called. The floppy's motor runs on after the read,
    //  and the SmartPort firmware in slot 5 loops until it stops. The RAM
    //  disk firmware's first call formats the RAM disk, and with no card the
    //  format loops on the card's registers, which read as the screen's
    //  bytes, so whether it ends depends on the cycle it starts on.
    TEST_METHOD (AfterAFloppyRead_TheBlockDevicesAreListed_AndTheRamDiskIsPassedOver)
    {
        static constexpr Byte  kSectors[] = { 0, 1, 2, 3 };
        static constexpr Word  kRamReady  = 0x077C;
        static constexpr Byte  kSetUp     = 0xA5;

        Rig                                rig;
        std::vector<Byte>                  module;
        std::vector<Byte>                  data;
        std::vector<CassoLinkBlockDevice>  devices;
        CassoLinkStatus                    status;



        AssertSucceeded (rig.machine->GetDiskStore().MountFromBytes (6, 0, "master.dsk", DiskFormat::Dsk, GuestSession::RequireDos33Master()));
        AssertSucceeded (rig.host->ReadSectors (1, 17, kSectors, data, status));
        Assert::AreEqual ((Byte) 0, status.error);

        AssertSucceeded (LinkProgram::BuildBlockModule (module));
        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (rig.host->ListBlockDevices (devices));

        Assert::AreEqual    ((size_t) 0, devices.size());
        Assert::AreNotEqual (kSetUp, rig.machine->GetCpu()->ReadByte (kRamReady), L"The RAM disk firmware was called");
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



    //  Where the Apple's cycles go while it reads blocks, by routine, for
    //  work on the read's speed. It measures rather than checks, so it runs
    //  only with CASSO_PROFILE_BLOCKS set. The profile is written to the
    //  test's output and to CassoBlockProfile.txt in the temporary folder.
    TEST_METHOD (BlockReads_AreProfiled)
    {
        static constexpr Byte      kRun   = 32;
        static constexpr uint32_t  kRuns  = 4;
        static constexpr size_t    kShown = 30;

        std::vector<Byte>                              disk;
        std::unique_ptr<Rig>                           rig;
        CassoLinkBlockDevice                           device;
        std::unordered_map<Word, uint64_t>             profile;
        std::unordered_map<std::string, Word>          programSymbols;
        std::unordered_map<std::string, Word>          moduleSymbols;
        std::map<Word, std::string>                    names;
        std::map<std::string, uint64_t>                byRoutine;
        std::vector<std::pair<uint64_t, std::string>>  ranked;
        uint64_t                                       total          = 0;
        uint32_t                                       blocks         = kRun * kRuns;
        std::string                                    report;



        if (EmulatorLink::GetSetting ("CASSO_PROFILE_BLOCKS").empty())
        {
            Logger::WriteMessage ("NOT RUN: set CASSO_PROFILE_BLOCKS to profile block reads\n");
            return;
        }

        disk   = MakeDisk();
        rig    = std::make_unique<Rig> (disk);
        device = rig->ListOne();

        AssertSucceeded (LinkProgram::GetProgramSymbols (LinkChip::Acia, programSymbols));
        AssertSucceeded (LinkProgram::GetBlockModuleSymbols (LinkChip::Acia, moduleSymbols));

        for (const auto & [name, address] : programSymbols)
        {
            if (address >= LinkProgram::kProgramAddress && address < LinkProgram::kZeroPageSave)
            {
                names[address] = name;
            }
        }

        for (const auto & [name, address] : moduleSymbols)
        {
            if (!name.starts_with ("p_") && address >= LinkProgram::kModuleAddress && address < LinkProgram::kModuleAddress + LinkProgram::kModuleLimit)
            {
                names[address] = "module " + name;
            }
        }

        rig->stream->SetProfile (&profile);

        for (uint32_t run = 0; run < kRuns; run++)
        {
            std::vector<Byte>  data;
            CassoLinkStatus    status;

            AssertSucceeded (rig->host->ReadBlocks (device, run * kRun, kRun, false, data, status));
            Assert::AreEqual (kRun, status.done);
        }

        rig->stream->SetProfile (nullptr);

        for (const auto & [pc, cycles] : profile)
        {
            std::string  routine;
            bool         named = (pc >= LinkProgram::kProgramAddress && pc < LinkProgram::kZeroPageSave)
                              || (pc >= LinkProgram::kModuleAddress && pc < LinkProgram::kModuleAddress + LinkProgram::kModuleLimit);
            auto         at    = names.upper_bound (pc);

            if (named && at != names.begin())
            {
                routine = std::prev (at)->second;
            }
            else if (pc >= LinkProgram::kShimAddress && pc < LinkProgram::kShimAddress + LinkProgram::kShimLimit)
            {
                routine = "shim";
            }
            else if (pc >= 0xC100 && pc < 0xD000)
            {
                routine = "slot firmware";
            }
            else if (pc >= 0xD000)
            {
                routine = "ROM";
            }
            else
            {
                routine = std::format ("page ${:02X}", pc >> 8);
            }

            byRoutine[routine] += cycles;
            total              += cycles;
        }

        for (const auto & [routine, cycles] : byRoutine)
        {
            ranked.emplace_back (cycles, routine);
        }

        std::sort (ranked.rbegin(), ranked.rend());

        report = std::format ("{} blocks, {} cycles, {} cycles a block\n", blocks, total, total / blocks);

        for (size_t i = 0; i < ranked.size() && i < kShown; i++)
        {
            report += std::format ("{:>9} a block  {:5.1f}%  {}\n", ranked[i].first / blocks, 100.0 * (double) ranked[i].first / (double) total, ranked[i].second);
        }

        Logger::WriteMessage (report.c_str());
        std::ofstream (std::filesystem::temp_directory_path() / "CassoBlockProfile.txt") << report;
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
