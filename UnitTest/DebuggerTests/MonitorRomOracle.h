#pragma once

#include "Debugger/DebugHandlerSet.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/SynchronousRunDriver.h"
#include "EmuTests/TestMachine.h"
#include "EmuTests/TextScreenScraper.h"
#include "HandlerTestRig.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "UiTests/InMemoryFileSystem.h"

#include "CppUnitTest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RomMonitor
//
//  The real Apple II Monitor, driven the way a person drives it: keys typed
//  at the `*` prompt, one at a time, and the answer read off the screen.
//  This is the oracle a Monitor-mode command is held to.
//
////////////////////////////////////////////////////////////////////////////////

class RomMonitor
{
public:
    //  Ceilings, not targets: the ROM waits at its prompt once it is done.
    static constexpr uint64_t  kBootCycles = 20'000'000;
    static constexpr uint64_t  kLineCycles = 2'000'000;



    ////////////////////////////////////////////////////////////////////////////
    //
    //  ReachMonitorPrompt
    //
    //  THROUGH THE MONITOR'S OWN RESET ENTRY, not through BASIC.
    //
    //  `CALL -151` is how a person reaches the Monitor, and it is not
    //  available here: with no disk the //e spins in its startup firmware
    //  showing only its banner, and the //c stops at "Check Disk Drive", so
    //  neither machine ever offers a BASIC prompt to type it at.
    //
    //  $FF59 is byte-identical on all five shipped ROMs -- SETNORM, INIT,
    //  SETVID, SETKBD, CLD, BELL, then MONZ, whose `LDA #$AA` is the `*`
    //  prompt. Entering there sets up the screen and the input and output
    //  hooks exactly as a reset does, with no operating system in the way.
    //
    ////////////////////////////////////////////////////////////////////////////

    static void ReachMonitorPrompt (TestMachine & machine)
    {
        static constexpr Word  kMonitorReset = 0xFF59;



        machine.PowerCycle();
        machine.RunCycles (kBootCycles);

        machine.GetCpu()->SetPC (kMonitorReset);
        machine.RunCycles (kLineCycles);
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  Type
    //
    //  One key, held on the latch until the ROM takes it.
    //
    //  On the base AppleKeyboard rather than the //e one: the //e-specific
    //  pointer is what KeystrokeInjector waits on, and waiting on it here
    //  landed no keys at all. This is the path MachineDebugTarget::InjectKey
    //  already uses.
    //
    ////////////////////////////////////////////////////////////////////////////

    static void Type (TestMachine & machine, Byte character)
    {
        static constexpr uint64_t  kSlice     = 20'000;
        static constexpr int       kMaxSlices = 200;
        AppleKeyboard            * keyboard   = machine.GetRefs().keyboard;
        int                        slice      = 0;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsNotNull (keyboard, L"the machine must have a keyboard");

        keyboard->PressKey (character);

        for (slice = 0; slice < kMaxSlices && !keyboard->IsStrobeClear(); ++slice)
        {
            machine.RunCycles (kSlice);
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (keyboard->IsStrobeClear(),
            std::format (L"the ROM did not take ${:02X} off the keyboard latch", character).c_str());
    }



    //  The line, then Return, then time for the ROM to act on it. `^X` is
    //  typed as the control key, as MonitorParser reads it.
    static void TypeLine (TestMachine & machine, const std::string & text)
    {
        static constexpr Byte  kReturn = 0x0D;
        static constexpr Byte  kCaret  = '^';
        static constexpr Byte  kToCtrl = 0x40;



        for (size_t i = 0; i < text.size(); ++i)
        {
            if ((Byte) text[i] == kCaret && i + 1 < text.size())
            {
                Type (machine, (Byte) (toupper ((unsigned char) text[++i]) - kToCtrl));
                continue;
            }

            Type (machine, (Byte) text[i]);
        }

        Type (machine, kReturn);
        machine.RunCycles (kLineCycles);
    }



    //  Each row of the text screen, trailing spaces dropped.
    static std::vector<std::string> ScreenRows (MachineHost & machine)
    {
        std::vector<std::string>  rows = TextScreenScraper::Scrape (machine);



        for (std::string & row : rows)
        {
            while (!row.empty() && row.back() == ' ')
            {
                row.pop_back();
            }
        }

        return rows;
    }



    static std::string Screen (MachineHost & machine)
    {
        std::string  text;



        for (const std::string & row : ScreenRows (machine))
        {
            text += row;
            text += '\n';
        }

        return text;
    }



    //  What the ROM printed for a line: the rows between its echo and the
    //  prompt that follows. The prompt is the last row starting with `*` and
    //  the echo the one before it. Controls echo as nothing, so the echo is
    //  matched with them removed.
    static std::vector<std::string> Output (TestMachine & machine, const std::string & line)
    {
        std::vector<std::string>  rows;
        std::vector<std::string>  output;
        std::string               echo   = "*";
        size_t                    prompt = std::string::npos;
        size_t                    at     = std::string::npos;



        for (size_t i = 0; i < line.size(); ++i)
        {
            if (line[i] == '^')
            {
                ++i;
                continue;
            }

            echo += (char) toupper ((unsigned char) line[i]);
        }

        TypeLine (machine, line);
        rows = ScreenRows (machine);

        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (rows[i].starts_with ("*"))
            {
                at     = prompt;
                prompt = i;
            }
        }

        if (at == std::string::npos || rows[at] != echo)
        {
            std::string  why = "the ROM did not echo " + echo + " before its prompt\n" + Screen (machine);



            Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail (std::wstring (why.begin(), why.end()).c_str());
        }

        for (size_t i = at + 1; i < prompt; ++i)
        {
            output.push_back (rows[i]);
        }

        return output;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  OracleRig
//
//  One original Apple II at its Monitor prompt, and a Casso session in
//  Monitor mode over the same machine, so a line can be put to the ROM and to
//  Casso against the same memory.
//
//  The original ][ is the oracle because its RESET is the Monitor: the
//  machine reaches the `*` prompt from power-on with nothing entered by
//  hand, and its ROM is the one Apple's 1979 listing documents.
//
////////////////////////////////////////////////////////////////////////////////

class OracleRig
{
public:
    TestMachine                machine;
    MachineDebugTarget         target;
    SynchronousRunDriver       driver;
    RecordingNotificationSink  sink;
    InMemoryFileSystem         files;
    DebugSession               session;
    DebugHandlerSet            handlers;

    OracleRig() :
        machine (std::string ("Apple2"), TestMachine::Slots::Empty),
        target  (machine),
        driver  (machine, target.GetRunHook()),
        session (target, sink, RunState::Paused)
    {
        target.SetRunDriver (&driver);
        handlers.Attach (session);
        session.SetFileSystem (&files);

        machine.PowerCycle();
        machine.RunCycles (RomMonitor::kLineCycles);

        session.ExecuteLine ("MODE MONITOR");
    }

    //  The ROM's printed lines for the line, typed at its prompt.
    std::vector<std::string> Rom (const std::string & line)
    {
        return RomMonitor::Output (machine, line);
    }

    //  Casso's reply text for the same line, in Monitor mode.
    std::vector<std::string> Casso (const std::string & line)
    {
        Reply  reply = session.ExecuteLine (line);



        session.FormatReply (reply);
        return reply.text;
    }

    void Poke (Word address, std::initializer_list<Byte> bytes)
    {
        Word  at = address;



        for (Byte value : bytes)
        {
            target.TryPoke (at++, value);
        }
    }

    Byte Peek (Word address)
    {
        Byte  value = 0;



        target.TryPeek (address, value);
        return value;
    }
};
