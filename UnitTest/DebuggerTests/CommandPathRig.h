#pragma once

#include "ControllerRig.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugHandlerSet.h"
#include "HandlerTestRig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandPath
//
//  The ways a person reaches a register or memory command: the debugger
//  window's console, a client on the debug channel, and a batch script.
//
////////////////////////////////////////////////////////////////////////////////

enum class CommandPath
{
    WindowConsole,
    Channel,
    Batch,
};





////////////////////////////////////////////////////////////////////////////////
//
//  CommandPathRig
//
//  One real, power-cycled //e reached through one of the three paths, each
//  built the way production builds it:
//
//    window console   DebuggerController, DebuggerViewState::ExecuteWindowLine,
//                     runs driven by the CpuManagerRunDriver slice by slice
//    channel          the same controller, a request sent on the in-memory
//                     transport and pumped, the reply read back from its JSON
//    batch            every command family on a session over a
//                     SynchronousRunDriver, as DebugBatchRunner builds it
//
//  A line runs in a dialect the way that path runs one: the window and batch
//  switch the session's mode first, as MODE does, and the channel names the
//  mode in the request, which applies to that line alone.
//
////////////////////////////////////////////////////////////////////////////////

class CommandPathRig
{
public:
    static constexpr CommandPath  kAllPaths[]   = { CommandPath::WindowConsole, CommandPath::Channel, CommandPath::Batch };
    static constexpr uint64_t     kBatchBudget  = 2000000;
    static constexpr uint32_t     kSliceCycles  = 1023;
    static constexpr int          kMaxSlices    = 4000;
    static constexpr int          kDetailIndent = 7;

    //  The window and the channel reach the //e ControllerRig builds; another
    //  machine, or one with cards, is reached through batch.
    explicit CommandPathRig (CommandPath path, const std::string & machineId = "Apple2e", TestMachine::Slots slots = TestMachine::Slots::Empty) :
        m_path (path)
    {
        if (path == CommandPath::Batch)
        {
            m_batch = std::make_unique<BatchParts> (machineId, slots);
            m_batch->machine.PowerCycle();
            m_batch->target.SetRunDriver (&m_batch->driver);
            m_batch->handlers.Attach (m_batch->session);
            m_batch->session.SetFileSystem (&m_batch->files);
            m_batch->session.SetCallRecording (true);
            m_batch->session.SetBudget (kBatchBudget);
            return;
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (machineId == "Apple2e" && slots == TestMachine::Slots::Empty,
                                                                        L"the window and the channel are reached on the //e alone");

        m_window = std::make_unique<ControllerRig>();
        m_window->machine.PowerCycle();

        if (path == CommandPath::Channel)
        {
            HRESULT  hr = m_window->controller.Open();



            Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (SUCCEEDED (hr), L"the channel opened");
            m_connection = m_window->transport.Connect();
            m_window->controller.Pump();
        }
    }



    const wchar_t * GetName() const
    {
        return m_path == CommandPath::WindowConsole ? L"window console"
             : m_path == CommandPath::Channel       ? L"channel"
             :                                        L"batch";
    }

    CommandPath  GetPath() const { return m_path; }

    MachineHost & GetMachine()
    {
        return m_batch ? (MachineHost &) m_batch->machine : (MachineHost &) m_window->machine;
    }

    DebugSession & GetSession()
    {
        return m_batch ? m_batch->session : m_window->controller.GetSession();
    }

    InMemoryFileSystem & GetFiles()
    {
        return m_batch ? m_batch->files : m_window->files;
    }

    //  The emulator's run path, for tests that only it can reach.
    ControllerRig & GetWindow()
    {
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (m_window != nullptr, L"this path has no window");
        return *m_window;
    }



    //  One line, run and formatted as this path runs it.
    Reply Run (const std::string & line, CommandMode mode = CommandMode::AppleWin)
    {
        Reply  reply;



        if (m_path == CommandPath::Channel)
        {
            return RunOnChannel (line, mode);
        }

        SetMode (mode);

        if (m_path == CommandPath::Batch)
        {
            reply = m_batch->session.ExecuteLine (line);
            m_batch->session.FormatReply (reply);
            return reply;
        }

        return m_window->view.ExecuteWindowLine (m_window->controller.GetSession(), line, mode);
    }



    Reply RunOk (const std::string & line, CommandMode mode = CommandMode::AppleWin)
    {
        Reply  reply = Run (line, mode);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status,
                                                                         Where (line + ": " + reply.error.label + ", " + reply.error.detail).c_str());
        return reply;
    }



    //  An error, its label and its detail, and the text every AppleWin-format
    //  error prints: the label, then the detail indented under it.
    Reply RunFails (const std::string & line, const std::string & label, const std::string & detail, CommandMode mode = CommandMode::AppleWin)
    {
        Reply  reply = Run (line, mode);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, Where (line).c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (label,  reply.error.label,  Where (line).c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (detail, reply.error.detail, Where (line).c_str());
        return reply;
    }



    //  The whole of a reply's text, one line to an element.
    void AssertText (const Reply & reply, std::initializer_list<std::string> expected, const std::string & context)
    {
        std::vector<std::string>  lines (expected);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (Join (lines), Join (reply.text), Where (context).c_str());
    }



    //  The emulator's runs go on after the line that started them; this plays
    //  the CPU thread's part until the run stops. A batch run is already over.
    void FinishRun()
    {
        int  slices = 0;



        if (m_path == CommandPath::Batch)
        {
            return;
        }

        while (m_window->controller.GetRunDriver().IsRunning() && slices < kMaxSlices)
        {
            m_window->controller.GetRunDriver().OnSliceExecuted ((uint32_t) m_window->machine.RunCycles (kSliceCycles));
            ++slices;
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsFalse (m_window->controller.GetRunDriver().IsRunning(),
                                                                        Where ("the run stopped").c_str());

        if (m_path == CommandPath::Channel)
        {
            m_window->controller.Pump();
        }
    }



    //  Test data: bytes the program under test starts with, written as the CPU
    //  would write them, and the registers it starts with.
    void Store (Word address, std::initializer_list<Byte> bytes)
    {
        for (Byte b : bytes)
        {
            GetMachine().GetMemoryBus().WriteByte (address++, b);
        }
    }

    Byte Read (Word address)
    {
        return GetMachine().GetMemoryBus().ReadByte (address);
    }

    void SetRegisters (Word pc, Byte a, Byte x, Byte y, Byte p, Byte sp)
    {
        Cpu6502Registers  registers = { pc, a, x, y, sp, p };



        GetSession().GetTarget().SetRegisters (registers);
    }

    Cpu6502Registers GetRegisters()
    {
        return GetSession().GetTarget().GetRegisters();
    }



    //  The channel's last reply record, for its data.
    const JsonValue & GetLastRecord() const { return m_lastRecord; }

    //  Every record the channel sent this client, replies and notifications.
    const std::vector<std::string> & GetRecords() const
    {
        return m_window->transport.Written (m_connection);
    }

    std::wstring Where (const std::string & context) const
    {
        std::wstring  wide (context.begin(), context.end());



        return std::wstring (GetName()) + L": " + wide;
    }

    static std::string Join (const std::vector<std::string> & lines)
    {
        std::string  joined;



        for (const std::string & line : lines)
        {
            joined += line + "\n";
        }

        return joined;
    }

    //  As MODE does, and as batch mode's --mode does before the first line.
    void SetMode (CommandMode mode)
    {
        DebugCommand  command;



        if (GetSession().GetMode() == mode)
        {
            return;
        }

        command.verb       = DebugVerb::SetMode;
        command.sourceName = "MODE";
        command.mode       = mode;
        GetSession().Execute (command);
    }

private:
    struct BatchParts
    {
        TestMachine                machine;
        MachineDebugTarget         target;
        SynchronousRunDriver       driver;
        RecordingNotificationSink  sink;
        InMemoryFileSystem         files;
        DebugSession               session;
        DebugHandlerSet            handlers;

        BatchParts (const std::string & machineId, TestMachine::Slots slots) :
            machine (machineId, slots),
            target  (machine),
            driver  (machine, target.GetRunHook()),
            session (target, sink, RunState::Paused)
        {
        }
    };

    Reply RunOnChannel (const std::string & line, CommandMode mode)
    {
        std::vector<std::pair<std::string, JsonValue>>  request;
        JsonWriter::Options                             options;
        std::string                                     text;
        size_t                                          before = GetRecords().size();
        Reply                                           reply;
        HRESULT                                         hr     = S_OK;



        request.emplace_back ("type", JsonValue (std::string ("command")));
        request.emplace_back ("id",   JsonValue ((double) ++m_nextId));
        request.emplace_back ("line", JsonValue (line));
        request.emplace_back ("mode", JsonValue (std::string (CommandModeNames::GetName (mode))));
        options.fPretty = false;

        hr = JsonWriter::Write (JsonValue (std::move (request)), options, text);
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (SUCCEEDED (hr), L"the request was written");

        m_window->transport.Send (m_connection, text);
        m_window->controller.Pump();

        for (size_t i = before; i < GetRecords().size(); ++i)
        {
            JsonValue       value;
            JsonParseError  error;
            std::string     type;



            hr = JsonParser::Parse (GetRecords()[i], value, error);
            Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (SUCCEEDED (hr), Where (GetRecords()[i]).c_str());

            if (value.HasString ("type", type) && type == "reply")
            {
                m_lastRecord = std::move (value);
                return ToReply (m_lastRecord);
            }
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail (Where (line + ": no reply record").c_str());
        return reply;
    }

    static Reply ToReply (const JsonValue & record)
    {
        static constexpr const char * kStatuses[] = { "ok", "error", "notAvailable", "unknown" };
        Reply                         reply;
        std::string                   status;
        const JsonValue             * text  = nullptr;
        const JsonValue             * error = nullptr;



        record.HasString ("command", reply.command);
        record.HasString ("status",  status);

        for (int i = 0; i < (int) std::size (kStatuses); ++i)
        {
            if (status == kStatuses[i])
            {
                reply.status = (CommandStatus) i;
            }
        }

        if (record.HasArray ("text", text))
        {
            for (size_t i = 0; i < text->GetArraySize(); ++i)
            {
                reply.text.push_back (text->GetArrayElement (i).GetString());
            }
        }

        if (record.HasObject ("error", error))
        {
            error->HasString ("label",  reply.error.label);
            error->HasString ("detail", reply.error.detail);
        }

        return reply;
    }

    CommandPath                     m_path;
    std::unique_ptr<ControllerRig>  m_window;
    std::unique_ptr<BatchParts>     m_batch;
    ChannelConnectionId             m_connection = 0;
    int64_t                         m_nextId     = 0;
    JsonValue                       m_lastRecord;
};
