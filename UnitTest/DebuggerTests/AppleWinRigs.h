#pragma once

#include "Cli/DebugBatchRunner.h"
#include "ControllerRig.h"
#include "Core/JsonParser.h"
#include "Debugger/DebugHandlerSet.h"
#include "EmuTests/FixtureRomSource.h"
#include "HandlerTestRig.h"

#include "CppUnitTest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBatchRig
//
//  `CassoCli debug` as a user runs it: DebugBatchRunner over the fixture ROMs
//  and an in-memory file system, the script's lines typed as given. The text
//  output is split into one block per line that reached the session, holding
//  its reply and any stop it caused; a JSON run is read as records. The
//  machine stays as the script left it, for a test to read afterward.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWinBatchRig
{
public:
    using Lines = std::vector<std::string>;

    FixtureRomSource                  roms;
    InMemoryFileSystem                files;
    CommandLineOptions::DebugOptions  options;
    DebugBatchResult                  result;
    DebugBatchRunner                  runner;
    Lines                             script;

    AppleWinBatchRig() :
        runner (roms, files)
    {
        options.machine = "Apple2e";
    }

    static std::wstring Widen (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }

    //  The lines as a script; the exit status.
    int Run (const Lines & lines)
    {
        std::string  text;
        HRESULT      hr = S_OK;



        script = lines;

        for (const std::string & line : lines)
        {
            text += line + "\n";
        }

        hr = runner.Run (options, text, result);
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, Widen (result.diagnostics).c_str());
        return result.exitStatus;
    }

    Lines GetOutputLines() const
    {
        std::istringstream  stream (result.output);
        std::string         line;
        Lines               lines;



        while (std::getline (stream, line))
        {
            lines.push_back (line);
        }

        return lines;
    }

    //  What script line `index` printed after its echo: its reply's text, then
    //  whatever it caused. Lines the runner skips (blank, ;) print nothing.
    Lines GetBlock (size_t index) const
    {
        Lines                    output = GetOutputLines();
        std::map<size_t, Lines>  blocks;
        size_t                   next   = 0;
        std::optional<size_t>    current;



        for (const std::string & line : output)
        {
            std::optional<size_t>  echoOf = FindEcho (line, next);



            if (echoOf.has_value())
            {
                current         = *echoOf;
                next            = *echoOf + 1;
                blocks[*echoOf] = Lines();
                continue;
            }

            if (current.has_value())
            {
                blocks[*current].push_back (line);
            }
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (blocks.contains (index),
            Widen (std::format ("line {} ({}) was not echoed\n{}", index, index < script.size() ? script[index] : "?", result.output)).c_str());
        return blocks[index];
    }

    //  The block of line `index`, compared line by line with what it should be.
    void ExpectBlock (size_t index, const Lines & expected) const
    {
        Lines        actual = GetBlock (index);
        std::string  where  = std::format ("line {} ({})\n{}", index, script[index], result.output);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (expected.size(), actual.size(), Widen (where).c_str());

        for (size_t i = 0; i < expected.size(); ++i)
        {
            Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (expected[i], actual[i], Widen (where).c_str());
        }
    }

    //  Every record of a JSON run, parsed.
    std::vector<JsonValue> GetRecords() const
    {
        std::vector<JsonValue>  records;



        for (const std::string & line : GetOutputLines())
        {
            JsonValue       root;
            JsonParseError  error;
            HRESULT         hr = JsonParser::Parse (line, root, error);



            Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, Widen (line).c_str());
            records.push_back (root);
        }

        return records;
    }

    //  The reply records of a JSON run, one per line that reached the session.
    std::vector<JsonValue> GetReplies() const
    {
        std::vector<JsonValue>  replies;
        std::string             type;



        for (const JsonValue & record : GetRecords())
        {
            if (record.HasString ("type", type) && type == "reply")
            {
                replies.push_back (record);
            }
        }

        return replies;
    }

    //  A RAM byte of the machine the script left.
    Byte PeekRam (Word address)
    {
        return runner.GetHost()->GetMemoryBus().ReadByte (address);
    }

    Cpu6502Registers GetRegisters()
    {
        return runner.GetHost()->GetCpu()->GetCpu6502()->GetRegisters();
    }

private:
    static std::string Trim (const std::string & text)
    {
        size_t  first = text.find_first_not_of (" \t\r");
        size_t  last  = text.find_last_not_of  (" \t\r");



        return (first == std::string::npos) ? std::string() : text.substr (first, last - first + 1);
    }

    //  The script line, from `next` on, that this output line echoes: the
    //  first one not skipped, behind any mode's prompt.
    std::optional<size_t> FindEcho (const std::string & line, size_t next) const
    {
        for (size_t j = next; j < script.size(); ++j)
        {
            std::string  typed = Trim (script[j]);



            for (const char * prompt : { ">", "*", "0:000> " })
            {
                if (line == prompt + typed)
                {
                    return j;
                }
            }

            if (!typed.empty() && typed[0] != ';')
            {
                return std::nullopt;
            }
        }

        return std::nullopt;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinConsoleRig
//
//  The debugger window as the shell drives it: the controller opened, a line
//  typed into the console run through ExecuteWindowLine in the session's
//  mode, and the CPU thread's frame loop run slice by slice with the run
//  driver told after each. A channel client is connected, so the records it
//  is sent -- replies to its own requests, stops, resumes -- can be read.
//
//  The machine holds LDA #$41 / STA $0400 / RTS at $0300, with the PC on it
//  and the machine paused.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWinConsoleRig
{
public:
    static constexpr uint32_t  kSliceCycles = 1023;     // the frame loop's slice
    static constexpr int       kMaxSlices   = 4000;

    ControllerRig        rig;
    ChannelConnectionId  client = 0;

    AppleWinConsoleRig()
    {
        HRESULT  hr = rig.controller.Open();



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, L"the debugger opened");

        client = rig.transport.Connect();
        rig.controller.Pump();
    }

    DebugSession & GetSession()
    {
        return rig.controller.GetSession();
    }

    //  A line typed into the console, as the shell runs it.
    Reply Type (const std::string & line)
    {
        return rig.view.ExecuteWindowLine (GetSession(), line, GetSession().GetMode());
    }

    //  The same, and asserts that it succeeded.
    Reply TypeOk (const std::string & line)
    {
        Reply         reply = Type (line);
        std::string   why   = line + ": " + reply.error.label + ", " + reply.error.detail;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, AppleWinBatchRig::Widen (why).c_str());
        return reply;
    }

    //  The CPU thread's frame loop, slice by slice, until the run driver ends
    //  the run, the machine is paused, or kMaxSlices pass. True when a run
    //  ended.
    bool RunSlices (int maxSlices = kMaxSlices)
    {
        for (int i = 0; i < maxSlices && !rig.cpuManager.IsPaused(); ++i)
        {
            uint32_t  actual = (uint32_t) rig.machine.RunCycles (kSliceCycles);



            if (rig.controller.GetRunDriver().OnSliceExecuted (actual))
            {
                return true;
            }

            if (actual == 0)
            {
                return false;
            }
        }

        return false;
    }

    //  The user's resume and pause in the emulator, as the shell reports them.
    void UserResume()
    {
        rig.cpuManager.SetPaused (false);
        GetSession().OnUserResumed();
    }

    void UserPause()
    {
        rig.cpuManager.SetPaused (true);
        rig.controller.GetRunDriver().EndForUserPause();
        GetSession().OnUserPaused();
    }

    //  A channel request from the connected client; the records it was sent.
    std::vector<std::string> Send (const std::string & request)
    {
        rig.transport.Send (client, request);
        return TakeRecords();
    }

    //  The records the client was sent since the last call, after a pump.
    std::vector<std::string> TakeRecords()
    {
        std::vector<std::string>  written;
        std::vector<std::string>  taken;



        rig.controller.Pump();
        written = rig.transport.Written (client);
        taken.assign (written.begin() + (std::ptrdiff_t) m_read, written.end());
        m_read = written.size();
        return taken;
    }

    static JsonValue Parse (const std::string & record)
    {
        JsonValue       root;
        JsonParseError  error;
        HRESULT         hr = JsonParser::Parse (record, root, error);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, AppleWinBatchRig::Widen (record).c_str());
        return root;
    }

    Byte Peek (Word address)
    {
        Byte  value = 0;



        GetSession().TryPeek (address, value);
        return value;
    }

    Cpu6502Registers GetRegisters()
    {
        return GetSession().GetTarget().GetRegisters();
    }

private:
    size_t  m_read = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinJson
//
//  Reading a record's members, asserting each is present with the type
//  asked for.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWinJson
{
public:
    //  The member, or null when the record has none by that key.
    static const JsonValue * FindMember (const JsonValue & object, const std::string & key)
    {
        for (const auto & [name, value] : object.GetObjectEntries())
        {
            if (name == key)
            {
                return &value;
            }
        }

        return nullptr;
    }

    static std::string GetString (const JsonValue & object, const std::string & key)
    {
        std::string  text;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (object.HasString (key, text), AppleWinBatchRig::Widen ("no string " + key).c_str());
        return text;
    }

    static int GetInt (const JsonValue & object, const std::string & key)
    {
        int  number = 0;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (object.HasInt (key, number), AppleWinBatchRig::Widen ("no number " + key).c_str());
        return number;
    }

    static bool GetBool (const JsonValue & object, const std::string & key)
    {
        bool  flag = false;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (object.HasBool (key, flag), AppleWinBatchRig::Widen ("no bool " + key).c_str());
        return flag;
    }

    static const JsonValue & GetObjectMember (const JsonValue & object, const std::string & key)
    {
        const JsonValue  * member = nullptr;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (object.HasObject (key, member), AppleWinBatchRig::Widen ("no object " + key).c_str());
        return *member;
    }

    static const JsonValue & GetArrayMember (const JsonValue & object, const std::string & key)
    {
        const JsonValue  * member = nullptr;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (object.HasArray (key, member), AppleWinBatchRig::Widen ("no array " + key).c_str());
        return *member;
    }

    //  Present, and null: absent would not do.
    static bool IsNull (const JsonValue & object, const std::string & key)
    {
        const JsonValue  * member = FindMember (object, key);



        return member != nullptr && member->GetType() == JsonType::Null;
    }

    //  Whether the record has the member at all.
    static bool Has (const JsonValue & object, const std::string & key)
    {
        return FindMember (object, key) != nullptr;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  RefusingDebugTarget
//
//  The mock target with a run driver that cannot start a run, which a real
//  machine cannot be made to do without tripping an assertion.
//
////////////////////////////////////////////////////////////////////////////////

class RefusingDebugTarget : public MockDebugTarget
{
public:
    HRESULT StartRun (const RunRequest &) override { return E_FAIL; }
};
