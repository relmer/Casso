#include "Pch.h"

#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/Floppy35/Apple35ControllerCard.h"
#include "Devices/Floppy35/LironCard.h"
#include "FakeDiskFileIo.h"
#include "TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  IdleDevicePerformanceTests
//
//  The cost of an enhanced //e's idle storage cards: the SCSI card in slot
//  7, the UniDisk 3.5 card in slot 5, the 3.5 controller card in slot 4 and
//  the ProFile card in slot 2, with no images. After the cards' firmware
//  settles, each machine runs the same guest loop, which polls the keyboard
//  as the BASIC prompt does and doesn't access the cards. Each machine and
//  the machine without cards run the same cycles in turn, and the fastest of
//  each one's runs is compared, so a run slowed by the host's load doesn't
//  count.
//
//  The UniDisk 3.5's and the 3.5 controller card's 65C02s run their firmware
//  all the time, as the real ones do, and their idle loops are skipped in
//  whole passes (DeviceCpu::SetIdleLoop). Times on a shared host vary by
//  half from run to run, so the instructions the two CPUs run are the
//  measure, and the times are logged with a bound that fails only a gross
//  regression.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (IdleDevicePerformanceTests)
{
public:

    static constexpr uint64_t  kCycles         = 4'000'000ULL;
    static constexpr uint64_t  kSettle         = 2'000'000ULL;
    static constexpr uint64_t  kFrameCycles    = 17'030ULL;
    static constexpr int       kRuns           = 7;
    static constexpr double    kMaxRatio       = 2.0;
    static constexpr uint64_t  kMaxDevicePart  = 100;
    static constexpr Word      kLoop           = 0x0300;


    //  The machine runs from power-on until the cards' firmware is idle,
    //  then from a loop at $0300: LDA $C000, BPL $0300.
    static void StartIdleLoop (TestMachine & machine)
    {
        static constexpr Byte  kProgram[] = { 0xAD, 0x00, 0xC0, 0x10, 0xFB };

        uint64_t  ran = 0;



        machine.PowerCycle();

        while (ran < kSettle)
        {
            ran += machine.RunCycles (kSettle - ran);
        }

        for (size_t i = 0; i < std::size (kProgram); i++)
        {
            machine.GetMemoryBus().WriteByte ((Word) (kLoop + i), kProgram[i]);
        }

        machine.GetCpu()->SetPC (kLoop);
    }


    static double TimeRun (TestMachine & machine)
    {
        LARGE_INTEGER  freq  = {};
        LARGE_INTEGER  start = {};
        LARGE_INTEGER  end   = {};



        QueryPerformanceFrequency (&freq);
        QueryPerformanceCounter (&start);

        RunFrames (machine, kCycles);

        QueryPerformanceCounter (&end);

        return (double) (end.QuadPart - start.QuadPart) / (double) freq.QuadPart;
    }


    //  The fastest run of each machine, the two run in turn. Returns the
    //  ratio of `machine`'s to `bare`'s.
    static double CompareRuns (TestMachine & bare, TestMachine & machine, double & outBare, double & outMachine)
    {
        outBare    = std::numeric_limits<double>::max();
        outMachine = std::numeric_limits<double>::max();

        StartIdleLoop (bare);
        StartIdleLoop (machine);

        for (int i = 0; i < kRuns; i++)
        {
            outBare    = std::min (outBare, TimeRun (bare));
            outMachine = std::min (outMachine, TimeRun (machine));
        }

        return outMachine / outBare;
    }


    //  The machine runs a frame's cycles at a time, as the shell runs it, so
    //  the cards catch up once a frame.
    static void RunFrames (TestMachine & machine, uint64_t cycles)
    {
        uint64_t  ran = 0;



        while (ran < cycles)
        {
            ran += machine.RunCycles (std::min (kFrameCycles, cycles - ran));
        }
    }


    //  The two machines' CPUs, run to the same cycle with every pass run, have
    //  the same registers and the same RAM.
    static void AssertSameState (DeviceCpu & skipped, DeviceCpu & ran, const std::function<Byte (Word)> & skippedRam, const std::function<Byte (Word)> & ranRam, Word ramSize, const wchar_t * name)
    {
        constexpr uint64_t  kPastBoth = 1000;

        uint64_t          target = std::max (skipped.GetCycles(), ran.GetCycles()) + kPastBoth;
        Cpu6502Registers  a      = {};
        Cpu6502Registers  b      = {};



        skipped.SetIdleLoop (0, 0, {});
        ran.SetIdleLoop (0, 0, {});
        skipped.RunUntil (target);
        ran.RunUntil (target);

        a = skipped.GetRegisters();
        b = ran.GetRegisters();

        Assert::AreEqual (ran.GetCycles(), skipped.GetCycles(), name);
        Assert::IsTrue (a.pc == b.pc && a.a == b.a && a.x == b.x && a.y == b.y && a.sp == b.sp && a.p == b.p, name);

        for (Word address = 0; address < ramSize; address++)
        {
            Assert::AreEqual (ranRam (address), skippedRam (address), std::format (L"{}: RAM ${:04X}", name, address).c_str());
        }
    }


    //  SKIPPED IDLE PASSES END WHERE RUN ONES DO: a machine whose card CPUs
    //  skip their idle loops and one whose CPUs run every pass, run the same
    //  Apple cycles, have the same CPU registers and RAM at the same cycle.
    TEST_METHOD (EveryCardIdle_SkippedPassesEndWhereRunPassesEnd)
    {
        constexpr uint64_t  kLongRun    = 12'000'000ULL;
        constexpr Word      kUniDiskRam = 0x800;

        FakeDiskFileIo                       skipIo;
        FakeDiskFileIo                       runIo;
        std::vector<TestMachine::BlockCard>  cards;



        cards.push_back ({ 5, kpszLironCardDevice,   std::vector<PortConfig> (1) });
        cards.push_back ({ 4, kpszApple35CardDevice, std::vector<PortConfig> (1) });

        cards[0].ports[0].device = kpszUniDisk35Device;
        cards[1].ports[0].device = kpszApple35DriveDevice;

        TestMachine  skipping ("Apple2eEnhanced", TestMachine::Slots::Empty, cards, skipIo);
        TestMachine  running  ("Apple2eEnhanced", TestMachine::Slots::Empty, cards, runIo);
        LironCard              & skipLiron = *skipping.GetRefs().lironCards[0];
        LironCard              & runLiron  = *running.GetRefs().lironCards[0];
        Apple35ControllerCard  & skipCard  = *skipping.GetRefs().apple35Cards[0];
        Apple35ControllerCard  & runCard   = *running.GetRefs().apple35Cards[0];

        runLiron.GetDrive (0).GetCpu().SetIdleLoop (0, 0, {});
        runCard.GetCardCpu().SetIdleLoop (0, 0, {});

        StartIdleLoop (skipping);
        StartIdleLoop (running);
        RunFrames (skipping, kLongRun);
        RunFrames (running, kLongRun);

        AssertSameState (skipLiron.GetDrive (0).GetCpu(), runLiron.GetDrive (0).GetCpu(),
                         [&] (Word address) { return skipLiron.GetDrive (0).Read (address); },
                         [&] (Word address) { return runLiron.GetDrive (0).Read (address); },
                         kUniDiskRam, L"UniDisk 3.5");
        AssertSameState (skipCard.GetCardCpu(), runCard.GetCardCpu(),
                         [&] (Word address) { return skipCard.GetCardMemory().GetRam()[address]; },
                         [&] (Word address) { return runCard.GetCardMemory().GetRam()[address]; },
                         (Word) Apple35CardMemory::kRamSize, L"3.5 controller card");
    }


    //  IDLE DEVICE CPUS SKIP THEIR FIRMWARE'S IDLE LOOPS: with the guest away
    //  from the cards, the UniDisk 3.5's and the 3.5 controller card's CPUs
    //  run under one instruction for each hundred of the Apple's cycles.
    TEST_METHOD (EveryCardIdle_DeviceCpusSkipTheirIdleLoops)
    {
        FakeDiskFileIo                       io;
        std::vector<TestMachine::BlockCard>  cards;
        uint64_t                             driveStart = 0;
        uint64_t                             cardStart  = 0;
        uint64_t                             drive      = 0;
        uint64_t                             card       = 0;



        cards.push_back ({ 5, kpszLironCardDevice,   std::vector<PortConfig> (1) });
        cards.push_back ({ 4, kpszApple35CardDevice, std::vector<PortConfig> (1) });

        cards[0].ports[0].device = kpszUniDisk35Device;
        cards[1].ports[0].device = kpszApple35DriveDevice;

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, cards, io);
        DeviceCpu  & driveCpu = machine.GetRefs().lironCards[0]->GetDrive (0).GetCpu();
        DeviceCpu  & cardCpu  = machine.GetRefs().apple35Cards[0]->GetCardCpu();

        StartIdleLoop (machine);

        driveStart = driveCpu.GetInstructions();
        cardStart  = cardCpu.GetInstructions();

        RunFrames (machine, kCycles);

        drive = driveCpu.GetInstructions() - driveStart;
        card  = cardCpu.GetInstructions() - cardStart;

        Logger::WriteMessage (std::format ("{} Apple cycles: UniDisk 3.5 CPU {} instructions, 3.5 controller card CPU {}\n", kCycles, drive, card).c_str());
        Assert::IsTrue (drive < kCycles / kMaxDevicePart, L"the UniDisk 3.5's CPU skips its idle loop");
        Assert::IsTrue (card < kCycles / kMaxDevicePart, L"the 3.5 controller card's CPU skips its idle loop");
    }


    TEST_METHOD (EveryCardIdle_CostIsBounded)
    {
        FakeDiskFileIo                       bareIo;
        FakeDiskFileIo                       cardsIo;
        std::vector<TestMachine::BlockCard>  cards;
        double                               bare   = 0.0;
        double                               loaded = 0.0;
        double                               ratio  = 0.0;



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

        ratio = CompareRuns (without, with, bare, loaded);

        Logger::WriteMessage (std::format ("without cards {:.3f} s, with every card {:.3f} s, ratio {:.2f}\n", bare, loaded, ratio).c_str());

        for (const TestMachine::BlockCard & card : cards)
        {
            FakeDiskFileIo  oneIo;
            TestMachine     one ("Apple2eEnhanced", TestMachine::Slots::Empty, { card }, oneIo);
            double          alone    = 0.0;
            double          oneRatio = CompareRuns (without, one, bare, alone);



            Logger::WriteMessage (std::format ("{} alone {:.3f} s, ratio {:.2f}\n", card.device, alone, oneRatio).c_str());
        }

        Assert::IsTrue (ratio < kMaxRatio, L"the idle cards take less than twice the machine's time");
    }
};
