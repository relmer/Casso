#include "Pch.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
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


    //  The Joyport test program on Joyport.do, driven the way its prompts ask
    //  a person to drive it: every test from the menu, pressing Space at each
    //  setup screen and moving the stick or pressing fire within each step's
    //  wait. Its first two tests expect the rear Controller Select switch at
    //  Right and then Left, which pins the jack; Casso has only Center, where
    //  AN0 picks the jack, and those tests leave AN0 off, so the switches go
    //  on both jacks, as one controller drives them. The third is the
    //  two-stick test, with the rear switch in the middle, and there each
    //  switch goes on ONLY the jack the program is testing: a swapped AN0
    //  fails it. The paddle test comes last and needs Apple mode, so reaching
    //  it is the pass.
    TEST_METHOD (TheTestProgramPassesEveryAtariJoystickStep)
    {
        TestMachine     machine ("Apple2e", TestMachine::Slots::DiskOnly);
        ProgramRun      run;
        int             step    = 0;



        BootTheTestProgram (machine, true);
        KeystrokeInjector::InjectKey (machine, kEveryTestKey);

        for (step = 0; step < kMaxSteps && !run.isFailed && !Contains (run.screen, "GAME PADDLE TEST"); step++)
        {
            AdvanceTheJoystickTests (machine, run);
        }

        Assert::IsFalse  (run.isFailed, (L"the program reported NOTHING HAPPENED at: " + Widen (run.screen)).c_str());
        Assert::IsTrue   (Contains (run.screen, "GAME PADDLE TEST"), L"the program reached its paddle test, past every Atari joystick step");
        Assert::AreEqual (kCenteredSteps, run.centeredSteps,
            L"five switches on each jack, each closed on that jack alone, with the rear switch in the middle");
    }


    //  The paddle test, chosen straight from the menu. It is the Joyport's
    //  Apple mode, which passes the paddles through; Casso does not emulate
    //  Apple mode, but with the Joyport detached the paddles reach the game
    //  port directly, which is what Apple mode amounts to. Every paddle is
    //  turned end to end at each step and both buttons are held when it asks.
    TEST_METHOD (ThePaddleTestPassesFromTheMenu)
    {
        TestMachine     machine ("Apple2e", TestMachine::Slots::DiskOnly);
        ProgramRun      run;
        int             step    = 0;



        BootTheTestProgram (machine, false);
        KeystrokeInjector::InjectKey (machine, kPaddleTestKey);

        for (step = 0; step < kMaxSteps && !run.isFailed && !Contains (run.screen, "ALL DONE"); step++)
        {
            AdvanceThePaddleTest (machine, run, step, kPaddleCount);
        }

        Assert::IsFalse (run.isFailed, (L"the program reported NOTHING HAPPENED at: " + Widen (run.screen)).c_str());
        Assert::IsTrue  (Contains (run.screen, "ALL DONE"), L"every paddle and button step passed");
        Assert::AreEqual (kPaddleSteps, run.paddleSteps, L"four paddles in the middle position, two in each of the others");
    }


    //  One controller drives only paddles 0 and 1, so paddles 2 and 3 are
    //  skipped with S. The test still finishes, and its last screen says how
    //  many steps were skipped rather than that every step passed.
    TEST_METHOD (TheSkipKeyPassesOverPaddlesThatAreNotThere)
    {
        TestMachine     machine ("Apple2e", TestMachine::Slots::DiskOnly);
        ProgramRun      run;
        int             step    = 0;



        BootTheTestProgram (machine, false);
        KeystrokeInjector::InjectKey (machine, kPaddleTestKey);

        for (step = 0; step < kMaxSteps && !run.isFailed && !Contains (run.screen, "PRESS SPACE FOR THE MENU"); step++)
        {
            AdvanceThePaddleTest (machine, run, step, kOneControllersPaddles);
        }

        Assert::IsFalse  (run.isFailed, (L"the program reported NOTHING HAPPENED at: " + Widen (run.screen)).c_str());
        Assert::IsTrue   (Contains (run.screen, "DONE, WITH 2 STEPS SKIPPED."), (L"paddles 2 and 3 skipped: " + Widen (run.screen)).c_str());
        Assert::IsFalse  (Contains (run.screen, "ALL DONE"),                    L"and not reported as every step passing");
        Assert::AreEqual (kPaddleSteps, run.paddleSteps,                        L"every paddle was still asked for");
    }


    //  A step that times out offers to try that step again, and Space does:
    //  the same prompt comes back rather than the whole test restarting.
    TEST_METHOD (AFailedStepIsTriedAgainNotRestarted)
    {
        TestMachine     machine ("Apple2e", TestMachine::Slots::DiskOnly);
        ProgramRun      run;
        JoyportJacks    jacks;
        int             step    = 0;



        BootTheTestProgram (machine, true);
        KeystrokeInjector::InjectKey (machine, '1');
        machine.RunCycles (kStepCycles);
        KeystrokeInjector::InjectKey (machine, ' ');

        for (step = 0; step < kMaxSteps && !Contains (ReadScreen (machine), "NOTHING HAPPENED"); step++)
        {
            machine.RunCycles (kStepCycles);
        }

        Assert::IsTrue (Contains (ReadScreen (machine), "RIGHT JOYSTICK: PUSH IT RIGHT"), L"the step that timed out");
        Assert::IsTrue (Contains (ReadScreen (machine), "NOTHING HAPPENED"),              L"timed out with nothing pressed");

        KeystrokeInjector::InjectKey (machine, ' ');
        machine.RunCycles (kStepCycles);
        run.screen = ReadScreen (machine);

        Assert::IsTrue  (Contains (run.screen, "RIGHT JOYSTICK: PUSH IT RIGHT"), L"Space tries the same step again");
        Assert::IsFalse (Contains (run.screen, "NOTHING HAPPENED"),              L"with the message cleared");

        jacks.jack[JoyportJacks::kLeftJack].set (static_cast<size_t> (JoystickSwitch::Right));
        SetJacks (machine, jacks);
        machine.RunCycles (kStepCycles);

        Assert::IsTrue (Contains (ReadScreen (machine), "RIGHT JOYSTICK: PUSH IT LEFT"), L"and passing it moves on to the next step");
    }


private:

    static constexpr const char *  kProgramDiskPath       = "Apple2/Demos/Joyport.do";
    static constexpr int           kMaxSteps              = 200;
    static constexpr int           kCenteredSteps         = 10;
    static constexpr int           kPaddleSteps           = 8;
    static constexpr uint32_t      kStepCycles            = 400'000;
    static constexpr char          kEveryTestKey          = '5';
    static constexpr char          kPaddleTestKey         = '4';
    static constexpr Byte          kPaddleLeft            = 0;
    static constexpr Byte          kPaddleRight           = 255;
    static constexpr int           kPaddleCount           = 4;
    static constexpr int           kOneControllersPaddles = 2;
    static constexpr Byte          kPaddleCenter          = 127;
    static constexpr size_t        kPaddleDigit           = 12;       // past "TURN PADDLE "


    struct ProgramRun
    {
        bool          isCentered    = false;
        bool          isFailed      = false;
        int           centeredSteps = 0;
        int           paddleSteps   = 0;
        std::string   screen;
    };


    static std::wstring Widen (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }


    //  Boots the disk and waits for the menu, with the Joyport attached or not.
    static void BootTheTestProgram (TestMachine & machine, bool isJoyportAttached)
    {
        std::vector<Byte>  bytes = GuestSession::RequireRepoImage (kProgramDiskPath);
        int                step  = 0;



        machine.GetJoyport()->SetAttached (isJoyportAttached);
        GuestSession::MountAndBoot (machine, bytes);

        for (step = 0; step < kMaxSteps && !Contains (ReadScreen (machine), "CHOOSE A TEST"); step++)
        {
            machine.RunCycles (kStepCycles);
        }

        Assert::IsTrue (Contains (ReadScreen (machine), "CHOOSE A TEST"), L"the program's menu");
    }


    //  The whole screen as one line, rows run together, so a phrase that
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


    //  The switch the prompt on screen asks for, or Count for none. A prompt
    //  starts the screen with the jack: "LEFT JOYSTICK: PUSH IT UP".
    static JoystickSwitch GetAskedSwitch (const std::string & screen, size_t & jack)
    {
        if (!Contains (screen, "JOYSTICK: "))
        {
            return JoystickSwitch::Count;
        }

        jack = screen.starts_with ("LEFT ") ? JoyportJacks::kLeftJack : JoyportJacks::kRightJack;

        if (Contains (screen, "PUSH IT RIGHT")) { return JoystickSwitch::Right; }
        if (Contains (screen, "PUSH IT LEFT"))  { return JoystickSwitch::Left;  }
        if (Contains (screen, "PRESS FIRE"))    { return JoystickSwitch::Fire;  }
        if (Contains (screen, "PUSH IT UP"))    { return JoystickSwitch::Up;    }
        if (Contains (screen, "PUSH IT DOWN"))  { return JoystickSwitch::Down;  }

        return JoystickSwitch::Count;
    }


    //  One look at the screen and one response to it, then time for the
    //  program's wait loop to see the switch.
    static void AdvanceTheJoystickTests (MachineHost & machine, ProgramRun & run)
    {
        std::string     screen      = ReadScreen (machine);
        JoyportJacks    jacks;
        JoystickSwitch  asked       = JoystickSwitch::Count;
        size_t          jack        = JoyportJacks::kLeftJack;
        bool            isNewPrompt = screen != run.screen;



        run.screen     = screen;
        run.isFailed   = Contains (screen, "NOTHING HAPPENED");
        run.isCentered = run.isCentered || Contains (screen, "REAR SWITCH TO THE MIDDLE");

        if (Contains (screen, "PRESS SPACE WHEN READY"))
        {
            Assert::IsTrue (Contains (screen, "SET THE FRONT SWITCH TO THE FRONT") || Contains (screen, "GAME PADDLE TEST"),
                            (L"every joystick setup screen gives the front switch: " + Widen (screen)).c_str());
            SetJacks (machine, jacks);
            KeystrokeInjector::InjectKey (machine, ' ');
            machine.RunCycles (kStepCycles);
            return;
        }

        asked = GetAskedSwitch (screen, jack);

        if (asked != JoystickSwitch::Count && run.isCentered)
        {
            jacks.jack[jack].set (static_cast<size_t> (asked));
            run.centeredSteps += isNewPrompt ? 1 : 0;
        }
        else if (asked != JoystickSwitch::Count)
        {
            jacks.jack[JoyportJacks::kLeftJack].set  (static_cast<size_t> (asked));
            jacks.jack[JoyportJacks::kRightJack].set (static_cast<size_t> (asked));
        }

        SetJacks (machine, jacks);
        machine.RunCycles (kStepCycles);
    }


    //  One look at the screen and one response: Space at a setup screen, a
    //  paddle at the other end from the last look while one is to be turned,
    //  and both buttons held while one is to be pressed. Paddles from
    //  `connected` up stay centered, as nothing drives them, and are skipped
    //  with S when the program asks for one.
    static void AdvanceThePaddleTest (MachineHost & machine, ProgramRun & run, int step, int connected)
    {
        std::string              screen    = ReadScreen (machine);
        Apple2eSoftSwitchBank  * bank      = machine.GetRefs().iieSoftSwitches;
        Apple2eKeyboard        * keyboard  = machine.GetRefs().iieKeyboard;
        bool                     isButton  = Contains (screen, "PRESS ITS BUTTON");
        Byte                     position  = (step % 2 == 0) ? kPaddleLeft : kPaddleRight;
        size_t                   turn      = screen.find ("TURN PADDLE ");
        int                      asked     = -1;
        int                      axis      = 0;



        if (turn != std::string::npos)
        {
            asked = screen[turn + kPaddleDigit] - '0';
        }

        run.paddleSteps += (asked >= 0 && screen.compare (0, TextScreenScraper::kCols40, run.screen, 0, TextScreenScraper::kCols40) != 0) ? 1 : 0;
        run.screen       = screen;
        run.isFailed     = Contains (screen, "NOTHING HAPPENED");

        if (Contains (screen, "PRESS SPACE WHEN READY"))
        {
            Assert::IsTrue (Contains (screen, "SET THE FRONT SWITCH TO THE REAR"),
                            (L"every paddle setup screen gives the front switch: " + Widen (screen)).c_str());
            KeystrokeInjector::InjectKey (machine, ' ');
        }

        if (asked >= connected)
        {
            KeystrokeInjector::InjectKey (machine, 'S');
        }

        for (axis = 0; axis < kPaddleCount; axis++)
        {
            bank->SetPaddle (axis, axis < connected ? position : kPaddleCenter);
        }

        keyboard->SetOpenApple   (isButton);
        keyboard->SetClosedApple (isButton);

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
