#pragma once

#include "Cli/DebugBatchRunner.h"
#include "ControllerRig.h"
#include "Core/JsonParser.h"
#include "EmuTests/FixtureRomSource.h"

#include "CppUnitTest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgProgram
//
//  The program every WinDbg rig loads, so a test reads its effects against
//  known code, with the PC on $0300, the stack empty and interrupts masked:
//
//    0300  JSR $0310        0310  LDA #$41
//    0303  NOP              0312  STA $0400
//    0304  NOP              0315  LDA $0401
//    0305  JMP $0305        0318  RTS
//
//  $0400 holds $00 and $0401 holds $42 until the program runs.
//
////////////////////////////////////////////////////////////////////////////////

class WinDbgProgram
{
public:
    static constexpr Word  kStart     = 0x0300;
    static constexpr Word  kCall      = 0x0310;
    static constexpr Byte  kStackTop  = 0xFF;
    static constexpr Byte  kFlags     = 0x24;
    static constexpr Byte  kStored    = 0x41;
    static constexpr Byte  kRead      = 0x42;



    static void Load (MachineHost & host)
    {
        static constexpr Byte  kMain[] = { 0x20, 0x10, 0x03, 0xEA, 0xEA, 0x4C, 0x05, 0x03 };
        static constexpr Byte  kSub[]  = { 0xA9, 0x41, 0x8D, 0x00, 0x04, 0xAD, 0x01, 0x04, 0x60 };
        Cpu6502Registers       registers;



        for (size_t i = 0; i < std::size (kMain); i++)
        {
            host.GetMemoryBus().WriteByte ((Word) (kStart + i), kMain[i]);
        }

        for (size_t i = 0; i < std::size (kSub); i++)
        {
            host.GetMemoryBus().WriteByte ((Word) (kCall + i), kSub[i]);
        }

        host.GetMemoryBus().WriteByte (0x0400, 0x00);
        host.GetMemoryBus().WriteByte (0x0401, kRead);

        registers    = host.GetCpu()->GetCpu6502()->GetRegisters();
        registers.pc = kStart;
        registers.sp = kStackTop;
        registers.p  = kFlags;
        registers.a  = 0;
        registers.x  = 0;
        registers.y  = 0;
        host.GetCpu()->GetCpu6502()->SetRegisters (registers);
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgWindowRig
//
//  The debugger window in WinDbg mode over a real machine, driven the way the
//  window drives it: the controller opened as opening the window opens it,
//  the session switched by the Dialect menu's MODE line, each typed line run
//  through ExecuteWindowLine in the session's mode as the emulator's command
//  handler runs it, and a run ended at slice boundaries as the CPU thread
//  ends it. A channel client listens, so each stop is read as the record a
//  client receives.
//
////////////////////////////////////////////////////////////////////////////////

class WinDbgWindowRig
{
public:
    //  One stop as the channel reported it.
    struct Stop
    {
        std::string          reason;
        Word                 pc           = 0;
        std::optional<int>   breakpointId;
        std::optional<int>   watchId;
        Word                 watchAddress = 0;
        Byte                 watchValue   = 0;
        std::string          watchAccess;
        Word                 accessPc     = 0;
        std::string          condition;
    };

    static constexpr uint64_t  kSliceCycles = 1023;
    static constexpr int       kMaxSlices   = 64;

    ControllerRig        rig;
    ChannelConnectionId  observer = 0;



    WinDbgWindowRig()
    {
        HRESULT  hr = S_OK;



        WinDbgProgram::Load (rig.machine);

        hr = rig.controller.Open();
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (SUCCEEDED (hr), L"the window opens the debugger");

        observer = rig.transport.Connect();
        rig.controller.Pump();

        TypeOk ("MODE WINDBG");
    }



    DebugSession & Session() { return rig.controller.GetSession(); }



    //  A line typed into the console.
    Reply Type (const std::string & line)
    {
        return rig.view.ExecuteWindowLine (Session(), line, Session().GetMode());
    }



    Reply TypeOk (const std::string & line)
    {
        Reply         reply = Type (line);
        std::string   why   = line + ": " + reply.error.label + ", " + reply.error.detail;
        std::wstring  where (why.begin(), why.end());



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, where.c_str());
        return reply;
    }



    //  A line that fails with exactly this status, label and detail, shown
    //  as the two-line error the console prints.
    Reply TypeFails (const std::string & line, CommandStatus status, const std::string & label, const std::string & detail)
    {
        Reply         reply = Type (line);
        std::wstring  where (line.begin(), line.end());



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) status, (int) reply.status, where.c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (label,  reply.error.label,  where.c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (detail, reply.error.detail, where.c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((size_t) 2, reply.text.size(), where.c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ("Error: " + label, reply.text[0], where.c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ("       " + detail, reply.text[1], where.c_str());
        return reply;
    }



    //  The CPU thread's slices until the run ends. False when it outlasts
    //  them, which a test that expects a stop fails on.
    bool FinishRun (int maxSlices = kMaxSlices)
    {
        for (int slice = 0; slice < maxSlices && rig.controller.GetRunDriver().IsRunning(); slice++)
        {
            uint64_t  ran = rig.machine.RunCycles (kSliceCycles);



            rig.controller.GetRunDriver().OnSliceExecuted ((uint32_t) ran);
        }

        rig.controller.Pump();
        return !rig.controller.GetRunDriver().IsRunning();
    }



    //  A run typed, then carried to its end.
    Stop RunToStop (const std::string & line)
    {
        std::wstring  where (line.begin(), line.end());



        TypeOk (line);
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (FinishRun(), where.c_str());
        return LastStop();
    }



    std::vector<Stop> GetStops()
    {
        std::vector<Stop>  stops;



        rig.controller.Pump();

        for (const std::string & record : rig.transport.Written (observer))
        {
            JsonValue         value;
            JsonParseError    error;
            std::string       type;
            const JsonValue * watch = nullptr;
            Stop              stop;
            int               number = 0;
            HRESULT           hr     = JsonParser::Parse (record, value, error);



            if (FAILED (hr) || !value.HasString ("type", type) || type != "stopped")
            {
                continue;
            }

            value.HasString ("reason", stop.reason);
            value.HasString ("condition", stop.condition);
            stop.pc = value.HasInt ("pc", number) ? (Word) number : 0;

            if (value.HasInt ("breakpointId", number))
            {
                stop.breakpointId = number;
            }

            if (value.HasObject ("watch", watch))
            {
                stop.watchId      = watch->HasInt ("id", number) ? std::optional<int> (number) : std::nullopt;
                stop.watchAddress = watch->HasInt ("address",  number) ? (Word) number : 0;
                stop.watchValue   = watch->HasInt ("value",    number) ? (Byte) number : 0;
                stop.accessPc     = watch->HasInt ("accessPc", number) ? (Word) number : 0;
                watch->HasString ("access", stop.watchAccess);
            }

            stops.push_back (stop);
        }

        return stops;
    }



    Stop LastStop()
    {
        std::vector<Stop>  stops = GetStops();



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsFalse (stops.empty(), L"a stop was announced");
        return stops.back();
    }



    Cpu6502Registers Registers() { return Session().GetTarget().GetRegisters(); }



    Byte Peek (Word address)
    {
        Byte  value = 0;



        Session().GetTarget().TryPeek (address, value);
        return value;
    }



    //  Test data: bytes in memory and the PC on start, as a loader would
    //  leave them.
    void Load (Word address, std::initializer_list<Byte> bytes)
    {
        Word  at = address;



        for (Byte b : bytes)
        {
            rig.machine.GetMemoryBus().WriteByte (at++, b);
        }
    }



    void SetPc (Word pc, Byte sp = WinDbgProgram::kStackTop)
    {
        Cpu6502Registers  registers = Registers();



        registers.pc = pc;
        registers.sp = sp;
        Session().GetTarget().SetRegisters (registers);
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgBatchRig
//
//  `CassoCli debug --mode windbg` over the fixture ROMs and an in-memory file
//  system: the batch runner builds the machine, the program is loaded before
//  the first line, and the lines run as a script file holds them, through
//  the synchronous run driver, with the batch sink printing the stops.
//
////////////////////////////////////////////////////////////////////////////////

class WinDbgBatchRig
{
public:
    FixtureRomSource                  roms;
    InMemoryFileSystem                files;
    DebugBatchRunner                  runner { roms, files };
    CommandLineOptions::DebugOptions  options;
    DebugBatchResult                  result;



    WinDbgBatchRig()
    {
        options.machine = "Apple2e";
        options.mode    = "windbg";
    }



    //  The exit status.
    int Run (const std::string & script)
    {
        HRESULT  hr = runner.Prepare (options, result);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, L"the batch machine was built");

        WinDbgProgram::Load (*runner.GetHost());
        runner.Execute (options, script, result);
        return result.exitStatus;
    }



    //  What one script line printed: the lines after its prompt, up to the
    //  next prompt. occurrence picks among lines typed more than once.
    std::string Section (const std::string & line, int occurrence = 0) const
    {
        std::istringstream  stream (result.output);
        std::string         each;
        std::string         text;
        bool                isInside = false;
        int                 seen     = 0;



        while (std::getline (stream, each))
        {
            bool  isPrompt = each.starts_with ("0:000> ") || each.starts_with (">");



            if (isPrompt && isInside)
            {
                break;
            }

            if (isInside)
            {
                text += each + "\n";
            }

            if (each == "0:000> " + line || each == ">" + line)
            {
                isInside = seen == occurrence;
                seen++;
            }
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (seen > occurrence, std::wstring (line.begin(), line.end()).c_str());
        return text;
    }



    Cpu6502Registers Registers() { return runner.GetHost()->GetCpu()->GetCpu6502()->GetRegisters(); }



    Byte Peek (Word address) { return runner.GetHost()->GetMemoryBus().ReadByte (address); }
};
