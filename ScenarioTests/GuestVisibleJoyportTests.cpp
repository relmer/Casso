#include "Pch.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
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


    //  The test program from the Joyport's own manual, driven the way its
    //  prompts ask a person to drive it: press Space, then move the stick or
    //  press fire within its wait loop. Its first two sections expect the rear
    //  Controller Select switch at Right and then Left, which pins the jack;
    //  Casso has only Center, where AN0 picks the jack, and those sections
    //  leave AN0 off, so the switches go on both jacks, as one controller
    //  drives them. The third section is the two-stick one, with the rear
    //  switch centered, and there each switch goes on ONLY the jack the
    //  program is testing: a swapped AN0 fails it. Its last section tests
    //  Apple-mode paddles, which is not the mode Casso emulates, so reaching
    //  that section is the pass.
    TEST_METHOD (TheManualsTestProgramPassesEveryAtariStickStep)
    {
        std::vector<Byte>  bytes     = GuestSession::RequireRepoImage (kManualDiskPath);
        TestMachine        machine ("Apple2e", TestMachine::Slots::DiskOnly);
        ManualProgress     progress;
        int                step      = 0;



        machine.GetJoyport()->SetAttached (true);
        GuestSession::MountAndBoot (machine, bytes);

        for (step = 0; step < kMaxManualSteps && !progress.isDone; step++)
        {
            AdvanceTheManualProgram (machine, progress);
        }

        Assert::IsFalse (progress.isFailed,   (L"the program reported NOTHING HAPPENED after: " + progress.lastPrompt).c_str());
        Assert::IsTrue  (progress.isDone,     L"the program reached its paddle section, past every Atari-stick test");
        Assert::AreEqual (kCenteredSteps, progress.centeredSteps,
            L"five switches on each jack, each closed on that jack alone, in the centered section");
    }


private:

    static constexpr const char *  kManualDiskPath = "Apple2/Demos/Joyport.do";
    static constexpr int           kMaxManualSteps = 80;
    static constexpr int           kCenteredSteps  = 10;
    static constexpr uint32_t      kStepCycles     = 400'000;
    static constexpr size_t        kJackNameOffset = 9;        // past "TILT THE "


    struct ManualProgress
    {
        bool          isCentered    = false;
        bool          isDone        = false;
        bool          isFailed      = false;
        size_t        jack          = JoyportJacks::kLeftJack;
        int           centeredSteps = 0;
        std::string   lastScreen;
        std::wstring  lastPrompt;
    };


    //  The whole screen as one line, rows run together, so a prompt that
    //  wraps at column 40 still reads as one phrase.
    static std::string ReadScreen (MachineHost & machine)
    {
        std::vector<std::string>  rows   = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), TextScreenScraper::kTextPage1);
        std::string               screen;

        for (std::string row : rows)
        {
            row.resize (TextScreenScraper::kCols40, ' ');
            screen += row;
        }

        return screen;
    }


    static bool Contains (const std::string & screen, const char * phrase)
    {
        return screen.find (phrase) != std::string::npos;
    }


    //  The switch the prompt on screen asks for, or Count for none.
    static JoystickSwitch GetAskedSwitch (const std::string & screen, ManualProgress & progress)
    {
        size_t  tilt = screen.find ("TILT THE ");

        if (tilt != std::string::npos && Contains (screen, "ATARI JOYSTICK RIGHT"))
        {
            progress.jack = (screen.compare (tilt + kJackNameOffset, 4, "LEFT") == 0) ? JoyportJacks::kLeftJack
                                                                                       : JoyportJacks::kRightJack;
            return JoystickSwitch::Right;
        }

        if (Contains (screen, "TILT THE JOYSTICK LEFT")) { return JoystickSwitch::Left; }
        if (Contains (screen, "PUSH THE FIRE BUTTON"))   { return JoystickSwitch::Fire; }
        if (Contains (screen, "PRESS THE JOYSTICK UP"))  { return JoystickSwitch::Up; }
        if (Contains (screen, "TILT THE JOYSTICK DOWN")) { return JoystickSwitch::Down; }

        return JoystickSwitch::Count;
    }


    //  One look at the screen and one response to it, then time for the
    //  program's wait loop to see the switch.
    static void AdvanceTheManualProgram (MachineHost & machine, ManualProgress & progress)
    {
        std::string     screen      = ReadScreen (machine);
        JoyportJacks    jacks;
        JoystickSwitch  asked       = JoystickSwitch::Count;
        bool            isNewPrompt = screen != progress.lastScreen;

        progress.lastScreen = screen;
        progress.lastPrompt = std::wstring (screen.begin(), screen.end());
        progress.isFailed   = Contains (screen, "NOTHING HAPPENED");
        progress.isDone     = progress.isFailed || Contains (screen, "TEST THE PADDLE READINGS");
        progress.isCentered = progress.isCentered || Contains (screen, "MIDDLE POSITION");

        if (progress.isDone)
        {
            return;
        }

        if (Contains (screen, "PRESS SPACE WHEN READY TO START"))
        {
            SetJacks (machine, jacks);
            KeystrokeInjector::InjectKey (machine, ' ');
            machine.RunCycles (kStepCycles);
            return;
        }

        asked = GetAskedSwitch (screen, progress);

        if (asked != JoystickSwitch::Count && progress.isCentered)
        {
            jacks.jack[progress.jack].set (static_cast<size_t> (asked));
            progress.centeredSteps += isNewPrompt ? 1 : 0;
        }
        else if (asked != JoystickSwitch::Count)
        {
            jacks.jack[JoyportJacks::kLeftJack].set  (static_cast<size_t> (asked));
            jacks.jack[JoyportJacks::kRightJack].set (static_cast<size_t> (asked));
        }

        SetJacks (machine, jacks);
        machine.RunCycles (kStepCycles);
    }

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
