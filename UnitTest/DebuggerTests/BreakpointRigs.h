#pragma once

#include "Cli/DebugBatchSink.h"
#include "ControllerRig.h"
#include "Core/JsonParser.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/DebugSession.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/MonitorFormatter.h"
#include "Debugger/SynchronousRunDriver.h"
#include "Debugger/WinDbgFormatter.h"
#include "EmuTests/TestMachine.h"
#include "UiTests/InMemoryFileSystem.h"

#include "CppUnitTest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointRigs
//
//  The two ways a breakpoint command reaches a run, built as production builds
//  them, for the breakpoint and watchpoint tests.
//
//  BatchPathRig is `CassoCli debug`: a powered-on machine, the synchronous run
//  driver, every command family, the batch sink that prints stops, call
//  recording on and a cycle budget on every run, as DebugBatchRunner sets them
//  up. A run is over when the command that started it returns.
//
//  WindowPathRig is the debugger window and the pipe: the controller over a
//  real machine, opened as the emulator opens it, a channel client connected,
//  and the CPU thread's frame loop stood in for by RunSlices, which runs
//  slices and reports each to the run driver exactly as
//  EmulatorShell::ExecuteCpuSlices does. That one call site is the only part
//  of the path a unit test cannot run.
//
//  Neither rig installs a hook, a filter or a mask, or marks a flag: whatever
//  a test observes of those was put there by the command under test.
//
////////////////////////////////////////////////////////////////////////////////

namespace BreakpointRigs
{
    using Microsoft::VisualStudio::CppUnitTestFramework::Assert;

    inline std::wstring Widen (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }



    //  Two line lists, reported by the first line that differs, with both
    //  lists whole, so a failure reads as a diff.
    inline void AssertLines (const std::vector<std::string> & expected, const std::vector<std::string> & actual, const std::string & where)
    {
        std::string  shown = where + "\n  expected:";



        for (const std::string & line : expected)
        {
            shown += "\n    " + line;
        }

        shown += "\n  actual:";

        for (const std::string & line : actual)
        {
            shown += "\n    " + line;
        }

        Assert::IsTrue (expected == actual, Widen (shown).c_str());
    }



    inline JsonValue ParseJson (const std::string & record)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT         hr = JsonParser::Parse (record, value, error);



        Assert::AreEqual (S_OK, hr, Widen (record + ": " + error.message).c_str());
        return value;
    }



    inline std::string GetString (const JsonValue & value, const std::string & key)
    {
        std::string  text;



        Assert::IsTrue (value.HasString (key, text), Widen ("no string " + key).c_str());
        return text;
    }



    inline int GetInt (const JsonValue & value, const std::string & key)
    {
        int  number = 0;



        Assert::IsTrue (value.HasInt (key, number), Widen ("no number " + key).c_str());
        return number;
    }



    inline bool GetBool (const JsonValue & value, const std::string & key)
    {
        bool  flag = false;



        Assert::IsTrue (value.HasBool (key, flag), Widen ("no bool " + key).c_str());
        return flag;
    }



    //  A reply record's text lines.
    inline std::vector<std::string> GetTextLines (const JsonValue & value)
    {
        const JsonValue           * array = nullptr;
        std::vector<std::string>    lines;



        Assert::IsTrue (value.HasArray ("text", array), L"no text array");

        for (size_t i = 0; i < array->GetArraySize(); ++i)
        {
            lines.push_back (array->GetArrayElement (i).GetString());
        }

        return lines;
    }



    inline bool HasMember (const JsonValue & value, const std::string & key)
    {
        const std::vector<std::pair<std::string, JsonValue>> & members = value.GetObjectEntries();



        return std::any_of (members.begin(), members.end(), [&key] (const std::pair<std::string, JsonValue> & member) { return member.first == key; });
    }



    inline const JsonValue & GetObject (const JsonValue & value, const std::string & key)
    {
        const JsonValue  * member = nullptr;



        Assert::IsTrue (value.HasObject (key, member), Widen ("no object " + key).c_str());
        return *member;
    }



    inline void SplitInto (const std::string & text, std::vector<std::string> & lines)
    {
        size_t  start = 0;



        while (start < text.size())
        {
            size_t  end = text.find ('\n', start);



            end = (end == std::string::npos) ? text.size() : end;
            lines.push_back (text.substr (start, end - start));
            start = end + 1;
        }
    }



    //  A stop as each output format prints it: the Monitor's is the register
    //  line, WinDbg's is AppleWin's line and then the registers as r shows
    //  them, and the others print AppleWin's.
    inline std::vector<std::string> GetStopLines (CommandMode mode, const std::string & appleWin, const Cpu6502Registers & registers)
    {
        switch (mode)
        {
        case CommandMode::Monitor: return { MonitorFormatter::FormatRegisters (registers) };
        case CommandMode::WinDbg:  return { appleWin, WinDbgFormatter::FormatRegisters (registers) };
        default:                   return { appleWin };
        }
    }



    //  Program bytes at an address, and the CPU on its first with the stack
    //  empty, interrupts masked and A, X and Y clear. Test data, loaded the way
    //  a MEB would store it.
    inline void Load (IDebugTarget & target, Word address, std::initializer_list<Byte> bytes, Word pc)
    {
        static constexpr Byte  kIrqMasked = 0x34;
        static constexpr Byte  kStackTop  = 0xFF;
        Cpu6502Registers       registers  = target.GetRegisters();
        Word                   at         = address;



        for (Byte value : bytes)
        {
            Assert::IsTrue (target.TryPoke (at++, value), L"the program is in RAM");
        }

        registers.pc = pc;
        registers.sp = kStackTop;
        registers.p  = kIrqMasked;
        registers.a  = 0;
        registers.x  = 0;
        registers.y  = 0;
        target.SetRegisters (registers);
    }



    inline Byte Peek (const IDebugTarget & target, Word address)
    {
        Byte  value = 0;



        Assert::IsTrue (target.TryPeek (address, value), L"the byte is readable");
        return value;
    }





    ////////////////////////////////////////////////////////////////////////////
    //
    //  Programs
    //
    //  The code the run tests execute. Each fills $0400 with $A0 first, so a
    //  store that has not happened is distinguishable from one that has.
    //
    ////////////////////////////////////////////////////////////////////////////

    struct Programs
    {
        //  0300 LDA #$41 / 0302 STA $0400 / 0305 INX / 0306 INY / 0307 JMP $0307
        //  A straight line into a spin, so a run that passes every breakpoint
        //  is still somewhere a sentinel breakpoint at $0307 can end it.
        static void LoadLine (IDebugTarget & target)
        {
            Load (target, 0x0300, { 0xA9, 0x41, 0x8D, 0x00, 0x04, 0xE8, 0xC8, 0x4C, 0x07, 0x03 }, 0x0300);
            Assert::IsTrue (target.TryPoke (0x0400, 0xA0));
        }

        //  0300 LDA #$41 / 0302 STA $0400 / 0305 INX / 0306 JMP $0300
        //  Every pass counts itself in X.
        static void LoadLoop (IDebugTarget & target)
        {
            Load (target, 0x0300, { 0xA9, 0x41, 0x8D, 0x00, 0x04, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);
            Assert::IsTrue (target.TryPoke (0x0400, 0xA0));
        }

        //  0300 JSR $0310 / 0303 INX / 0304 JMP $0304
        //  0310 LDA #$41 / 0312 STA $0400 / 0315 INY / 0316 RTS
        static void LoadCall (IDebugTarget & target)
        {
            Load (target, 0x0310, { 0xA9, 0x41, 0x8D, 0x00, 0x04, 0xC8, 0x60 }, 0x0300);
            Load (target, 0x0300, { 0x20, 0x10, 0x03, 0xE8, 0x4C, 0x04, 0x03 }, 0x0300);
            Assert::IsTrue (target.TryPoke (0x0400, 0xA0));
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  BatchPathRig
    //
    ////////////////////////////////////////////////////////////////////////////

    class BatchPathRig
    {
    public:
        //  Every batch run carries a budget; this one is short enough that a
        //  run whose breakpoint never fires ends in a budget stop the test can
        //  see, rather than in a hang.
        static constexpr uint64_t  kBudget = 200000;

        //  Powered on before the target and the session see it, as the batch
        //  runner builds it.
        class PoweredMachine : public TestMachine
        {
        public:
            explicit PoweredMachine (const std::string & id) : TestMachine (id, TestMachine::Slots::Empty)
            {
                PowerCycle();
            }
        };

        PoweredMachine        machine;
        MachineDebugTarget    target;
        SynchronousRunDriver  driver;
        DebugBatchSink        sink;
        InMemoryFileSystem    files;
        DebugSession          session;
        DebugHandlerSet       handlers;
        Reply                 last;



        explicit BatchPathRig (const std::string & id = "Apple2e") :
            machine (id),
            target  (machine),
            driver  (machine, target.GetRunHook()),
            session (target, sink, RunState::Paused)
        {
            target.SetRunDriver   (&driver);
            sink.SetSession       (&session);
            handlers.Attach       (session);
            session.SetFileSystem (&files);
            session.SetCallRecording (true);
            session.SetBudget     (kBudget);
        }



        //  One script line as the batch runner runs it: the reply's text in
        //  the session's output format, then whatever the command caused.
        std::vector<std::string> Run (const std::string & line)
        {
            std::vector<std::string>  lines;



            last = session.ExecuteLine (line);
            session.FormatReply (last);

            lines = last.text;
            SplitInto (sink.TakePending(), lines);
            return lines;
        }



        std::vector<std::string> RunOk (const std::string & line)
        {
            std::vector<std::string>  lines = Run (line);



            Assert::AreEqual ((int) CommandStatus::Ok, (int) last.status,
                              Widen (line + ": " + last.error.label + ", " + last.error.detail).c_str());
            return lines;
        }



        //  A failure, its status, its label and its whole message.
        void RunFails (const std::string & line, const std::string & label, const std::string & detail, CommandStatus status = CommandStatus::Error)
        {
            Run (line);

            Assert::AreEqual ((int) status, (int) last.status, Widen (line).c_str());
            Assert::AreEqual (label,  last.error.label,  Widen (line).c_str());
            Assert::AreEqual (detail, last.error.detail, Widen (line).c_str());
        }



        void Expect (const std::string & line, const std::vector<std::string> & expected)
        {
            AssertLines (expected, RunOk (line), line);
        }



        //  A line in a mode chosen for it alone, as a channel request naming a
        //  mode runs it; the session's own mode is left as it was.
        std::vector<std::string> RunIn (const std::string & line, CommandMode mode)
        {
            std::vector<std::string>  lines;



            last = session.ExecuteLine (line, mode);
            session.FormatReply (last, mode);

            lines = last.text;
            SplitInto (sink.TakePending(), lines);
            return lines;
        }



        //  BPL in AppleWin's words, whatever mode the script is in.
        std::vector<std::string> List ()
        {
            return RunIn ("BPL", CommandMode::AppleWin);
        }



        //  MODE, through the current mode's marker, as a script switches.
        void EnterMode (CommandMode mode)
        {
            if (mode != session.GetMode())
            {
                RunOk (DebuggerViewState::GetModeLine (std::string ("MODE ") + CommandModeNames::GetUpperName (mode), session.GetMode()));
            }
        }



        Cpu6502Registers  GetRegisters () const       { return target.GetRegisters(); }
        Byte              Peek         (Word address) { return BreakpointRigs::Peek (target, address); }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  WindowPathRig
    //
    ////////////////////////////////////////////////////////////////////////////

    class WindowPathRig
    {
    public:
        //  The frame loop's slice, as EmulatorShell::ExecuteCpuSlices takes it.
        static constexpr uint32_t  kSliceCycles = 1023;

        //  Far more slices than any test's run needs, so a run that should
        //  have stopped is caught still running rather than looping on.
        static constexpr int  kSliceLimit = 400;

        ControllerRig        rig;
        ChannelConnectionId  client = 0;
        int64_t              nextId = 1;



        WindowPathRig()
        {
            HRESULT  hr = rig.controller.Open();



            Assert::AreEqual (S_OK, hr, L"the controller opens as the emulator opens it");

            client = rig.transport.Connect();
            rig.controller.Pump();
        }



        DebugSession        & GetSession () { return rig.controller.GetSession(); }
        IDebugTarget        & GetTarget  () { return rig.controller.GetSession().GetTarget(); }
        CpuManagerRunDriver & GetDriver  () { return rig.controller.GetRunDriver(); }
        Cpu6502Registers      GetRegisters () { return GetTarget().GetRegisters(); }
        Byte                  Peek       (Word address) { return BreakpointRigs::Peek (GetTarget(), address); }



        //  A line typed into the window's console, run as the emulator runs
        //  it: in the session's own mode, through the window's line handling.
        Reply Run (const std::string & line)
        {
            return rig.view.ExecuteWindowLine (GetSession(), line, GetSession().GetMode());
        }



        //  The Dialect menu's MODE, sent through the current mode's marker.
        void EnterMode (CommandMode mode)
        {
            if (mode != GetSession().GetMode())
            {
                RunOk (DebuggerViewState::GetModeLine (std::string ("MODE ") + CommandModeNames::GetUpperName (mode), GetSession().GetMode()));
            }
        }



        //  BPL in AppleWin's words, as the breakpoints pane reads it.
        std::vector<std::string> List ()
        {
            Reply  reply = GetSession().ExecuteLine ("BPL", CommandMode::AppleWin);



            GetSession().FormatReply (reply, CommandMode::AppleWin);
            return reply.text;
        }



        Reply RunOk (const std::string & line)
        {
            Reply  reply = Run (line);



            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status,
                              Widen (line + ": " + reply.error.label + ", " + reply.error.detail).c_str());
            return reply;
        }



        //  The same line from the channel client, with its id; returns the
        //  reply record parsed.
        JsonValue Send (const std::string & line)
        {
            std::string  escaped;
            size_t       before = rig.transport.Written (client).size();
            int64_t      id     = nextId++;



            for (char ch : line)
            {
                if (ch == '"' || ch == '\\')
                {
                    escaped += '\\';
                }

                escaped += ch;
            }

            rig.transport.Send (client, std::format ("{{\"type\":\"command\",\"id\":{},\"line\":\"{}\"}}", id, escaped));
            rig.controller.Pump();

            Assert::IsTrue (rig.transport.Written (client).size() > before, Widen ("no reply to " + line).c_str());
            return ParseJson (rig.transport.Written (client)[before]);
        }



        //  The frame loop: slices while the CPU manager lets the machine run,
        //  each reported to the run driver, which ends a run on the slice its
        //  stop landed in; then the service tick that delivers notifications.
        //  Returns the cycles run.
        uint64_t RunSlices (int slices = kSliceLimit)
        {
            uint64_t  total = 0;



            for (int slice = 0; slice < slices && !rig.cpuManager.IsPaused(); ++slice)
            {
                uint64_t  ran = rig.machine.RunCycles (kSliceCycles);



                total += ran;

                if (GetDriver().OnSliceExecuted ((uint32_t) ran))
                {
                    break;
                }
            }

            rig.controller.Pump();
            return total;
        }



        //  A run started from the window, carried to its end by the frame loop.
        void Go (const std::string & line)
        {
            RunOk (line);
            RunSlices();

            Assert::IsFalse (GetDriver().IsRunning(), Widen (line + ": the run is still going").c_str());
        }



        //  Every record of a type the client has received, parsed, in order.
        std::vector<JsonValue> GetRecords (const std::string & type)
        {
            std::vector<JsonValue>  records;
            std::string             kind;



            for (const std::string & record : rig.transport.Written (client))
            {
                JsonValue  value = ParseJson (record);



                if (value.HasString ("type", kind) && kind == type)
                {
                    records.push_back (value);
                }
            }

            return records;
        }



        //  The one stop notification the client has received since the count
        //  was taken, checked for its reason and its PC.
        JsonValue GetOnlyStop (size_t stopsBefore, const std::string & reason, Word pc)
        {
            std::vector<JsonValue>  stops = GetRecords ("stopped");
            std::string             actual;
            int                     at     = -1;



            Assert::AreEqual (stopsBefore + 1, stops.size(), L"one stop notification for the run");
            Assert::IsTrue   (stops.back().HasString ("reason", actual));
            Assert::AreEqual (reason, actual);
            Assert::IsTrue   (stops.back().HasInt ("pc", at));
            Assert::AreEqual ((int) pc, at, L"the stop's PC");
            return stops.back();
        }
    };
}
