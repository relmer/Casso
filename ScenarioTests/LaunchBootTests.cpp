#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "Config/LaunchPlacement.h"
#include "Config/StockMachines.h"
#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchBootTests
//
//  An image opened on each stock model, in the temporary machine `--launch`
//  builds: LaunchPlacement places the image on the model's stock config, and
//  the machine boots it. A 140K DOS 3.3 disk, an 800K ProDOS disk and a 32 MB
//  ProDOS hard disk, on every model, as File Explorer's Open in Casso on rows
//  open them. A model with nowhere to put the image gets a refusal instead.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LaunchBootTests)
{
public:

    static constexpr uint64_t      kBootCap      = 80'000'000ULL;
    static constexpr size_t        kSector       = 256;
    static constexpr size_t        kTrackBlocks  = 8;
    static constexpr size_t        kTrackSectors = 16;
    static constexpr size_t        kBlocks140K   = 280;
    static constexpr size_t        kBlocks800K   = 1600;
    static constexpr size_t        kBlocks32M    = 65535;

    static constexpr int  kDosSector[] = { 0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15 };


    //  The 140K disk's blocks in ProDOS order, then empty blocks to `blocks`.
    static std::vector<Byte> ToProDosOrder (const std::vector<Byte> & dosOrder, size_t blocks)
    {
        std::vector<Byte>  out (blocks * 2 * kSector, 0);



        for (size_t block = 0; block < kBlocks140K; block++)
        {
            for (size_t half = 0; half < 2; half++)
            {
                size_t  track = block / kTrackBlocks;
                size_t  from  = (track * kTrackSectors + (size_t) kDosSector[(block % kTrackBlocks) * 2 + half]) * kSector;

                std::copy_n (dosOrder.begin() + (ptrdiff_t) from, kSector, out.begin() + (ptrdiff_t) ((block * 2 + half) * kSector));
            }
        }

        return out;
    }


    static bool RunUntilScreenShows (MachineHost & host, const std::string & needle, uint64_t cycleCap)
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


    //  Opens `path` on each model as --launch does, and returns a line per
    //  model with its outcome.
    static std::vector<std::string> OpenOnEachModel (FakeDiskFileIo & io, const std::string & path, const std::vector<Byte> & floppy, const char * banner)
    {
        std::vector<std::string>  outcomes;



        for (const StockMachine & model : StockMachines::GetAll())
        {
            MachineConfig   stock;
            DiskImageFacts  facts = DiskImageFacts::Read (io, path);
            LaunchPlan      plan;
            std::string     outcome;



            TestMachine::LoadConfig (model.id, stock);
            plan = LaunchPlacement::Place (facts, path, model, stock);

            if (!plan.refusal.empty())
            {
                outcomes.push_back (std::format ("{} refused", model.id));
                Logger::WriteMessage ((TextEncoding::WideToUtf8 (plan.refusal) + "\n").c_str());
                continue;
            }

            std::unique_ptr<TestMachine>  machine;

            if (!model.hasSlots)
            {
                machine = std::make_unique<TestMachine> (model.id, plan.config.ports, io);
            }
            else
            {
                std::vector<TestMachine::BlockCard>  cards;

                for (int slot : plan.addedSlots)
                {
                    for (const SlotConfig & entry : plan.config.slots)
                    {
                        //  A card on Casso's firmware stays in the config, as --launch builds it;
                        //  the others have their firmware looked up out of the fixtures.
                        if (entry.slot == slot && !entry.cassoFirmware)
                        {
                            cards.push_back (TestMachine::BlockCard { entry.slot, entry.device, entry.ports, entry.cassoFirmware });
                        }
                    }
                }

                machine = std::make_unique<TestMachine> (model.id, TestMachine::Slots::AsShipped, cards, io,
                                                         [&plan] (MachineConfig & config, MachineBuildServices &) { config = plan.config; });
            }

            if (!plan.disk1Path.empty())
            {
                AssertSucceeded (machine->GetDiskStore().MountFromBytes (6, 0, "C:\\disks\\a.dsk", DiskFormat::Dsk, floppy));
            }

            //  Power on alone, as Casso starts the machine: the ROM, or the
            //  boot slot the plan sets, starts the image.
            machine->PowerCycle();

            outcome = RunUntilScreenShows (*machine, banner, kBootCap) ? "boots" : "no banner";

            if (outcome != "boots")
            {
                std::vector<std::string>  rows;

                GuestSession::CollectRows (*machine, rows);

                for (const std::string & row : rows)
                {
                    Logger::WriteMessage ((row + "|\n").c_str());
                }
            }

            outcomes.push_back (std::format ("{} {}", model.id, outcome));
        }

        return outcomes;
    }


    static std::string Join (const std::vector<std::string> & lines)
    {
        std::string  text;



        for (const std::string & line : lines)
        {
            text += line + "\n";
        }

        return text;
    }



    TEST_METHOD (A140KDisk_BootsOnEachModel)
    {
        FakeDiskFileIo     io;
        std::vector<Byte>  disk = GuestSession::RequireDos33Master();



        io.files["C:\\disks\\a.dsk"]  = disk;
        io.stamps["C:\\disks\\a.dsk"] = FileStamp { disk.size(), 1 };

        Assert::AreEqual (std::string ("Apple2 boots\nApple2Plus boots\nApple2e boots\nApple2eEnhanced boots\nApple2c boots\nApple2cPlus boots\n"),
                          Join (OpenOnEachModel (io, "C:\\disks\\a.dsk", disk, "DOS VERSION 3.3")));
    }



    TEST_METHOD (An800KDisk_BootsOnEachModelWithAUniDiskCard)
    {
        FakeDiskFileIo     io;
        std::vector<Byte>  disk = ToProDosOrder (GuestSession::RequireProDosUsersDisk(), kBlocks800K);



        io.files["C:\\disks\\a.po"]  = disk;
        io.stamps["C:\\disks\\a.po"] = FileStamp { disk.size(), 1 };

        Assert::AreEqual (std::string ("Apple2 refused\nApple2Plus boots\nApple2e boots\nApple2eEnhanced boots\nApple2c boots\nApple2cPlus boots\n"),
                          Join (OpenOnEachModel (io, "C:\\disks\\a.po", {}, "PRODOS")));
    }



    TEST_METHOD (A32MBHardDisk_BootsOnEachModelWithSlots)
    {
        FakeDiskFileIo     io;
        std::vector<Byte>  disk = ToProDosOrder (GuestSession::RequireProDosUsersDisk(), kBlocks32M);



        io.files["C:\\disks\\a.hdv"]  = disk;
        io.stamps["C:\\disks\\a.hdv"] = FileStamp { disk.size(), 1 };

        Assert::AreEqual (std::string ("Apple2 boots\nApple2Plus boots\nApple2e boots\nApple2eEnhanced boots\nApple2c refused\nApple2cPlus refused\n"),
                          Join (OpenOnEachModel (io, "C:\\disks\\a.hdv", {}, "PRODOS")));
    }
};
