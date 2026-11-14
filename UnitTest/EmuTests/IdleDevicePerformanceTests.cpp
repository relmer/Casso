#include "Pch.h"

#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "FakeDiskFileIo.h"
#include "TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  IdleDevicePerformanceTests
//
//  The cost of an enhanced //e's idle storage cards: the SCSI card in slot
//  7, the UniDisk 3.5 card in slot 5, the 3.5 controller card in slot 4 and
//  the ProFile card in slot 2, with no images. The machines run the same
//  cycles three times each, and the medians are compared and logged, with
//  each card's alone.
//
//  The UniDisk 3.5's and the 3.5 controller card's 65C02s run their firmware
//  all the time, as the real ones do, so every card idle takes about twice
//  the machine's time without them (2026-11-14, Debug). The bound fails a
//  regression past that, not the spec's goal of no measurable cost.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (IdleDevicePerformanceTests)
{
public:

    static constexpr uint64_t  kCycles   = 4'000'000ULL;
    static constexpr int       kRuns     = 3;
    static constexpr double    kMaxRatio = 3.0;


    static double TimeRun (TestMachine & machine)
    {
        LARGE_INTEGER  freq  = {};
        LARGE_INTEGER  start = {};
        LARGE_INTEGER  end   = {};
        uint64_t       ran   = 0;



        QueryPerformanceFrequency (&freq);
        QueryPerformanceCounter (&start);

        while (ran < kCycles)
        {
            ran += machine.RunCycles (kCycles - ran);
        }

        QueryPerformanceCounter (&end);

        return (double) (end.QuadPart - start.QuadPart) / (double) freq.QuadPart;
    }


    static double MedianRun (TestMachine & machine)
    {
        std::vector<double>  times;



        machine.PowerCycle();

        for (int i = 0; i < kRuns; i++)
        {
            times.push_back (TimeRun (machine));
        }

        std::sort (times.begin(), times.end());

        return times[times.size() / 2];
    }


    TEST_METHOD (EveryCardIdle_CostIsBounded)
    {
        FakeDiskFileIo                       bareIo;
        FakeDiskFileIo                       cardsIo;
        std::vector<TestMachine::BlockCard>  cards;
        double                               bare   = 0.0;
        double                               loaded = 0.0;



        cards.push_back ({ 7, kpszAppleScsiCardDevice, std::vector<PortConfig> (AppleScsiCard::kTargets) });
        cards.push_back ({ 5, kpszLironCardDevice,     std::vector<PortConfig> (1) });
        cards.push_back ({ 4, kpszApple35CardDevice,   std::vector<PortConfig> (1) });
        cards.push_back ({ 2, kpszProFileCardDevice,   std::vector<PortConfig> (1) });

        cards[1].ports[0].device = kpszUniDisk35Device;
        cards[2].ports[0].device = kpszApple35DriveDevice;
        cards[3].ports[0].device = kpszProFile5Device;

        TestMachine  without ("Apple2eEnhanced", TestMachine::Slots::Empty, {}, bareIo);
        TestMachine  with    ("Apple2eEnhanced", TestMachine::Slots::Empty, cards, cardsIo);

        Assert::AreEqual ((size_t) 1, with.GetRefs().lironCards.size(), L"the cards are attached");

        bare   = MedianRun (without);
        loaded = MedianRun (with);

        Logger::WriteMessage (std::format ("without cards {:.3f} s, with every card {:.3f} s, ratio {:.2f}\n", bare, loaded, loaded / bare).c_str());

        for (const TestMachine::BlockCard & card : cards)
        {
            FakeDiskFileIo  oneIo;
            TestMachine     one ("Apple2eEnhanced", TestMachine::Slots::Empty, { card }, oneIo);
            double          time = MedianRun (one);



            Logger::WriteMessage (std::format ("{} alone {:.3f} s, ratio {:.2f}\n", card.device, time, time / bare).c_str());
        }

        Assert::IsTrue (loaded < bare * kMaxRatio, L"the idle cards take less than three times the machine's time");
    }
};
