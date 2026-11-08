#pragma once

#include "ControllerRig.h"
#include "Core/JsonParser.h"
#include "Debugger/DebugHandlerSet.h"
#include "EmuTests/FixtureProvider.h"
#include "HandlerTestRig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SessionRig
//
//  The batch path as CassoCli debug builds it: a real machine run
//  synchronously, one session with every command family attached through
//  DebugHandlerSet, the call record on, and an in-memory file system. The
//  constructor changes to C:\Work with CD, as a script would.
//
//  Run takes the line's dialect and renders the reply in that dialect's
//  format, as a script line typed in that mode would be.
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

    explicit SessionRig (const std::string & id = "Apple2e") :
        machine (id, TestMachine::Slots::Empty),
        target  (machine),
        driver  (machine, target.GetRunHook()),
        session (target, sink, RunState::Paused)
    {
        target.SetRunDriver      (&driver);
        machine.PowerCycle();
        handlers.Attach          (session);
        session.SetFileSystem    (&files);
        session.SetCallRecording (true);
        RunOk ("CD C:\\Work");
    }

    Reply Run (const std::string & line, CommandMode mode = CommandMode::AppleWin)
    {
        Reply  reply = session.ExecuteLine (line, mode);



        session.FormatReply (reply, mode);
        return reply;
    }

    Reply RunOk (const std::string & line, CommandMode mode = CommandMode::AppleWin)
    {
        Reply         reply = Run (line, mode);
        std::string   why   = line + ": " + reply.error.label + ", " + reply.error.detail;
        std::wstring  where (why.begin(), why.end());



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, where.c_str());
        return reply;
    }

    //  The label and the exact detail of a failure.
    Reply RunFails (const std::string & line, const std::string & label, const std::string & detail, CommandMode mode = CommandMode::AppleWin)
    {
        Reply         reply = Run (line, mode);
        std::wstring  where (line.begin(), line.end());



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreNotEqual ((int) CommandStatus::Ok, (int) reply.status, where.c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (label,  reply.error.label,  where.c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (detail, reply.error.detail, where.c_str());
        return reply;
    }

    //  Bytes into memory through the side-effect-free view.
    void Poke (Word address, std::initializer_list<Byte> bytes)
    {
        for (Byte b : bytes)
        {
            target.TryPoke (address++, b);
        }
    }

    Byte Peek (Word address)
    {
        Byte  value = 0;



        target.TryPeek (address, value);
        return value;
    }

    Word GetPc()
    {
        return target.GetRegisters().pc;
    }

    const StopEvent & LastStop()
    {
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsFalse (sink.stops.empty(), L"no stop was delivered");
        return sink.stops.back();
    }

    //  A file under UnitTest/Fixtures, copied into the in-memory file system at
    //  C:\Work\<name>.
    void CopyFixture (const std::string & relativePath, const std::wstring & name)
    {
        FixtureProvider       provider;
        std::vector<uint8_t>  bytes;
        HRESULT               hr = provider.OpenFixture (relativePath, bytes);
        std::wstring          where (relativePath.begin(), relativePath.end());



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, where.c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsFalse (bytes.empty(), where.c_str());
        files.WriteAllText (L"C:\\Work\\" + name, std::string (bytes.begin(), bytes.end()));
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  WindowRun
//
//  A run started in the window, or by a pipe client, executes on the CPU
//  thread one slice at a time through CpuManagerRunDriver. Finish does what
//  the frame loop does: runs slices and reports each to the driver until the
//  run ends, or fails when it has not ended within the slice limit.
//
////////////////////////////////////////////////////////////////////////////////

class WindowRun
{
public:
    static constexpr uint64_t  kSliceCycles = 1023;
    static constexpr int       kMaxSlices   = 2000;

    static void Finish (ControllerRig & rig, int maxSlices = kMaxSlices)
    {
        int  slices = 0;



        while (rig.controller.GetRunDriver().IsRunning() && slices < maxSlices)
        {
            uint64_t  ran = rig.machine.RunCycles (kSliceCycles);



            rig.controller.GetRunDriver().OnSliceExecuted ((uint32_t) ran);
            ++slices;
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsFalse (rig.controller.GetRunDriver().IsRunning(), L"the run did not end within the slice limit");
    }

    //  Runs a number of slices without requiring the run to end.
    static void RunSlices (ControllerRig & rig, int slices)
    {
        for (int i = 0; i < slices && rig.controller.GetRunDriver().IsRunning(); ++i)
        {
            uint64_t  ran = rig.machine.RunCycles (kSliceCycles);



            rig.controller.GetRunDriver().OnSliceExecuted ((uint32_t) ran);
        }
    }

    //  A line typed in the window's console, then the run it started, to its end.
    static Reply Run (ControllerRig & rig, const std::string & line, CommandMode mode = CommandMode::AppleWin)
    {
        Reply  reply = rig.Run (line, mode);



        Finish (rig);
        return reply;
    }

    static Word GetPc (ControllerRig & rig)
    {
        return rig.controller.GetSession().GetTarget().GetRegisters().pc;
    }

    //  A file under UnitTest/Fixtures, copied into the rig's file system.
    static void CopyFixture (ControllerRig & rig, const std::string & relativePath, const std::wstring & path)
    {
        FixtureProvider       provider;
        std::vector<uint8_t>  bytes;
        HRESULT               hr = provider.OpenFixture (relativePath, bytes);
        std::wstring          where (relativePath.begin(), relativePath.end());



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, where.c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsFalse (bytes.empty(), where.c_str());
        rig.files.WriteAllText (path, std::string (bytes.begin(), bytes.end()));
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  PipeClient
//
//  One client of the debug channel on a ControllerRig: opens the channel,
//  connects, and sends command requests as a client writes them, then pumps
//  the server as the CPU thread does each frame. Each request's records --
//  its reply and any notification that went out with it -- come back parsed.
//
////////////////////////////////////////////////////////////////////////////////

class PipeClient
{
public:
    explicit PipeClient (ControllerRig & rig) :
        m_rig (rig)
    {
        HRESULT  hr = rig.controller.Open();



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, L"the channel opened");
        m_connection = rig.transport.Connect();
    }

    //  A command request, in the session's mode or the mode given ("windbg",
    //  "monitor", ...), and the reply record the client got for it.
    JsonValue Send (const std::string & line, const std::string & mode = std::string())
    {
        std::string  request = std::format (R"({{"type":"command","id":{},"line":"{}")", ++m_nextId, Escape (line));
        size_t       before  = m_rig.transport.Written (m_connection).size();



        request += mode.empty() ? std::string ("}") : std::format (R"(,"mode":"{}"}})", mode);
        m_rig.transport.Send (m_connection, request);
        m_rig.controller.Pump();

        return FindRecord (before, "reply");
    }

    //  Pumps the server so a notification raised since is written, and gives
    //  the newest record of that type the client has received.
    JsonValue TakeNotification (const std::string & type)
    {
        m_rig.controller.Pump();
        return FindRecord (0, type);
    }

    //  The reply's text lines.
    static std::vector<std::string> GetText (const JsonValue & record)
    {
        std::vector<std::string>  lines;
        const JsonValue         * text = nullptr;



        if (record.HasArray ("text", text))
        {
            for (size_t i = 0; i < text->GetArraySize(); ++i)
            {
                lines.push_back (text->GetArrayElement (i).GetString());
            }
        }

        return lines;
    }

    //  True when the record has the member and it is null.
    static bool IsNullMember (const JsonValue & record, const std::string & key)
    {
        for (const auto & [name, value] : record.GetObjectEntries())
        {
            if (name == key)
            {
                return value.GetType() == JsonType::Null;
            }
        }

        return false;
    }

    static std::string GetStringMember (const JsonValue & record, const std::string & key)
    {
        std::string  value;
        HRESULT      hr = record.GetString (key, value);
        std::wstring where (key.begin(), key.end());



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, where.c_str());
        return value;
    }

private:
    JsonValue FindRecord (size_t from, const std::string & type)
    {
        const std::vector<std::string> & written = m_rig.transport.Written (m_connection);
        JsonValue                         found;
        bool                              isFound = false;



        for (size_t i = from; i < written.size(); ++i)
        {
            JsonValue       root;
            JsonParseError  error;
            std::string     recordType;
            HRESULT         hr = JsonParser::Parse (written[i], root, error);



            Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, L"a record parses");

            if (root.HasString ("type", recordType) && recordType == type)
            {
                found   = root;
                isFound = true;
            }
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (isFound, std::wstring (type.begin(), type.end()).c_str());
        return found;
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

    ControllerRig        & m_rig;
    ChannelConnectionId    m_connection = 0;
    int                    m_nextId     = 0;
};
