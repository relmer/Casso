#include "Pch.h"

#include "EmuTests/GuestSession.h"
#include "EmuTests/TestMachine.h"
#include "HResultAssert.h"
#include "Machines/Apple3/Apple3System.h"
#include "Machines/Apple3/Apple3TextCells.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3SosTests
//
//  The Apple /// booting real disks headless: the ROM's screen with an
//  empty drive, SOS and Business BASIC to their prompt on 128K and 256K,
//  and BASIC evaluating a typed expression. The disks are fetched by
//  scripts/FetchStockDisks.ps1; a missing disk or ROM fails the test.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3SosTests)
{
public:

    static constexpr uint64_t      kTicksPerSecond   = 2040968;
    static constexpr int           kSecondsToPrompt  = 40;
    static constexpr int           kDiskSlot         = 6;
    static constexpr int           kDrive1           = 0;
    static constexpr const char *  kpszBusinessBasic = "Disks/Apple/apple3-business-basic.dsk";


    //  The screen as text: 80 columns when the 80-column switch is set,
    //  40 otherwise, one line per row, with control codes as spaces.
    static std::string GetScreenText (Apple3System & system)
    {
        bool         is80    = system.GetVideo().Is80Columns();
        int          columns = is80 ? Apple3TextCells::kColumns * 2 : Apple3TextCells::kColumns;
        std::string  text;



        for (int row = 0; row < Apple3TextCells::kRows; row++)
        {
            for (int column = 0; column < columns; column++)
            {
                Word  page = (!is80 || column % 2 == 0) ? Apple3TextCells::kPage1 : Apple3TextCells::kPage2;
                Word  cell = (Word) (Apple3TextCells::GetRowAddress (row, page) + (is80 ? column / 2 : column));
                Byte  c    = (Byte) (system.GetMmu().PeekSystem (cell) & 0x7F);



                text += (c >= 0x20 && c < 0x7F) ? (char) c : ' ';
            }

            text += '\n';
        }

        return text;
    }


    //  Runs a second at a time until the screen contains the text.
    static bool RunUntilScreenHas (TestMachine & machine, const std::string & text, int seconds)
    {
        bool  isFound = false;



        for (int second = 0; second < seconds && !isFound; second++)
        {
            machine.GetRunner().RunCycles (kTicksPerSecond);
            isFound = GetScreenText (*machine.GetApple3()).find (text) != std::string::npos;
        }

        return isFound;
    }


    //  Types each character once the program has taken the one before.
    static void Type (TestMachine & machine, const std::string & text)
    {
        Apple3Keyboard  & keyboard = machine.GetApple3()->GetKeyboard();



        for (char c : text)
        {
            for (int wait = 0; wait < 100 && !keyboard.IsStrobeClear(); wait++)
            {
                machine.GetRunner().RunCycles (kTicksPerSecond / 100);
            }

            keyboard.PressKey ((Byte) c);
            machine.GetRunner().RunCycles (kTicksPerSecond / 50);
        }
    }


    static void MountBusinessBasic (TestMachine & machine)
    {
        std::vector<Byte>  bytes = GuestSession::RequireRepoImage (kpszBusinessBasic);
        HRESULT            hr    = machine.GetDiskStore().MountFromBytes (kDiskSlot, kDrive1, "apple3-business-basic.dsk", DiskFormat::Dsk, bytes);



        AssertSucceeded (hr, L"the Business BASIC disk mounts");
        machine.GetStorage().ConnectDiskToController (kDiskSlot, kDrive1);
    }


    static void CheckBusinessBasicPrompt (uint32_t memoryK)
    {
        TestMachine  machine ("Apple3", [memoryK] (MachineConfig & config) { config.memoryK = memoryK; });



        MountBusinessBasic (machine);

        Assert::IsTrue (RunUntilScreenHas (machine, "Apple Business BASIC v1.23", kSecondsToPrompt),
                        L"SOS loads Business BASIC, and its banner is on the screen");
        Assert::IsTrue (RunUntilScreenHas (machine, "BLOCKS FREE", kSecondsToPrompt),
                        L"and the disk's startup program runs to its end");
    }


    TEST_METHOD (NoDisk_RetryIsOnTheScreen)
    {
        TestMachine  machine ("Apple3");



        Assert::IsTrue (RunUntilScreenHas (machine, "RETRY", kSecondsToPrompt));
    }


    TEST_METHOD (BusinessBasic_RunsTheStartupProgram_128K)
    {
        CheckBusinessBasicPrompt (128);
    }


    TEST_METHOD (BusinessBasic_RunsTheStartupProgram_256K)
    {
        CheckBusinessBasicPrompt (256);
    }


    TEST_METHOD (BusinessBasic_PrintsSixTimesSeven)
    {
        TestMachine  machine ("Apple3");



        MountBusinessBasic (machine);
        Assert::IsTrue (RunUntilScreenHas (machine, "BLOCKS FREE", kSecondsToPrompt));

        Type (machine, "PRINT 6*7\r");

        Assert::IsTrue (RunUntilScreenHas (machine, "\n42 ", 2), L"BASIC prints 42 on the next line");
    }
};
