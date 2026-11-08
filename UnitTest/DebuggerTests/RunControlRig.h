#pragma once

#include "Cli/DebugBatchRunner.h"
#include "Core/JsonParser.h"
#include "Debugger/DebuggerController.h"
#include "EmuTests/FixtureRomSource.h"
#include "EmuTests/TestMachine.h"
#include "InMemoryPipeTransport.h"
#include "Shell/CpuManager.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "UiTests/InMemoryFileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RunText
//
//  Text helpers the run-control tests share.
//
////////////////////////////////////////////////////////////////////////////////

class RunText
{
public:
    static std::wstring Widen (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }

    //  Lines joined with a newline after each, the way batch output and a
    //  reply's text are compared.
    static std::string Join (const std::vector<std::string> & lines)
    {
        std::string  joined;



        for (const std::string & line : lines)
        {
            joined += line + "\n";
        }

        return joined;
    }

    static JsonValue Parse (const std::string & record)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT         hr = JsonParser::Parse (record, value, error);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, Widen (record + ": " + error.message).c_str());
        return value;
    }

    static std::string GetString (const JsonValue & record, const std::string & key)
    {
        std::string  value;
        bool         isThere = record.HasString (key, value);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (isThere, Widen ("no string " + key).c_str());
        return value;
    }

    static int GetInt (const JsonValue & record, const std::string & key)
    {
        int   value   = 0;
        bool  isThere = record.HasInt (key, value);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (isThere, Widen ("no number " + key).c_str());
        return value;
    }

    static bool Has (const JsonValue & record, const std::string & key)
    {
        for (const auto & [name, value] : record.GetObjectEntries())
        {
            if (name == key)
            {
                return true;
            }
        }

        return false;
    }

    static const JsonValue & GetObject (const JsonValue & record, const std::string & key)
    {
        const JsonValue  * value   = nullptr;
        bool               isThere = record.HasObject (key, value);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (isThere, Widen ("no object " + key).c_str());
        return *value;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  Dialects
//
//  The five command modes as a user reaches them: each mode's name as MODE
//  takes it, and each mode's marker for an engine command.
//
////////////////////////////////////////////////////////////////////////////////

class Dialects
{
public:
    static constexpr CommandMode  kAll[] = { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared,
                                             CommandMode::WinDbg,   CommandMode::Casso };

    static std::string GetName (CommandMode mode)
    {
        switch (mode)
        {
        case CommandMode::Monitor:   return "MONITOR";
        case CommandMode::GSSquared: return "GSSQUARED";
        case CommandMode::WinDbg:    return "WINDBG";
        case CommandMode::Casso:     return "CASSO";
        default:                     return "APPLEWIN";
        }
    }

    //  An engine command typed in a mode: behind `/` in the Monitor's, `!` in
    //  WinDbg's, and by its bare name in the others.
    static std::string GetEngineLine (CommandMode mode, const std::string & line)
    {
        switch (mode)
        {
        case CommandMode::Monitor: return "/" + line;
        case CommandMode::WinDbg:  return "!" + line;
        default:                   return line;
        }
    }

    //  The line that switches a session in mode `from` to mode `to`.
    static std::string GetSwitchLine (CommandMode from, CommandMode to)
    {
        return GetEngineLine (from, "MODE " + GetName (to));
    }

    static std::wstring Describe (CommandMode mode)
    {
        return RunText::Widen (GetName (mode));
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorRunRig
//
//  The debugger as it runs inside the emulator: a DebuggerController over a
//  real machine, opened as EmulatorShell opens it (so the call record is on),
//  a DebuggerViewState for the console, and one channel client connected, so
//  every notification the session raises is seen as a client sees it.
//
//  THE CPU THREAD'S LOOP IS STOOD IN FOR BY Pass. EmulatorShell cannot run in
//  a test, so Pass makes the calls its loop makes, in its order: the service
//  tick's Pump, then, only while the CPU manager is not paused, a frame cut
//  into 1023-cycle slices, each reported to the run driver BEFORE the zero-
//  slice test, as ExecuteCpuSlices reports it. A console line is what the
//  IDM_DEBUG_COMMAND handler runs: ExecuteWindowLine in the session's mode.
//
////////////////////////////////////////////////////////////////////////////////

class EmulatorRunRig
{
public:
    static constexpr uint32_t  kSliceCycles = 1023;
    static constexpr uint32_t  kFrameCycles = 17030;
    static constexpr int       kMaxPasses   = 200;
    static constexpr uint32_t  kProcessId   = 4242;

    TestMachine            machine;
    CpuManager             cpuManager;
    InMemoryPipeTransport  transport;
    InMemoryFileSystem     files;
    DebuggerController     controller;
    DebuggerViewState      view;
    ChannelConnectionId    client = 0;

    //  running: the machine is running freely when the debugger opens, as
    //  it is when a game is in play; otherwise it is paused.
    explicit EmulatorRunRig (bool running = false, const std::string & machineId = "Apple2e") :
        machine    (machineId, TestMachine::Slots::Empty),
        controller (machine, SetPaused (cpuManager, !running), transport, files, nullptr, kProcessId)
    {
        HRESULT  hr = S_OK;



        machine.PowerCycle();

        hr = controller.Open();
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (SUCCEEDED (hr), L"the debugger opened");

        client = transport.Connect();
        controller.Pump();
    }

    static CpuManager & SetPaused (CpuManager & cpu, bool paused)
    {
        cpu.SetPaused (paused);
        return cpu;
    }

    DebugSession & Session()
    {
        return controller.GetSession();
    }

    //  Writes bytes and points the CPU at start, with the stack at the top
    //  and interrupts masked.
    void Load (Word address, std::initializer_list<Byte> bytes, Word start)
    {
        static constexpr Byte  kIrqMasked = 0x34;
        static constexpr Byte  kStackTop  = 0xFF;
        IDebugTarget         & target     = Session().GetTarget();
        Cpu6502Registers       registers  = target.GetRegisters();
        Word                   at         = address;



        for (Byte b : bytes)
        {
            target.TryPoke (at++, b);
        }

        registers.pc = start;
        registers.sp = kStackTop;
        registers.p  = kIrqMasked;
        target.SetRegisters (registers);
    }

    Cpu6502Registers Registers()
    {
        return Session().GetTarget().GetRegisters();
    }

    Byte Peek (Word address)
    {
        Byte  value = 0;



        Session().GetTarget().TryPeek (address, value);
        return value;
    }

    //  A line typed into the window's console, in the session's mode, as the
    //  CPU thread runs it from its command queue; the service tick's pump
    //  follows, as it does there.
    Reply Console (const std::string & line)
    {
        Reply  reply = view.ExecuteWindowLine (Session(), line, Session().GetMode());



        controller.Pump();
        return reply;
    }

    //  A console line that must succeed.
    Reply ConsoleOk (const std::string & line)
    {
        Reply  reply = Console (line);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status,
                                                                          RunText::Widen (line + ": " + reply.error.label + ", " + reply.error.detail).c_str());
        return reply;
    }

    //  The session switched into a mode from AppleWin, as a user types it.
    void EnterMode (CommandMode mode)
    {
        ConsoleOk (Dialects::GetSwitchLine (Session().GetMode(), mode));
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (Session().GetMode() == mode, Dialects::Describe (mode).c_str());
    }

    //  The main window's pause and resume. The UI thread sets the CPU manager,
    //  then the CPU thread runs EmulatorShell::NotifyDebugPauseChanged, which
    //  ends a debugger run for a pause and tells the session. EmulatorShell
    //  cannot run in a test, so these make the calls it makes, in its order.
    void UserPause()
    {
        cpuManager.SetPaused (true);
        controller.GetRunDriver().EndForUserPause();
        Session().OnUserPaused();
        controller.Pump();
    }

    void UserResume()
    {
        cpuManager.SetPaused (false);
        Session().OnUserResumed();
        controller.Pump();
    }

    //  One pass of the CPU thread's loop.
    void Pass (uint32_t sliceCycles = kSliceCycles)
    {
        uint32_t  executed = 0;
        uint32_t  ran      = 0;



        controller.Pump();

        if (cpuManager.IsPaused())
        {
            return;
        }

        while (executed < kFrameCycles)
        {
            ran       = (uint32_t) machine.RunCycles (std::min (sliceCycles, kFrameCycles - executed));
            executed += ran;

            if (controller.GetRunDriver().OnSliceExecuted (ran))
            {
                break;
            }

            if (ran == 0)
            {
                break;
            }
        }
    }

    //  Passes until the machine is paused, or maxPasses; then the pump that
    //  sends what the last pass raised. True when the machine is paused.
    bool RunUntilPaused (int maxPasses = kMaxPasses)
    {
        for (int pass = 0; pass < maxPasses && !cpuManager.IsPaused(); ++pass)
        {
            Pass();
        }

        controller.Pump();
        return cpuManager.IsPaused();
    }

    //  A request from the connected client, pumped through; the reply to it.
    JsonValue Request (const std::string & json)
    {
        size_t  before = transport.Written (client).size();



        transport.Send (client, json);
        controller.Pump();

        return RecordAfter (before, "reply");
    }

    JsonValue Command (const std::string & line, int id = 1)
    {
        return Request (std::format ("{{\"type\":\"command\",\"id\":{},\"line\":\"{}\"}}", id, line));
    }

    //  The records the client received, parsed.
    std::vector<JsonValue> Records (ChannelConnectionId connection = 0)
    {
        std::vector<JsonValue>  records;



        for (const std::string & record : transport.Written (connection == 0 ? client : connection))
        {
            records.push_back (RunText::Parse (record));
        }

        return records;
    }

    std::vector<JsonValue> RecordsOfType (const std::string & type, ChannelConnectionId connection = 0)
    {
        std::vector<JsonValue>  matching;
        std::string             each;



        for (const JsonValue & record : Records (connection))
        {
            if (record.HasString ("type", each) && each == type)
            {
                matching.push_back (record);
            }
        }

        return matching;
    }

    //  The one stop the client has heard, failing if it heard none or more.
    JsonValue OnlyStop()
    {
        std::vector<JsonValue>  stops = RecordsOfType ("stopped");



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((size_t) 1, stops.size(), L"the client heard one stop");
        return stops.front();
    }

    JsonValue LastStop()
    {
        std::vector<JsonValue>  stops = RecordsOfType ("stopped");



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsFalse (stops.empty(), L"the client heard no stop");
        return stops.back();
    }

    //  The first record of a type written at or after index before.
    JsonValue RecordAfter (size_t before, const std::string & type)
    {
        const std::vector<std::string> & written = transport.Written (client);
        std::string                      each;



        for (size_t index = before; index < written.size(); ++index)
        {
            JsonValue  record = RunText::Parse (written[index]);



            if (record.HasString ("type", each) && each == type)
            {
                return record;
            }
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail (RunText::Widen ("no " + type + " record arrived").c_str());
        return JsonValue();
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  BatchScriptRig
//
//  `CassoCli debug` as DebugMode runs it, less the console: a DebugBatchRunner
//  over the fixture ROMs and an in-memory file system. The machine it built
//  stays reachable after the run, so a test can read what the script did to
//  memory and registers as well as what it printed.
//
//  Every run carries a small budget unless a test sets another, so a run that
//  fails to stop fails the test rather than spinning for the batch default.
//
////////////////////////////////////////////////////////////////////////////////

class BatchScriptRig
{
public:
    static constexpr uint64_t  kTestBudget = 200000;

    FixtureRomSource                  roms;
    InMemoryFileSystem                files;
    CommandLineOptions::DebugOptions  options;
    DebugBatchResult                  result;
    DebugBatchRunner                  runner { roms, files };

    BatchScriptRig()
    {
        options.machine   = "Apple2e";
        options.maxCycles = kTestBudget;
    }

    //  A program placed in memory after the machine is built and before the
    //  script runs, as a loader would leave it, with the PC on start.
    void Load (Word address, const std::vector<Byte> & bytes, std::optional<Word> start = std::nullopt)
    {
        m_loads.emplace_back (address, bytes);

        if (start.has_value())
        {
            m_start = start;
        }
    }

    //  The lines as --command lines, run as DebugBatchRunner::Run runs them,
    //  with the loads placed between building the machine and the script;
    //  the exit status.
    int Run (const std::vector<std::string> & lines)
    {
        HRESULT  hr = S_OK;



        options.commands = lines;
        hr = runner.Prepare (options, result);

        if (SUCCEEDED (hr))
        {
            Place();
            runner.Execute (options, "", result);
        }

        return result.exitStatus;
    }

    Cpu6502Registers Registers()
    {
        return runner.GetHost()->GetCpu()->GetCpu6502()->GetRegisters();
    }

    Byte Peek (Word address)
    {
        return runner.GetHost()->GetMemoryBus().ReadByte (address);
    }

    //  The output against the lines expected, reported by the first line
    //  that differs.
    void AssertOutput (const std::vector<std::string> & expected)
    {
        std::string  wanted = RunText::Join (expected);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (wanted, result.output,
                                                                          RunText::Widen ("--- whole output ---\n" + result.output).c_str());
    }

    //  The JSON records the run printed, parsed.
    std::vector<JsonValue> Records()
    {
        std::vector<JsonValue>  records;
        std::istringstream      stream (result.output);
        std::string             line;



        while (std::getline (stream, line))
        {
            records.push_back (RunText::Parse (line));
        }

        return records;
    }

    std::vector<JsonValue> RecordsOfType (const std::string & type)
    {
        std::vector<JsonValue>  matching;
        std::string             each;



        for (const JsonValue & record : Records())
        {
            if (record.HasString ("type", each) && each == type)
            {
                matching.push_back (record);
            }
        }

        return matching;
    }

private:
    void Place()
    {
        MachineHost       * host      = runner.GetHost();
        Cpu6502Registers    registers = host->GetCpu()->GetCpu6502()->GetRegisters();



        for (const auto & [address, bytes] : m_loads)
        {
            for (size_t i = 0; i < bytes.size(); ++i)
            {
                host->GetMemoryBus().WriteByte ((Word) (address + i), bytes[i]);
            }
        }

        if (m_start.has_value())
        {
            registers.pc = *m_start;
            host->GetCpu()->GetCpu6502()->SetRegisters (registers);
        }
    }

    std::vector<std::pair<Word, std::vector<Byte>>>  m_loads;
    std::optional<Word>                              m_start;
};
