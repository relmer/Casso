#pragma once

#include "Debugger/DebuggerController.h"
#include "EmuTests/TestMachine.h"
#include "InMemoryPipeTransport.h"
#include "Shell/CpuManager.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "UiTests/InMemoryFileSystem.h"

#include "CppUnitTest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WindowConsoleRig
//
//  The debugger window's command box over a real machine. A typed line goes
//  where the emulator's console handler sends it: DebuggerViewState::
//  ExecuteWindowLine, in the mode the session is in. ControllerRig::Run calls
//  the static ExecuteLine, which is batch's answer to a window-only name, so
//  it cannot stand in for the console here.
//
//  Runs are the emulator's: a run a line starts is carried by the CPU
//  manager's run driver, and RunFrames stands in for the frame loop, running
//  a slice of the machine and then asking the driver whether the run ended.
//
//  The dialect is changed the way a user changes it, by typing MODE, so the
//  mode a line runs in is the session's own and not one a test passed in.
//
////////////////////////////////////////////////////////////////////////////////

class WindowConsoleRig
{
public:
    static constexpr uint32_t  kSliceCycles = 1023;
    static constexpr int       kMaxFrames   = 200;

    TestMachine            machine;
    CpuManager             cpuManager;
    InMemoryPipeTransport  transport;
    InMemoryFileSystem     files;
    DebuggerController     controller;
    DebuggerViewState      view;



    explicit WindowConsoleRig (const std::string & machineId = "Apple2e", TestMachine::Slots slots = TestMachine::Slots::Empty) :
        machine    (machineId, slots),
        controller (machine, Paused (cpuManager), transport, files, nullptr, 1)
    {
    }



    static CpuManager & Paused (CpuManager & cpu)
    {
        cpu.SetPaused (true);
        return cpu;
    }



    DebugSession & Session()
    {
        return controller.GetSession();
    }



    //  A line typed into the command box, run in the session's own mode, as
    //  the emulator's console handler runs it.
    Reply Type (const std::string & line)
    {
        return view.ExecuteWindowLine (Session(), line, Session().GetMode());
    }



    //  The snapshot the CPU thread hands the window.
    DebuggerViewSnapshot Build (bool isPaused = true)
    {
        return view.Build (Session(), isPaused);
    }



    void Poke (Word address, std::initializer_list<Byte> bytes)
    {
        Word  at = address;



        for (Byte b : bytes)
        {
            machine.GetMemoryBus().WriteByte (at++, b);
        }
    }



    //  Every byte from first up to, not including, end.
    void Fill (Word first, uint32_t end, Byte value)
    {
        for (uint32_t at = first; at < end; at++)
        {
            machine.GetMemoryBus().WriteByte ((Word) at, value);
        }
    }



    Byte Peek (Word address)
    {
        return machine.GetMemoryBus().ReadByte (address);
    }



    Cpu6502Registers GetRegisters()
    {
        return Session().GetTarget().GetRegisters();
    }



    //  The PC, with interrupts masked and the stack pointer where a test puts
    //  it, so a run is the program's alone.
    void SetPc (Word pc, Byte sp = 0xFF)
    {
        static constexpr Byte  kIrqMasked = 0x34;
        Cpu6502Registers       r          = GetRegisters();



        r.pc = pc;
        r.sp = sp;
        r.p  = kIrqMasked;
        Session().GetTarget().SetRegisters (r);
    }



    //  The frame loop: a slice of the machine, then the driver's check, until
    //  the run a line started has ended. False when it never did.
    bool RunUntilStopped()
    {
        for (int frame = 0; frame < kMaxFrames && controller.GetRunDriver().IsRunning(); frame++)
        {
            uint64_t  ran = machine.RunCycles (kSliceCycles);



            if (controller.GetRunDriver().OnSliceExecuted ((uint32_t) ran))
            {
                return true;
            }
        }

        return !controller.GetRunDriver().IsRunning();
    }



    //  Frames of a run that has no reason to stop: the machine runs the
    //  cycles and the driver is told, as each frame tells it.
    void RunFrames (int frames, uint32_t cycles = kSliceCycles)
    {
        for (int frame = 0; frame < frames; frame++)
        {
            uint64_t  ran = machine.RunCycles (cycles);



            (void) controller.GetRunDriver().OnSliceExecuted ((uint32_t) ran);
        }
    }



    //  T typed into the console, and the frames that carry the step out.
    void Step()
    {
        Reply  reply = Type ("T");



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, L"T started a step");
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue   (RunUntilStopped(), L"and the step ended");
    }



    //  The addresses of a disassembly view's rows, to compare two builds.
    static std::vector<Word> GetAddresses (const std::vector<DebuggerViewSnapshot::CodeLine> & rows)
    {
        std::vector<Word>  addresses;



        for (const DebuggerViewSnapshot::CodeLine & row : rows)
        {
            addresses.push_back (row.address);
        }

        return addresses;
    }



    //  The row holding an address, or -1 when the view does not show it.
    static int GetRowOf (const std::vector<DebuggerViewSnapshot::CodeLine> & rows, Word address)
    {
        for (size_t i = 0; i < rows.size(); i++)
        {
            if (rows[i].address == address)
            {
                return (int) i;
            }
        }

        return -1;
    }



    //  The row marked as the PC's, or -1.
    static int GetCurrentRow (const std::vector<DebuggerViewSnapshot::CodeLine> & rows)
    {
        for (size_t i = 0; i < rows.size(); i++)
        {
            if (rows[i].isCurrent)
            {
                return (int) i;
            }
        }

        return -1;
    }



    //  Nothing a window command changes has changed between two builds: the
    //  disassembly views and where they are, the memory windows, the pane last
    //  brought forward and the last Go to.
    static void AssertPanesUnchanged (const DebuggerViewSnapshot & before, const DebuggerViewSnapshot & after, const std::string & what)
    {
        using Microsoft::VisualStudio::CppUnitTestFramework::Assert;

        std::wstring  where = Widen (what);



        for (size_t view = 0; view < before.codeViews.size(); view++)
        {
            Assert::IsTrue   (GetAddresses (before.codeViews[view]) == GetAddresses (after.codeViews[view]), where.c_str());
            Assert::AreEqual (before.codeOpen[view], after.codeOpen[view], where.c_str());
        }

        Assert::AreEqual (before.followView,           after.followView,           where.c_str());
        Assert::AreEqual (before.memoryWindows.size(), after.memoryWindows.size(), where.c_str());

        for (size_t i = 0; i < before.memoryWindows.size() && i < after.memoryWindows.size(); i++)
        {
            Assert::AreEqual (before.memoryWindows[i].id,    after.memoryWindows[i].id,    where.c_str());
            Assert::AreEqual (before.memoryWindows[i].first, after.memoryWindows[i].first, where.c_str());
        }

        Assert::AreEqual (before.showPane,         after.showPane,         where.c_str());
        Assert::AreEqual (before.showPaneSerial,   after.showPaneSerial,   where.c_str());
        Assert::AreEqual (before.goTo.has_value(), after.goTo.has_value(), where.c_str());

        if (before.goTo.has_value() && after.goTo.has_value())
        {
            Assert::AreEqual (before.goTo->serial, after.goTo->serial, where.c_str());
        }
    }



    static std::wstring Widen (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }



    //  The reply's text as one string, a line to a line.
    static std::string Join (const std::vector<std::string> & lines)
    {
        std::string  text;



        for (const std::string & line : lines)
        {
            text += line + "\n";
        }

        return text;
    }



    //  Status, label, detail and every line of text, exactly.
    static void AssertReply (const Reply                    & reply,
                             CommandStatus                    status,
                             const std::string              & label,
                             const std::string              & detail,
                             const std::vector<std::string> & text,
                             const std::string              & what)
    {
        using Microsoft::VisualStudio::CppUnitTestFramework::Assert;

        std::wstring  where = Widen (what);



        Assert::AreEqual ((int) status,  (int) reply.status, where.c_str());
        Assert::AreEqual (label,         reply.error.label,  where.c_str());
        Assert::AreEqual (detail,        reply.error.detail, where.c_str());
        Assert::AreEqual (Join (text),   Join (reply.text),  where.c_str());
    }
};
