#include "Pch.h"

#include "Core/MachineConfig.h"
#include "EmuTests/TestMachine.h"
#include "Machines/Apple3/Apple3Mmu.h"
#include "Machines/Apple3/Apple3System.h"
#include "Machines/Apple3/Apple3TimeBase.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3TimingTests
//
//  A timing program in a test ROM runs nested loops of documented cycle
//  counts and times them with timer 2 of the D VIA, as a program on the
//  machine measures itself. From ROM, the measured time is the time the
//  cycle counts predict, at 2 MHz with the screen off and on and at 1 MHz.
//  From RAM, the loop averages 1.8 MHz with the screen off and 1.4 MHz with
//  it on, and 1 MHz in the 1 MHz mode.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3TimingTests)
{
public:

    //  The program's parameters and results, in RAM.
    static constexpr Word      kEnvironment  = 0x0500;
    static constexpr Word      kLoopVector   = 0x0502;
    static constexpr Word      kYCount       = 0x0504;
    static constexpr Word      kZCount       = 0x0505;
    static constexpr Word      kTimerHigh1   = 0x0510;
    static constexpr Word      kTimerLow     = 0x0511;
    static constexpr Word      kTimerHigh2   = 0x0512;
    static constexpr Word      kDone         = 0x0513;
    static constexpr Word      kRamLoop      = 0x0600;

    static constexpr Word      kRomLoop      = 0xF00F;
    static constexpr size_t    kLoopOffset   = kRomLoop - Apple3System::kRomStart;

    static constexpr Byte      kEnvBase      = Apple3Mmu::kEnvRomSelect | Apple3Mmu::kEnvPrimaryStack | Apple3Mmu::kEnvIoEnable;
    static constexpr Byte      kEnvScreen    = Apple3Mmu::kEnvScreen;
    static constexpr Byte      kEnvOneMhz    = Apple3Mmu::kEnvSlowClock;

    //  The inner loop runs DEX and BNE 256 times, 1279 cycles; each Y pass
    //  adds DEY and BNE; each Z pass adds LDY, DEC and BNE.
    static constexpr uint64_t  kCyclesPerY   = 1284;
    static constexpr uint64_t  kCyclesPerZ   = 12;

    static constexpr uint64_t  kTicksPerSecond     = 2040968;
    static constexpr double    kMasterPerTick      = 912.0 / Apple3TimeBase::kTicksPerLine;
    static constexpr double    kMasterPerViaCycle  = 14.0;

    //  Sets the environment, then jumps to the loop. The loop starts timer
    //  2 at $FFFF, runs Z passes of Y passes of 256 DEX, reads the timer's
    //  high, low and high bytes, sets the done flag and stops. Its branches
    //  are relative, so it runs at any address.
    static std::vector<Byte> MakeRom()
    {
        std::vector<Byte>  rom (Apple3Mmu::kRomBytes, 0xEA);
        std::vector<Byte>  start =
        {
            0x78,                   // SEI
            0xAD, 0x00, 0x05,       // LDA $0500
            0x8D, 0xD1, 0xFF,       // STA $FFD1   environment register
            0xA9, 0xFF,             // LDA #$FF
            0x8D, 0xD3, 0xFF,       // STA $FFD3   its direction register
            0x6C, 0x02, 0x05,       // JMP ($0502)
        };
        std::vector<Byte>  loop  = MakeLoop();



        std::copy (start.begin(), start.end(), rom.begin());
        std::copy (loop.begin(),  loop.end(),  rom.begin() + kLoopOffset);

        rom[0xFFC] = (Byte) (Apple3System::kRomStart & 0xFF);
        rom[0xFFD] = (Byte) (Apple3System::kRomStart >> 8);

        return rom;
    }


    static std::vector<Byte> MakeLoop()
    {
        return
        {
            0xA9, 0xFF,             // LDA #$FF
            0x8D, 0xD8, 0xFF,       // STA $FFD8   timer 2 low latch
            0x8D, 0xD9, 0xFF,       // STA $FFD9   timer 2 high, starts it
            0xA2, 0x00,             // LDX #0
            0xAC, 0x04, 0x05,       // L0: LDY $0504
            0xCA,                   // L1: DEX
            0xD0, 0xFD,             // BNE L1
            0x88,                   // DEY
            0xD0, 0xFA,             // BNE L1
            0xCE, 0x05, 0x05,       // DEC $0505
            0xD0, 0xF2,             // BNE L0
            0xAD, 0xD9, 0xFF,       // LDA $FFD9
            0x8D, 0x10, 0x05,       // STA $0510
            0xAD, 0xD8, 0xFF,       // LDA $FFD8
            0x8D, 0x11, 0x05,       // STA $0511
            0xAD, 0xD9, 0xFF,       // LDA $FFD9
            0x8D, 0x12, 0x05,       // STA $0512
            0xA9, 0x01,             // LDA #1
            0x8D, 0x13, 0x05,       // STA $0513
            0xB8,                   // CLV
            0x50, 0xFE,             // BVC *
        };
    }


    struct Run
    {
        uint64_t  cycles       = 0;
        uint64_t  masterClocks = 0;
        uint32_t  viaCycles    = 0;
    };


    //  Runs the program with the environment and loop counts given, from
    //  the ROM or from a copy in RAM.
    static Run RunProgram (const std::wstring & romName, Byte environment, bool isInRam, Byte yCount, Byte zCount)
    {
        fs::path       romPath  = fs::temp_directory_path() / romName;
        Run            run;
        Byte           high1    = 0;
        Byte           high2    = 0;
        Byte           low      = 0;
        Word           timer    = 0;
        uint64_t       yPasses  = (yCount == 0) ? 256 : yCount;
        uint64_t       start    = 0;
        bool           isDone   = false;



        {
            std::vector<Byte>  rom = MakeRom();
            std::ofstream      out (romPath, std::ios::binary);



            out.write (reinterpret_cast<const char *> (rom.data()), (std::streamsize) rom.size());
        }

        TestMachine    machine ("Apple3", [&] (MachineConfig & config)
        {
            config.systemRom.resolvedPath = romPath.string();
            config.systemRom.fileSize     = Apple3Mmu::kRomBytes;
        });
        EmuCpu       & cpu      = *machine.GetCpu();
        Apple3System & system   = *machine.GetApple3();
        Word           loop     = isInRam ? kRamLoop : kRomLoop;



        if (isInRam)
        {
            std::vector<Byte>  code    = MakeLoop();
            Word               address = kRamLoop;



            for (Byte b : code)
            {
                cpu.WriteByte (address++, b);
            }
        }

        cpu.WriteByte (kEnvironment,    environment);
        cpu.WriteByte (kLoopVector,     (Byte) (loop & 0xFF));
        cpu.WriteByte (kLoopVector + 1, (Byte) (loop >> 8));
        cpu.WriteByte (kYCount,         yCount);
        cpu.WriteByte (kZCount,         zCount);
        cpu.WriteByte (kDone,           0);

        start = system.GetTimeBase().GetMasterClocks();

        for (int step = 0; step < 4000 && !isDone; step++)
        {
            machine.GetRunner().RunCycles (kTicksPerSecond / 1000);
            isDone = system.GetMmu().PeekSystem (kDone) != 0;
        }

        Assert::IsTrue (isDone, L"the program ran to the end");

        high1 = system.GetMmu().PeekSystem (kTimerHigh1);
        low   = system.GetMmu().PeekSystem (kTimerLow);
        high2 = system.GetMmu().PeekSystem (kTimerHigh2);

        //  The low byte wrapped between the two high reads: a low byte near
        //  $FF was read after the wrap.
        timer = (Word) (((high1 == high2 || low < 0x80) ? high1 : high2) << 8 | low);

        run.viaCycles    = (uint32_t) (0xFFFF - timer);
        run.masterClocks = system.GetTimeBase().GetMasterClocks() - start;

        //  From the start of the timer to the low byte's read: LDX, the
        //  passes, and the LDA, STA and LDA before the read.
        run.cycles = 2 + zCount * (yPasses * kCyclesPerY + kCyclesPerZ) - 1 + 12;

        fs::remove (romPath);

        return run;
    }


    //  The VIA cycles the cycle count predicts at one or two ticks a cycle.
    static void CheckRomTiming (const std::wstring & romName, Byte environment, uint32_t ticksPerCycle)
    {
        Run     run       = RunProgram (romName, environment, false, 8, 1);
        double  predicted = (double) run.cycles * ticksPerCycle * kMasterPerTick / kMasterPerViaCycle;



        Logger::WriteMessage (std::format ("{} cycles: {} VIA cycles, {:.1f} predicted\n", run.cycles, run.viaCycles, predicted).c_str());

        Assert::AreEqual (predicted, (double) run.viaCycles, predicted * 0.01);
    }


    //  The average rate over the whole program, about 45 frames.
    static double MeasureRamMhz (const std::wstring & romName, Byte environment)
    {
        Run     run     = RunProgram (romName, environment, true, 0, 4);
        double  seconds = (double) run.masterClocks / Apple3TimeBase::kMasterClockHz;



        return (double) run.cycles / seconds / 1e6;
    }


    TEST_METHOD (RomCode_TakesThePredictedTime_At2Mhz)
    {
        CheckRomTiming (L"Apple3Timing2Mhz.rom", kEnvBase, 1);
    }


    TEST_METHOD (RomCode_IsNotSlowedByTheDisplay)
    {
        CheckRomTiming (L"Apple3Timing2MhzScreen.rom", kEnvBase | kEnvScreen, 1);
    }


    TEST_METHOD (RomCode_TakesThePredictedTime_At1Mhz)
    {
        CheckRomTiming (L"Apple3Timing1Mhz.rom", kEnvBase | kEnvScreen | kEnvOneMhz, 2);
    }


    TEST_METHOD (RamLoop_AveragesTheDocumentedRates)
    {
        double  screenOff = MeasureRamMhz (L"Apple3TimingRamOff.rom", kEnvBase);
        double  screenOn  = MeasureRamMhz (L"Apple3TimingRamOn.rom",  kEnvBase | kEnvScreen);
        double  oneMhz    = MeasureRamMhz (L"Apple3TimingRam1Mhz.rom", kEnvBase | kEnvScreen | kEnvOneMhz);
        double  slowRate  = Apple3TimeBase::kMasterClockHz / (2 * kMasterPerTick) / 1e6;



        Logger::WriteMessage (std::format ("RAM loop: {:.3f} MHz with the screen off, {:.3f} MHz with it on, {:.3f} MHz at 1 MHz\n", screenOff, screenOn, oneMhz).c_str());

        Assert::AreEqual (1.8,      screenOff, 0.05);
        Assert::AreEqual (1.4,      screenOn,  0.05);
        Assert::AreEqual (slowRate, oneMhz,    slowRate * 0.01);
    }
};
