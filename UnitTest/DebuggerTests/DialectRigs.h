#pragma once

#include "Cli/DebugBatchRunner.h"
#include "Cli/DebugBatchSink.h"
#include "ControllerRig.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/ReplyJson.h"
#include "Debugger/SynchronousRunDriver.h"
#include "EmuTests/FixtureRomSource.h"
#include "EmuTests/TestMachine.h"
#include "UiTests/InMemoryFileSystem.h"

#include "CppUnitTest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DialectCheck
//
//  Assertions the Monitor and GSSquared dialect tests share: whole lines
//  compared one by one, with the whole of both sides in the message, and an
//  error checked for its status, its label and its detail.
//
////////////////////////////////////////////////////////////////////////////////

class DialectCheck
{
public:
    using Lines = std::vector<std::string>;

    static std::wstring Widen (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }

    static std::string Join (const Lines & lines)
    {
        std::string  text;



        for (const std::string & line : lines)
        {
            text += "[" + line + "]\n";
        }

        return text;
    }

    static void AreLines (const Lines & expected, const Lines & actual, const std::string & what)
    {
        std::string  both = what + "\n--- expected ---\n" + Join (expected) + "--- actual ---\n" + Join (actual);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (expected.size(), actual.size(), Widen (both).c_str());

        for (size_t i = 0; i < expected.size(); ++i)
        {
            Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (Widen (expected[i]), Widen (actual[i]), Widen (both).c_str());
        }
    }

    //  An error reply as the session returns it, before any formatter.
    static void IsError (const Reply & reply, CommandStatus status, const std::string & label, const std::string & detail, const std::string & what)
    {
        std::string  shown = what + ": " + reply.error.label + " / " + reply.error.detail;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) status, (int) reply.status, Widen (shown).c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (Widen (label),  Widen (reply.error.label),  Widen (shown).c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (Widen (detail), Widen (reply.error.detail), Widen (shown).c_str());
    }

    static void IsOk (const Reply & reply, const std::string & what)
    {
        std::string  shown = what + ": " + reply.error.label + " / " + reply.error.detail;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, Widen (shown).c_str());
    }

    //  An error as the Monitor's output format prints it: its ERR, then the
    //  two-line reason.
    static Lines MonitorError (const std::string & label, const std::string & detail)
    {
        return { "ERR", "Error: " + label, "       " + detail };
    }

    //  An error as every other output format prints it.
    static Lines AppleWinError (const std::string & label, const std::string & detail)
    {
        return { "Error: " + label, "       " + detail };
    }

    //  Text split into lines, a trailing newline ending the last rather than
    //  starting another.
    static void AppendLines (const std::string & text, Lines & lines)
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
};





////////////////////////////////////////////////////////////////////////////////
//
//  DialectSink
//
//  Batch mode's own sink, so a stop prints as `CassoCli debug` prints it,
//  and a record of every stop and mode change for a test to read.
//
////////////////////////////////////////////////////////////////////////////////

class DialectSink : public IDebugNotificationSink
{
public:
    DebugBatchSink            batch;
    std::vector<StopEvent>    stops;
    std::vector<CommandMode>  modes;

    void OnStopped        (const StopEvent & stop) override    { stops.push_back (stop); batch.OnStopped (stop); }
    void OnResumed        () override                          { batch.OnResumed(); }
    void OnReset          (bool isPowerCycle) override         { batch.OnReset (isPowerCycle); }
    void OnMachineChanged (const std::string & name) override  { batch.OnMachineChanged (name); }
    void OnModeChanged    (CommandMode mode) override          { modes.push_back (mode); batch.OnModeChanged (mode); }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DialectSessionRig
//
//  The session `CassoCli debug` builds, over a real Apple II run
//  synchronously: every command family, the call record on, batch's sink,
//  and a budget so no run can go on forever. Nothing is set by hand that the
//  batch runner does not set; the power cycle leaves the registers where
//  the reset put them. The mode is reached by typing MODE, as a script does.
//
//  Lines returns what batch prints for a line after its prompt: the reply's
//  text, then any stop the line caused.
//
////////////////////////////////////////////////////////////////////////////////

class DialectSessionRig
{
public:
    static constexpr uint32_t  kBudget = 200000;

    TestMachine           machine;
    MachineDebugTarget    target;
    SynchronousRunDriver  driver;
    DialectSink           sink;
    InMemoryFileSystem    files;
    DebugSession          session;
    DebugHandlerSet       handlers;
    std::string           stopText;

    explicit DialectSessionRig (CommandMode mode = CommandMode::AppleWin, const std::string & machineId = "Apple2e") :
        machine (machineId, TestMachine::Slots::Empty),
        target  (machine),
        driver  (machine, target.GetRunHook()),
        session (target, sink, RunState::Paused)
    {
        target.SetRunDriver (&driver);
        machine.PowerCycle();

        handlers.Attach       (session);
        session.SetFileSystem (&files);
        session.SetCallRecording (true);
        sink.batch.SetSession (&session);

        DialectCheck::IsOk (Run (std::format ("BUDGET {}", kBudget)), "BUDGET");

        if (mode != CommandMode::AppleWin)
        {
            DialectCheck::IsOk (Run ("MODE " + CommandModeNames::GetUpperName (mode)), "MODE");
        }

        sink.stops.clear();
        sink.modes.clear();
    }

    ~DialectSessionRig()
    {
        sink.batch.SetSession (nullptr);
    }

    DialectSessionRig             (const DialectSessionRig &) = delete;
    DialectSessionRig & operator= (const DialectSessionRig &) = delete;

    //  The line as batch runs it, formatted in the session's output format.
    Reply Run (const std::string & line)
    {
        Reply  reply = session.ExecuteLine (line);



        session.FormatReply (reply);
        stopText = sink.batch.TakePending();
        return reply;
    }

    //  The same, in a mode given for this line alone, formatted as the
    //  channel formats such a line.
    Reply Run (const std::string & line, CommandMode mode)
    {
        Reply  reply = session.ExecuteLine (line, mode);



        session.FormatReply (reply, mode);
        stopText = sink.batch.TakePending();
        return reply;
    }

    DialectCheck::Lines Lines (const std::string & line)
    {
        Reply                reply = Run (line);
        DialectCheck::Lines  lines = reply.text;



        DialectCheck::AppendLines (stopText, lines);
        return lines;
    }

    Reply Ok (const std::string & line)
    {
        Reply  reply = Run (line);



        DialectCheck::IsOk (reply, line);
        return reply;
    }

    void Expect (const std::string & line, const DialectCheck::Lines & expected)
    {
        DialectCheck::AreLines (expected, Lines (line), line);
    }

    Byte Peek (Word address)
    {
        Byte  value = 0;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (target.TryPeek (address, value), L"readable");
        return value;
    }

    std::vector<Byte> PeekRange (Word first, Word last)
    {
        std::vector<Byte>  bytes;



        for (uint32_t address = first; address <= last; ++address)
        {
            bytes.push_back (Peek ((Word) address));
        }

        return bytes;
    }

    Cpu6502Registers Registers() const
    {
        return target.GetRegisters();
    }

    const StopEvent & LastStop()
    {
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsFalse (sink.stops.empty(), L"no stop was delivered");
        return sink.stops.back();
    }

    std::string File (const std::wstring & path)
    {
        std::string  content;
        HRESULT      hr      = files.ReadAllText (path, content);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, (L"no file " + path).c_str());
        return content;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DialectBatchRig
//
//  `CassoCli debug` itself: DebugBatchRunner over the fixture ROMs, a script
//  of lines, and the text or JSON it prints, prompts and stops included.
//
////////////////////////////////////////////////////////////////////////////////

class DialectBatchRig
{
public:
    static constexpr uint64_t  kMaxCycles = 200000;

    FixtureRomSource                  roms;
    InMemoryFileSystem                files;
    CommandLineOptions::DebugOptions  options;
    DebugBatchResult                  result;

    DialectBatchRig()
    {
        options.machine   = "Apple2e";
        options.maxCycles = kMaxCycles;
    }

    int Run (const DialectCheck::Lines & lines)
    {
        DebugBatchRunner  runner (roms, files);
        std::string       script;
        HRESULT           hr     = S_OK;



        for (const std::string & line : lines)
        {
            script += line + "\n";
        }

        hr = runner.Run (options, script, result);
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, DialectCheck::Widen (result.diagnostics).c_str());
        return result.exitStatus;
    }

    DialectCheck::Lines Output() const
    {
        DialectCheck::Lines  lines;



        DialectCheck::AppendLines (result.output, lines);
        return lines;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DialectWindowRig
//
//  The debugger window and a channel client over one running emulator: the
//  controller over a real machine, its channel open with one client
//  connected, a line typed into the console reaching the session through
//  DebuggerViewState::ExecuteWindowLine, and a run carried out by the
//  CpuManager run driver the frame loop drives.
//
//  THE FRAME LOOP IS STOOD IN FOR by Drive, which runs the machine a slice
//  at a time and reports each slice to the run driver, as ExecuteCpuSlices
//  does; EmulatorShell cannot run in a test. Everything the driver decides
//  is the production code's.
//
////////////////////////////////////////////////////////////////////////////////

class DialectWindowRig : public ControllerRig
{
public:
    static constexpr uint32_t  kSlice     = 1023;
    static constexpr int       kMaxSlices = 2000;

    ChannelConnectionId  client  = 0;
    size_t               read    = 0;

    explicit DialectWindowRig (CommandMode mode = CommandMode::AppleWin)
    {
        HRESULT  hr = controller.Open();



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, L"the channel opened");

        client = transport.Connect();
        controller.Pump();

        if (mode != CommandMode::AppleWin)
        {
            DialectCheck::IsOk (Window ("MODE " + CommandModeNames::GetUpperName (mode)), "MODE");
        }
    }

    DebugSession & Session()
    {
        return controller.GetSession();
    }

    //  A line typed into the console, in the session's mode.
    Reply Window (const std::string & line)
    {
        return view.ExecuteWindowLine (Session(), line, Session().GetMode());
    }

    //  A channel request's line, as DebugChannelServer hands it on.
    Reply Pipe (const std::string & line, std::optional<CommandMode> mode = {}, std::optional<uint64_t> budget = {})
    {
        return controller.RunLine (line, mode, budget);
    }

    bool IsRunning()
    {
        return controller.GetRunDriver().IsRunning();
    }

    //  The run in progress, a slice at a time, until the driver ends it.
    //  False when it was still going after kMaxSlices slices.
    bool Drive()
    {
        for (int slice = 0; slice < kMaxSlices && IsRunning(); ++slice)
        {
            uint64_t  ran = machine.RunCycles (kSlice);



            if (controller.GetRunDriver().OnSliceExecuted ((uint32_t) ran))
            {
                return true;
            }
        }

        return !IsRunning();
    }

    //  The records the channel sent the client since the last call, after a
    //  pump flushes what is queued.
    DialectCheck::Lines TakeRecords()
    {
        const std::vector<std::string> & written = transport.Written (client);
        DialectCheck::Lines              records;



        controller.Pump();

        for (; read < written.size(); ++read)
        {
            records.push_back (written[read]);
        }

        return records;
    }

    //  The one `stopped` record the last run sent.
    std::string TakeStop()
    {
        DialectCheck::Lines  stops;



        for (const std::string & record : TakeRecords())
        {
            if (record.find ("\"type\":\"stopped\"") != std::string::npos)
            {
                stops.push_back (record);
            }
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (size_t (1), stops.size(), DialectCheck::Widen (DialectCheck::Join (stops)).c_str());
        return stops.front();
    }

    Byte Peek (Word address)
    {
        Byte  value = 0;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (Session().GetTarget().TryPeek (address, value), L"readable");
        return value;
    }

    Cpu6502Registers Registers()
    {
        return Session().GetTarget().GetRegisters();
    }
};
