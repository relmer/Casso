#include "Pch.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleJoyportTests
//
//  The Joyport readout disk, booted and read the way a user reads it. The
//  program sets the annunciators with POKE and reads the buttons with PEEK,
//  so what it prints is Applesoft's own account of the switches: the gate for
//  "all ten switches, in both AN1 states, on both jacks".
//
//  Two patterns, each the other's complement, so every switch of both jacks is
//  seen closed once and open once.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleJoyportTests)
{
public:

    static constexpr const char *  kDiskPath = "Disks/Casso/JoyportTest.dsk";

    //  Where JoyportTest.bas prints: one row per switch, one column per jack.
    static constexpr int     kFirstSwitchRow = 4;       // VTAB 5, zero-based
    static constexpr size_t  kLeftColumn     = 9;       // HTAB 10
    static constexpr size_t  kRightColumn    = 21;      // HTAB 22
    static constexpr size_t  kWordLength     = 6;       // "CLOSED" / "OPEN  "

    //  Long enough for the program to redraw both jacks several times over.
    static constexpr uint32_t  kRedrawCycles = 3'000'000;


    //  The //e only: GuestSession's boot path is the //e's. The ][+ reads the
    //  same switches through its game port, which JoyportMachineTests covers
    //  through the bus.
    TEST_METHOD (TheReadoutDiskReadsEverySwitchOfBothJacks)
    {
        CheckTheReadoutDisk ("Apple2e");
    }


private:

    //  The order the program prints them in, top to bottom.
    static constexpr JoystickSwitch  kRowOrder[] =
    {
        JoystickSwitch::Up, JoystickSwitch::Down, JoystickSwitch::Left, JoystickSwitch::Right, JoystickSwitch::Fire,
    };


    static void CheckTheReadoutDisk (const char * machineId)
    {
        std::vector<Byte>  bytes   = GuestSession::RequireRepoImage (kDiskPath);
        TestMachine        machine (machineId, TestMachine::Slots::DiskOnly);
        JoyportJacks       first;
        JoyportJacks       second;

        //  Left: up and fire. Right: down, left and right. Neither a real stick
        //  nor the rules can close left and right together, but the Joyport
        //  reads whatever lines it is given, and the complement needs them.
        first.jack[JoyportJacks::kLeftJack].set  (static_cast<size_t> (JoystickSwitch::Up));
        first.jack[JoyportJacks::kLeftJack].set  (static_cast<size_t> (JoystickSwitch::Fire));
        first.jack[JoyportJacks::kRightJack].set (static_cast<size_t> (JoystickSwitch::Down));
        first.jack[JoyportJacks::kRightJack].set (static_cast<size_t> (JoystickSwitch::Left));
        first.jack[JoyportJacks::kRightJack].set (static_cast<size_t> (JoystickSwitch::Right));

        second.jack[JoyportJacks::kLeftJack]  = ~first.jack[JoyportJacks::kLeftJack];
        second.jack[JoyportJacks::kRightJack] = ~first.jack[JoyportJacks::kRightJack];

        machine.GetJoyport()->SetAttached (true);
        SetJacks (machine, first);
        GuestSession::MountAndBoot (machine, bytes);
        machine.RunCycles (kRedrawCycles);
        ExpectScreen (machine, first, machineId);

        SetJacks (machine, second);
        machine.RunCycles (kRedrawCycles);
        ExpectScreen (machine, second, machineId);
    }


    static void SetJacks (MachineHost & machine, const JoyportJacks & jacks)
    {
        machine.GetJoyport()->SetJackSwitches (JoyportJacks::kLeftJack,  jacks.jack[JoyportJacks::kLeftJack]);
        machine.GetJoyport()->SetJackSwitches (JoyportJacks::kRightJack, jacks.jack[JoyportJacks::kRightJack]);
    }


    static void ExpectScreen (MachineHost & machine, const JoyportJacks & jacks, const char * machineId)
    {
        std::vector<std::string>  rows      = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), TextScreenScraper::kTextPage1);
        std::wstring              machine16 = std::wstring (machineId, machineId + strlen (machineId));
        int                       checked   = 0;



        for (size_t i = 0; i < std::size (kRowOrder); i++)
        {
            const std::string  & row = rows[kFirstSwitchRow + i];

            for (size_t jack = 0; jack < JoyportJacks::kJackCount; jack++)
            {
                size_t       column   = (jack == JoyportJacks::kLeftJack) ? kLeftColumn : kRightColumn;
                bool         isClosed = jacks.jack[jack].test (static_cast<size_t> (kRowOrder[i]));
                std::string  expected = isClosed ? "CLOSED" : "OPEN  ";
                std::string  actual   = row.size() >= column + kWordLength ? row.substr (column, kWordLength) : row;

                Assert::AreEqual (expected, actual,
                    std::format (L"{}: row {} ({}), jack {}; the screen row reads '{}'", machine16, i,
                                 static_cast<int> (kRowOrder[i]), jack,
                                 std::wstring (row.begin(), row.end())).c_str());
                checked++;
            }
        }

        Assert::AreEqual (10, checked, L"five switches on each of two jacks");
    }
};
