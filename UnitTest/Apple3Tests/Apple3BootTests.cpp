#include "Pch.h"

#include "EmuTests/TestMachine.h"
#include "Machines/Apple3/Apple3System.h"
#include "Machines/Apple3/Apple3TextCells.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3BootTests
//
//  The Apple /// built from its shipped file with the fixture ROM: it
//  builds and starts in the ROM, and with an empty drive the ROM's power-on
//  checks succeed and RETRY is on the first row of the screen.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3BootTests)
{
public:

    static constexpr uint64_t  kTicksPerSecond = 2040968;
    static constexpr int       kSecondsToRetry = 20;


    //  Whether the first text row, 40 columns, starts with the text.
    static bool IsFirstRow (Apple3System & system, const std::string & text)
    {
        std::string  row;



        for (size_t column = 0; column < text.size(); column++)
        {
            row += (char) (system.GetMmu().PeekSystem ((Word) (Apple3TextCells::kPage1 + column)) & 0x7F);
        }

        return row == text;
    }


    TEST_METHOD (Machine_BuildsAndStartsInTheRom)
    {
        TestMachine  machine ("Apple3");



        Assert::IsNotNull (machine.GetApple3());
        Assert::IsTrue    (machine.GetCpu()->GetPC() >= Apple3System::kRomStart);
    }


    TEST_METHOD (EmptyDrive_RetryIsOnTheScreen)
    {
        TestMachine  machine ("Apple3");
        bool         isRetry = false;



        for (int second = 0; second < kSecondsToRetry && !isRetry; second++)
        {
            machine.GetRunner().RunCycles (kTicksPerSecond);
            isRetry = IsFirstRow (*machine.GetApple3(), "RETRY");
        }

        Assert::IsTrue (isRetry);
    }


    //  Control-Reset resets the VIAs, so the environment register reads
    //  $FF again, but only while environment bit 4 is set.
    static Byte ResetWithEnvironment (Byte environment)
    {
        TestMachine  machine ("Apple3");



        machine.GetRunner().RunCycles (kTicksPerSecond / 10);
        machine.GetCpu()->WriteByte (0xFFD3, 0xFF);
        machine.GetCpu()->WriteByte (0xFFDF, environment);
        Assert::AreEqual (environment, machine.GetApple3()->GetMmu().GetEnvironment());

        machine.SoftReset();

        return machine.GetApple3()->GetMmu().GetEnvironment();
    }


    TEST_METHOD (ControlReset_ResetsTheVias_WithBit4Set)
    {
        Assert::AreEqual ((Byte) 0xFF, ResetWithEnvironment (0x77));
    }


    TEST_METHOD (ControlReset_IsIgnored_WithBit4Clear)
    {
        Assert::AreEqual ((Byte) 0x67, ResetWithEnvironment (0x67));
    }
};