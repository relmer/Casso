#include "Pch.h"

#include "TestMachine.h"
#include "HResultAssert.h"
#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/Disk/DiskImageStore.h"
#include "FakeDiskFileIo.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRunCostTests
//
//  The host time per emulated cycle of a //e and a //c whose boot ROM reads a
//  pattern disk that never boots, so the drive spins and the controller runs
//  on every cycle, and of an enhanced //e with every storage card. The fastest
//  of several runs is logged in nanoseconds per cycle. These are the numbers
//  to compare before and after a change to the run loop, the CPU or the disk
//  controller, with the test host pinned to one core:
//
//      cmd /c "start /wait /b /high /affinity 10 "" vstest.console.exe ...
//          /TestCaseFilter:FullyQualifiedName~MachineRunCostTests"
//
//  Times on a shared host vary too much for a bound, so the test checks only
//  that each machine ran.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineRunCostTests)
{
public:

    static constexpr uint64_t  kSettleCycles = 1'000'000ULL;
    static constexpr uint64_t  kRunCycles    = 500'000ULL;
    static constexpr int       kRuns         = 7;
    static constexpr int       kDiskSlot     = 6;
    static constexpr int       kDiskDrive    = 0;
    static constexpr size_t    kPatternStep  = 7;
    static constexpr double    kNsPerSecond  = 1e9;


    TEST_METHOD (LogsTheCostOfACycleWithTheDiskSpinning)
    {
        for (const char * id : { "Apple2e", "Apple2c" })
        {
            TestMachine  machine (id);



            MountPatternDisk (machine);
            LogCost (machine, id);
        }
    }


    TEST_METHOD (LogsTheCostOfACycleWithEveryStorageCard)
    {
        FakeDiskFileIo                       io;
        std::vector<TestMachine::BlockCard>  cards;



        cards.push_back ({ 7, kpszAppleScsiCardDevice, std::vector<PortConfig> (AppleScsiCard::kTargets) });
        cards.push_back ({ 5, kpszLironCardDevice,     std::vector<PortConfig> (1) });
        cards.push_back ({ 4, kpszApple35CardDevice,   std::vector<PortConfig> (1) });
        cards.push_back ({ 2, kpszProFileCardDevice,   std::vector<PortConfig> (1) });

        cards[1].ports[0].device = kpszUniDisk35Device;
        cards[2].ports[0].device = kpszApple35DriveDevice;
        cards[3].ports[0].device = kpszProFile5Device;

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, cards, io);

        machine.PowerCycle();
        LogCost (machine, "every storage card");
    }


    static void LogCost (TestMachine & machine, const char * label)
    {
        using Clock = std::chrono::steady_clock;

        double    fastest = std::numeric_limits<double>::max();
        uint64_t  ran     = 0;
        int       run     = 0;



        machine.RunCycles (kSettleCycles);

        for (run = 0; run < kRuns; run++)
        {
            Clock::time_point  start   = Clock::now();
            uint64_t           cycles  = machine.RunCycles (kRunCycles);
            double             seconds = std::chrono::duration<double> (Clock::now() - start).count();



            ran     += cycles;
            fastest  = std::min (fastest, seconds * kNsPerSecond / (double) cycles);
        }

        Logger::WriteMessage (std::format ("{}: {:.2f} ns per cycle\n", label, fastest).c_str());

        Assert::IsTrue (ran > 0, L"the machine ran");
    }


    static void MountPatternDisk (TestMachine & machine)
    {
        std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);
        HRESULT            hr = S_OK;
        size_t             i  = 0;



        machine.PowerCycle();

        for (i = 0; i < raw.size(); i++)
        {
            raw[i] = static_cast<Byte> (i * kPatternStep);
        }

        hr = machine.GetDiskStore().MountFromBytes (kDiskSlot, kDiskDrive, "pattern.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes");

        machine.GetRefs().diskController->SetExternalDisk (kDiskDrive, machine.GetDiskStore().GetImage (kDiskSlot, kDiskDrive));
    }
};
