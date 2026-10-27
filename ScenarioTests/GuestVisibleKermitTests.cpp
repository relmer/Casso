#include "Pch.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleKermitTests
//
//  Kermit-65 3.87 for ProDOS, installed and configured by its own programs and
//  run to its prompt, on the three //e-class machines.
//
//  Setup selects the 80-column display. In 40 columns, Kermit clears a line
//  with JMP $FC95, which is SCRL3 inside SCROLL in the ][ and ][+ monitor and
//  not an entry point in any //e-class ROM. Kermit executes that clear at the
//  end of KERMIT.INIT, when it switches its input from the file to the
//  keyboard. What the genuine ROMs have at $FC95:
//
//    //e           the operand of a BPL, which the 6502 executes as SLO
//                  ($2C,X), EOR $C0,X, RTS, returning to Kermit intact
//    Enhanced //e  the middle of COUT1, which pops one byte too many, so a
//                  later RTS returns into Applesoft
//    //c           a line clear past its LDA #$A0, which fills the rest of
//                  the line with $00, displayed as @, and returns intact
//
//  So in 40 columns the Enhanced //e crashes and a row of @ appears at the
//  //c's prompt, both because of Kermit; Casso executes those bytes as the
//  hardware does. The 80-column setup clears the line through the firmware's
//  control codes instead.
//
//  Keys are typed with a pause after each one, and each answer waits for the
//  question the program prints next.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleKermitTests)
{
public:

    //  The installer, the setup program and KERMIT.INIT all run, and Kermit
    //  runs a command typed at its prompt and prints the result.
    TEST_METHOD (KermitRunsItsInitFileAndACommandTypedAtItsPrompt)
    {
        std::vector<Byte>  disk = GuestSession::RequireKermitDisk();



        for (const char * machineId : kMachines)
        {
            TestMachine                machine (machineId, TestMachine::Video::Timed);
            std::wstring               what = Widen (machineId);
            std::vector<std::string>   rows;



            GuestSession::BootToPrompt (machine, disk);

            TypeKeys (machine, "-KERMIT.INSTALL\r", "WHICH DRIVER?",          what);
            TypeKeys (machine, "1",                 "PRESS RETURN TO ACCEPT", what);
            TypeKeys (machine, "\r",                "RUN KERMIT.SETUP NOW?",  what);
            TypeKeys (machine, "Y",                 kSetupAnswers[0].prompt,  what);

            for (size_t i = 1; i < std::size (kSetupAnswers); i++)
            {
                TypeKeys (machine, kSetupAnswers[i - 1].keys, kSetupAnswers[i].prompt, what);
            }

            TypeKeys (machine, kSetupAnswers[std::size (kSetupAnswers) - 1].keys, "BREAK IN 920", what);

            TypeKeys (machine, "CLOSE\r",   "]CLOSE",         what);
            TypeKeys (machine, "-KERMIT\r", kLastHelpKeyword, what);
            Assert::IsTrue (IsAtKermitPrompt (machine), (what + L": the help list is followed by Kermit's prompt").c_str());

            TypeKeys (machine, "PWD\r", "PREFIX IS", what);

            rows = ReadScreen (machine);

            Assert::IsTrue (GuestSession::AnyRowIs (rows, "KERMIT-65>PWD"), (what + L": the command is echoed").c_str());
            Assert::IsTrue (IsAtKermitPrompt (machine),                     (what + L": and Kermit is back at its prompt: " + Widen (JoinRows (rows))).c_str());
        }
    }


private:

    //  One answer to the setup program, and what it prints next.
    struct SetupAnswer
    {
        const char *  keys;
        const char *  prompt;
    };

    static constexpr const char *  kMachines[] = { "Apple2e", "Apple2eEnhanced", "Apple2c" };

    //  No escape-character change, no XON/XOFF, no terminal emulation, text
    //  transfers, no parity, the Kermit protocol, the modem in slot 2 and the
    //  printer in slot 1, 9,600 baud, and the 80-column display in slot 3.
    static constexpr SetupAnswer  kSetupAnswers[] =
    {
        { "N", "CHANGE ESCAPE CHARACTER TO ESC?" },
        { "N", "XON/XOFF FLOW CONTROL?" },
        { "3", "WHICH TERMINAL?" },
        { "4", "WHICH TYPE? (1 - 4)" },
        { "1", "WHICH TYPE? (1 - 5)" },
        { "1", "WHICH PROTOCOL?" },
        { "Y", "MODEM IN SLOT 2?" },
        { "Y", "PRINTER IN SLOT 1?" },
        { "6", "WHICH SPEED?" },
        { "Y", "80 COLUMN DISPLAY?" },
        { "Y", "IN SLOT 3?" },
        { "N", "CHANGE SOMETHING?" },
    };

    //  The last row of the list Kermit prints for the `?` at the end of
    //  KERMIT.INIT.
    static constexpr const char *  kLastHelpKeyword = "UNLOCK";
    static constexpr const char *  kKermitPrompt    = "KERMIT-65>";

    //  While it sends a character, the Super Serial Card firmware discards
    //  keys pressed in that time, so each key is followed by this pause.
    static constexpr uint64_t  kKeyGapCycles = 60'000;

    //  How often the screen is read while a program runs, the cycles run before
    //  a missing prompt fails the test, and the cycles run after the prompt
    //  appears.
    static constexpr uint64_t  kPollCycles    = 250'000;
    static constexpr uint64_t  kCeilingCycles = 80'000'000;
    static constexpr uint64_t  kSettleCycles  = 2'000'000;


    static std::wstring Widen (const std::string & text)
    {
        return TextEncoding::NarrowToWide (text);
    }


    static std::string JoinRows (const std::vector<std::string> & rows)
    {
        std::string  text;

        for (const std::string & row : rows)
        {
            text += row + " | ";
        }

        return text;
    }


    //  The text screen's rows, 40 or 80 columns as displayed, without their
    //  trailing blanks.
    static std::vector<std::string> ReadScreen (MachineHost & machine)
    {
        std::vector<std::string>  rows = TextScreenScraper::Scrape (machine);

        for (std::string & row : rows)
        {
            row = GuestSession::TrimTrailingBlanks (row);
        }

        return rows;
    }


    //  True when the bottom-most non-blank row begins with Kermit's prompt,
    //  which is followed by the cursor on the frames it is lit.
    static bool IsAtKermitPrompt (MachineHost & machine)
    {
        std::vector<std::string>  rows = ReadScreen (machine);

        for (auto row = rows.rbegin(); row != rows.rend(); row++)
        {
            if (!row->empty())
            {
                return row->starts_with (kKermitPrompt);
            }
        }

        return false;
    }


    //  Types the keys, then runs until a row contains `prompt`, and a little
    //  longer so the program is in its keyboard loop before the next key.
    static void TypeKeys (MachineHost & machine, const char * keys, const char * prompt, const std::wstring & what)
    {
        uint64_t  spent = 0;
        bool      found = false;



        for (const char * c = keys; *c != '\0'; c++)
        {
            KeystrokeInjector::InjectKey (machine, static_cast<Byte> (*c));
            machine.RunCycles (kKeyGapCycles);
        }

        while (!found && spent < kCeilingCycles)
        {
            spent += machine.RunCycles (kPollCycles);
            found  = GuestSession::AnyRowContains (ReadScreen (machine), prompt);
        }

        Assert::IsTrue (found, (what + L": no row contains " + Widen (prompt) + L" after " + Widen (keys) + L": " + Widen (JoinRows (ReadScreen (machine)))).c_str());

        machine.RunCycles (kSettleCycles);
    }
};
