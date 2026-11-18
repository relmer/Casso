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
};