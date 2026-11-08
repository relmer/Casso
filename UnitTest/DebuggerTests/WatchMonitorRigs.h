#pragma once

#include "Cli/DebugBatchRunner.h"
#include "ControllerRig.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/DebugMemoryView.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/ReplyJson.h"
#include "Debugger/SynchronousRunDriver.h"
#include "EmuTests/FixtureRomSource.h"
#include "EmuTests/TestMachine.h"
#include "HandlerTestRig.h"
#include "UiTests/InMemoryFileSystem.h"

#include "CppUnitTest.h"





namespace WatchMonitorTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Check
    //
    //  The assertions every rig below shares: a reply's status, and an error's
    //  label AND detail, which is the whole of what a reader is told.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class Check
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }

        static std::string Join (const std::vector<std::string> & lines)
        {
            std::string  text;

            for (const std::string & line : lines)
            {
                text += line;
                text += '\n';
            }

            return text;
        }

        static void IsOk (const Reply & reply, const std::string & line)
        {
            std::string  why = line + ": " + reply.error.label + ", " + reply.error.detail + "\n" + Join (reply.text);

            Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, Widen (why).c_str());
        }

        static void Fails (const Reply & reply, const std::string & line, CommandStatus status, const std::string & label, const std::string & detail)
        {
            std::string  why = line + "\n" + Join (reply.text);

            Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) status, (int) reply.status, Widen (why).c_str());
            Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (label,  reply.error.label,  Widen (why).c_str());
            Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (detail, reply.error.detail, Widen (why).c_str());
        }

        //  The reply's whole text, one line per element, so a missing or an
        //  extra line fails as surely as a wrong one.
        static void TextIs (const std::vector<std::string> & expected, const Reply & reply, const std::string & line)
        {
            Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (Join (expected), Join (reply.text), Widen (line).c_str());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SessionRig
    //
    //  The batch session over a real machine, set up the way DebugBatchRunner
    //  sets up its own: a power-cycled machine run synchronously, every
    //  command family, a file system, the call record on, and a cycle budget.
    //  No current directory is set, because production sets none until CD.
    //
    //  Run renders each reply in the session's output format, as the runner
    //  prints it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class SessionRig
    {
    public:
        TestMachine                machine;
        MachineDebugTarget         target;
        SynchronousRunDriver       driver;
        RecordingNotificationSink  sink;
        InMemoryFileSystem         files;
        DebugSession               session;
        DebugHandlerSet            handlers;

        explicit SessionRig (const std::string & machineId = "Apple2e") :
            machine (machineId, TestMachine::Slots::Empty),
            target  (machine),
            driver  (machine, target.GetRunHook()),
            session (target, sink, RunState::Paused)
        {
            target.SetRunDriver (&driver);
            machine.PowerCycle();
            handlers.Attach (session);
            session.SetFileSystem (&files);
            session.SetCallRecording (true);
            session.ExecuteLine ("BUDGET 2000000", CommandMode::AppleWin);
        }

        Reply Run (const std::string & line)
        {
            Reply  reply = session.ExecuteLine (line);

            session.FormatReply (reply);
            return reply;
        }

        Reply RunOk (const std::string & line)
        {
            Reply  reply = Run (line);

            Check::IsOk (reply, line);
            return reply;
        }

        //  The reply's text, asserted whole.
        Reply RunIs (const std::string & line, const std::vector<std::string> & expected)
        {
            Reply  reply = RunOk (line);

            Check::TextIs (expected, reply, line);
            return reply;
        }

        Reply RunFails (const std::string & line, const std::string & label, const std::string & detail)
        {
            Reply  reply = Run (line);

            Check::Fails (reply, line, CommandStatus::Error, label, detail);
            return reply;
        }

        //  The record `--json` prints for the line.
        std::string Json (const std::string & line)
        {
            Reply  reply = Run (line);

            return ReplyJson::WriteReply (reply, std::nullopt);
        }

        void Poke (Word address, std::initializer_list<Byte> bytes)
        {
            Word  at = address;

            for (Byte value : bytes)
            {
                Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (target.TryPoke (at, value),
                                                                                 Check::Widen (std::format ("${:04X} is not writable", at)).c_str());
                ++at;
            }
        }

        Byte Peek (Word address)
        {
            Byte  value = 0;

            Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (target.TryPeek (address, value),
                                                                             Check::Widen (std::format ("${:04X} is not readable", address)).c_str());
            return value;
        }

        Word PeekWord (Word address)
        {
            return (Word) (Peek (address) | (Peek ((Word) (address + 1)) << 8));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WindowRig
    //
    //  The debugger inside the emulator: ControllerRig's controller over a real
    //  machine, driven two ways a user drives it.
    //
    //  Type is a line typed into the console, which the emulator runs through
    //  ExecuteWindowLine in the session's own mode, as EmulatorShell does.
    //  Request is a line a channel client sends over the pipe, pumped through
    //  the server; it returns the JSON record the client received.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class WindowRig
    {
    public:
        ControllerRig        rig;
        ChannelConnectionId  connection = 0;
        int64_t              nextId     = 1;

        DebugSession & Session() { return rig.controller.GetSession(); }

        Reply Type (const std::string & line)
        {
            return rig.view.ExecuteWindowLine (Session(), line, Session().GetMode());
        }

        Reply TypeOk (const std::string & line)
        {
            Reply  reply = Type (line);

            Check::IsOk (reply, line);
            return reply;
        }

        Reply TypeIs (const std::string & line, const std::vector<std::string> & expected)
        {
            Reply  reply = TypeOk (line);

            Check::TextIs (expected, reply, line);
            return reply;
        }

        //  One request, and the reply record it drew. A mode, when given, is
        //  the request's own and applies to this line alone.
        std::string Request (const std::string & line, const std::string & mode = std::string())
        {
            std::string  request = std::format (R"({{"type":"command","id":{},"line":"{}")", nextId++, Escape (line));
            size_t       before  = 0;

            Connect();

            if (!mode.empty())
            {
                request += std::format (R"(,"mode":"{}")", mode);
            }

            request += "}";
            before   = rig.transport.Written (connection).size();

            rig.transport.Send (connection, request);
            rig.controller.Pump();

            for (size_t i = before; i < rig.transport.Written (connection).size(); ++i)
            {
                if (rig.transport.Written (connection)[i].starts_with (R"({"type":"reply")"))
                {
                    return rig.transport.Written (connection)[i];
                }
            }

            Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail (Check::Widen ("no reply to " + request).c_str());
            return std::string();
        }

        //  The id the next request will carry, so a test can write the record
        //  it expects.
        int64_t GetNextId() const { return nextId; }

        //  What the emulator's frame loop does for a run the controller
        //  started: slices of the machine, each reported to the run driver,
        //  until the run ends or the ceiling is reached.
        void RunSlices (uint64_t maxCycles)
        {
            static constexpr uint64_t  kSlice = 1000;
            uint64_t                   spent  = 0;



            while (spent < maxCycles && rig.controller.GetRunDriver().IsRunning())
            {
                uint64_t  ran = rig.machine.RunCycles (kSlice);



                rig.controller.GetRunDriver().OnSliceExecuted ((uint32_t) ran);
                spent += kSlice;
            }
        }

        //  A pause from the window's Pause, ended at the next slice report as
        //  the frame loop would end it.
        void Pause()
        {
            rig.controller.RequestPause();
            rig.controller.GetRunDriver().OnSliceExecuted (0);
        }

        //  Everything the client was sent, one record per element.
        const std::vector<std::string> & Received()
        {
            rig.controller.Pump();
            return rig.transport.Written (connection);
        }

        Byte Peek (Word address)
        {
            Byte  value = 0;

            Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (Session().GetTarget().TryPeek (address, value),
                                                                             Check::Widen (std::format ("${:04X} is not readable", address)).c_str());
            return value;
        }

    private:
        void Connect()
        {
            HRESULT  hr = S_OK;

            if (connection != 0)
            {
                return;
            }

            hr = rig.controller.Open();
            Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, L"the channel opened");

            connection = rig.transport.Connect();
            rig.controller.Pump();
        }

        static std::string Escape (const std::string & text)
        {
            std::string  escaped;

            for (char ch : text)
            {
                if (ch == '"' || ch == '\\')
                {
                    escaped += '\\';
                }

                escaped += ch;
            }

            return escaped;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ScriptRig
    //
    //  `CassoCli debug` itself: DebugBatchRunner over the fixture ROMs and an
    //  in-memory file system. The runner is kept after the run, so a test can
    //  read the machine the script left behind.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ScriptRig
    {
    public:
        FixtureRomSource                   roms;
        InMemoryFileSystem                 files;
        CommandLineOptions::DebugOptions   options;
        DebugBatchResult                   result;
        std::unique_ptr<DebugBatchRunner>  runner;

        explicit ScriptRig (const std::string & machineId = "Apple2e")
        {
            static constexpr uint64_t  kBudget = 2000000;

            options.machine   = machineId;
            options.maxCycles = kBudget;
        }

        //  The script's whole output.
        std::string Run (const std::string & script)
        {
            HRESULT  hr = S_OK;

            runner = std::make_unique<DebugBatchRunner> (roms, files);
            hr     = runner->Run (options, script, result);

            Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, Check::Widen (result.diagnostics).c_str());
            return result.output;
        }

        Byte Peek (Word address)
        {
            DebugMemoryView  view (*runner->GetHost());
            Byte             value = 0;

            Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (view.TryPeek (address, value),
                                                                             Check::Widen (std::format ("${:04X} is not readable", address)).c_str());
            return value;
        }
    };
}
