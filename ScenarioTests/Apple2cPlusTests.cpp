#include "Pch.h"

#include "GuestSession.h"
#include "TestMachine.h"
#include "Machines/Apple2/Apple2cPlus/Apple2cPlusAccelerator.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cPlusTests
//
//  The //c Plus's accelerator in a whole machine: the same delay loop takes
//  about a quarter of the //c's machine cycles.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple2cPlusTests)
{
public:

    //  SEI; turn the slowdowns off with $80 at $C05E; 16 times 256 DEX
    //  loops; then JMP to itself at $0310.
    static constexpr Word  kProgram = 0x0300;
    static constexpr Word  kDone    = 0x0310;

    static uint64_t TimeTheLoop (const char * machineId)
    {
        constexpr uint64_t  kSettle = 2'000'000;
        constexpr uint64_t  kCap    = 10'000'000;

        const Byte    program[] = { 0x78, 0xA9, 0x80, 0x8D, 0x5E, 0xC0, 0xA0, 0x10, 0xA2, 0x00,
                                    0xCA, 0xD0, 0xFD, 0x88, 0xD0, 0xF8, 0x4C, 0x10, 0x03 };
        TestMachine   machine (machineId);
        uint64_t      spent   = 0;



        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);
        machine.RunCycles (kSettle);

        for (size_t i = 0; i < std::size (program); i++)
        {
            machine.GetMemoryBus().WriteByte ((Word) (kProgram + i), program[i]);
        }

        machine.GetCpu()->SetPC (kProgram);

        while (machine.GetCpu()->GetPC() != kDone && spent < kCap)
        {
            spent += machine.StepOne();
        }

        Assert::IsTrue (spent < kCap, L"the loop finishes");

        return spent;
    }


    TEST_METHOD (DelayLoop_RunsFourTimesAsFast)
    {
        uint64_t  apple2c     = TimeTheLoop ("Apple2c");
        uint64_t  apple2cPlus = TimeTheLoop ("Apple2cPlus");
        double    ratio       = (double) apple2c / (double) apple2cPlus;



        Assert::IsTrue (ratio > 3.8 && ratio < 4.0, std::format (L"//c {} cycles, //c Plus {} cycles", apple2c, apple2cPlus).c_str());
    }
};
