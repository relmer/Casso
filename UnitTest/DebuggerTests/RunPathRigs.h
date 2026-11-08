#pragma once

#include "Cli/DebugBatchRunner.h"
#include "ControllerRig.h"
#include "Core/JsonParser.h"
#include "EmuTests/FixtureRomSource.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RunPathText
//
//  Reading what the ways in hand back: a JSON record from the channel or
//  from batch mode's JSON Lines, its text, and its status.
//
////////////////////////////////////////////////////////////////////////////////

class RunPathText
{
public:
    static std::wstring Widen (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }



    static JsonValue Parse (const std::string & text)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT         hr = JsonParser::Parse (text, value, error);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, Widen (text + ": " + error.message).c_str());
        return value;
    }



    static std::string GetString (const JsonValue & record, const std::string & key)
    {
        std::string  value;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (record.HasString (key, value), Widen ("no " + key).c_str());
        return value;
    }



    //  A member of a record, whatever its type, or null when it is absent.
    static const JsonValue * FindMember (const JsonValue & record, const std::string & key)
    {
        for (const auto & [name, value] : record.GetObjectEntries())
        {
            if (name == key)
            {
                return &value;
            }
        }

        return nullptr;
    }



    static std::vector<std::string> GetText (const JsonValue & record)
    {
        const JsonValue           * text = FindMember (record, "text");
        std::vector<std::string>    lines;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsNotNull (text, L"the record has text");

        for (size_t i = 0; i < text->GetArraySize(); ++i)
        {
            lines.push_back (text->GetArrayElement (i).GetString());
        }

        return lines;
    }



    //  Lines compared whole, and all of them shown when they differ.
    static void AssertLines (const std::vector<std::string> & expected, const std::vector<std::string> & actual, const std::string & what)
    {
        std::string  shown;



        for (const std::string & line : actual)
        {
            shown += "\n  [" + line + "]";
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (expected == actual, Widen (what + ": got" + shown).c_str());
    }



    //  A JSON string, quoted, for a request line.
    static std::string Quote (const std::string & text)
    {
        std::string  quoted = "\"";



        for (char ch : text)
        {
            if (ch == '"' || ch == '\\')
            {
                quoted += '\\';
            }

            quoted += ch;
        }

        return quoted + "\"";
    }



    //  The lines of a file, each without its line ending.
    static std::vector<std::string> SplitLines (const std::string & text)
    {
        std::vector<std::string>  lines;
        size_t                    start = 0;
        size_t                    end   = text.find ('\n');



        while (end != std::string::npos)
        {
            lines.push_back (text.substr (start, end - start));
            start = end + 1;
            end   = text.find ('\n', start);
        }

        return lines;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorRig
//
//  The debugger inside the emulator, reached the ways a person reaches it:
//  the window's console, a channel client, the window's keys, and the
//  emulator's own pause, resume and resets. The controller is open, as it
//  is whenever the window or a client can type, so the call stack is being
//  recorded; a second client listens for every stop.
//
//  A debugger run goes through the CpuManagerRunDriver. Slices stands in
//  for the frame loop, which hands each slice to the driver on a thread
//  that needs an HWND and so cannot run in a test; that one call in the
//  frame loop is what this leaves uncovered. The emulator's pause, resume
//  and reset notifications are EmulatorShell's two lines each, repeated
//  here for the same reason.
//
////////////////////////////////////////////////////////////////////////////////

class EmulatorRig : public ControllerRig, public RunPathText
{
public:
    static constexpr uint32_t  kSlice     = 1023;
    static constexpr int       kMaxSlices = 4000;
    static constexpr Byte      kIrqMasked = 0x34;
    static constexpr Byte      kStackTop  = 0xFF;
    static constexpr Word      kKeyboard  = 0xC000;
    static constexpr Word      kStrobe    = 0xC010;

    ChannelConnectionId  listener = 0;
    ChannelConnectionId  client   = 0;
    int64_t              nextId   = 1;



    EmulatorRig()
    {
        HRESULT  hr = controller.Open();



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, L"the controller opened");
        listener = transport.Connect();
        client   = transport.Connect();
        controller.Pump();
    }



    DebugSession & Session()
    {
        return controller.GetSession();
    }



    //  Bytes written through the debugger's view of memory.
    void Poke (Word address, std::initializer_list<Byte> bytes)
    {
        Word  at = address;



        for (Byte b : bytes)
        {
            Session().GetTarget().TryPoke (at++, b);
        }
    }



    //  The CPU at start, with the stack empty, X and Y clear and P as given.
    void SetPc (Word start, Byte p = kIrqMasked)
    {
        Cpu6502Registers  registers = Session().GetTarget().GetRegisters();



        registers.pc = start;
        registers.sp = kStackTop;
        registers.x  = 0;
        registers.y  = 0;
        registers.p  = p;
        Session().GetTarget().SetRegisters (registers);
    }



    void Load (Word address, std::initializer_list<Byte> bytes, Word start, Byte p = kIrqMasked)
    {
        Poke  (address, bytes);
        SetPc (start, p);
    }



    Byte Peek (Word address)
    {
        Byte  value = 0;



        Session().GetTarget().TryPeek (address, value);
        return value;
    }



    //  The keyboard latch as the guest reads it at $C000, which has no side
    //  effect; the strobe cleared as a guest's store to $C010 clears it.
    Byte ReadKeyboard()
    {
        return machine.GetMemoryBus().ReadByte (kKeyboard);
    }

    void ClearStrobe()
    {
        machine.GetMemoryBus().WriteByte (kStrobe, 0);
    }



    //  A line typed into the console in the session's mode, run as the
    //  emulator runs the window's lines.
    Reply Window (const std::string & line)
    {
        return view.ExecuteWindowLine (Session(), line, Session().GetMode());
    }

    Reply WindowOk (const std::string & line)
    {
        Reply  reply = Window (line);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status,
                                                                          Widen (line + ": " + reply.error.label + ", " + reply.error.detail).c_str());
        return reply;
    }



    //  The frame loop's slices, each handed to the run driver, until the
    //  driver ends the run. False when no run ended within the limit.
    bool Slices (int limit = kMaxSlices)
    {
        uint64_t  ran     = 0;
        bool      isEnded = false;



        for (int slice = 0; slice < limit && !isEnded; ++slice)
        {
            ran     = machine.RunCycles (kSlice);
            isEnded = controller.GetRunDriver().OnSliceExecuted ((uint32_t) ran);
        }

        controller.Pump();
        return isEnded;
    }



    //  A run typed into the console, carried to its stop.
    Reply WindowRun (const std::string & line)
    {
        Reply  reply = WindowOk (line);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (Slices(), Widen ("the run ended: " + line).c_str());
        return reply;
    }



    //  The emulator resumed with no debugger run: the machine runs freely,
    //  slice by slice, for at least the cycles given.
    void FreeRun (uint64_t cycles)
    {
        uint64_t  spent = 0;
        uint64_t  ran   = 0;



        cpuManager.SetPaused (false);
        Session().OnUserResumed();

        while (spent < cycles)
        {
            ran    = machine.RunCycles (kSlice);
            spent += ran;
            controller.GetRunDriver().OnSliceExecuted ((uint32_t) ran);

            if (ran == 0)
            {
                break;
            }
        }

        controller.Pump();
    }



    //  The emulator's pause, as the shell reports it: a debugger run in
    //  progress ends, and the session hears the pause.
    void UserPause()
    {
        cpuManager.SetPaused (true);
        controller.GetRunDriver().EndForUserPause();
        Session().OnUserPaused();
        controller.Pump();
    }

    void PowerCycle()
    {
        machine.PowerCycle();
        Session().OnReset (true);
        controller.Pump();
    }

    void SoftReset()
    {
        machine.SoftReset();
        Session().OnReset (false);
        controller.Pump();
    }



    //  One command request from the client, with the mode and budget it
    //  gives, and the reply record the client received for it.
    JsonValue Pipe (const std::string & line, const char * mode = nullptr, std::optional<uint64_t> budget = {})
    {
        int64_t                            id      = nextId++;
        std::string                        request = std::format ("{{\"type\":\"command\",\"id\":{},\"line\":{}", id, Quote (line));
        const std::vector<std::string>   * written = nullptr;
        double                             number  = 0;



        if (mode != nullptr)
        {
            request += std::format (",\"mode\":\"{}\"", mode);
        }

        if (budget.has_value())
        {
            request += std::format (",\"budget\":{}", *budget);
        }

        transport.Send (client, request + "}");
        controller.Pump();

        written = &transport.Written (client);

        for (const std::string & record : *written)
        {
            JsonValue  parsed = Parse (record);



            if (parsed.HasNumber ("id", number) && (int64_t) number == id)
            {
                return parsed;
            }
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail (Widen ("no reply to " + line).c_str());
        return JsonValue();
    }

    std::vector<std::string> PipeText (const std::string & line, const char * mode = nullptr)
    {
        return GetText (Pipe (line, mode));
    }



    //  Every stop the listening client heard, oldest first.
    std::vector<JsonValue> GetStops()
    {
        std::vector<JsonValue>  stops;



        for (const std::string & record : transport.Written (listener))
        {
            JsonValue    parsed = Parse (record);
            std::string  type;



            if (parsed.HasString ("type", type) && type == "stopped")
            {
                stops.push_back (parsed);
            }
        }

        return stops;
    }

    JsonValue GetLastStop()
    {
        std::vector<JsonValue>  stops = GetStops();



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsFalse (stops.empty(), L"a stop was announced");
        return stops.back();
    }



    //  A key pressed in the window with the command line empty: the action
    //  the scheme maps it to, and the line that action's button sends, in
    //  the words of the session's mode. `cursor` is the code line selected,
    //  for Run to cursor.
    Reply Press (DebuggerKeyScheme scheme, WPARAM vk, bool ctrl, bool shift, std::optional<Word> cursor = {})
    {
        int                         id       = 0;
        int                         row      = -1;
        bool                        isMapped = DebuggerKeySchemes::GetMap (scheme).TryTranslate (vk, ctrl, false, shift, id);
        DebuggerViewSnapshot        snapshot = view.Build (Session());
        std::optional<std::string>  line;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (isMapped, L"the scheme maps the key");

        for (size_t i = 0; cursor.has_value() && i < snapshot.code.size(); ++i)
        {
            if (snapshot.code[i].address == *cursor)
            {
                row = (int) i;
            }
        }

        line = DebuggerViewState::GetActionLine ((DebuggerKeySchemes::Action) id, &snapshot, row);
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (line.has_value(), L"the action sends a line");

        return WindowOk (DebuggerViewState::GetModeLine (*line, snapshot.mode));
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  BatchScript
//
//  `CassoCli debug` over the fixture ROMs and an in-memory file system: the
//  real DebugBatchRunner, with every command family attached and the call
//  stack recorded from power-on, printing JSON Lines. The machine stays
//  readable once the script has run.
//
////////////////////////////////////////////////////////////////////////////////

class BatchScript : public RunPathText
{
public:
    FixtureRomSource                  roms;
    InMemoryFileSystem                files;
    CommandLineOptions::DebugOptions  options;
    DebugBatchResult                  result;
    DebugBatchRunner                  runner { roms, files };
    std::vector<JsonValue>            records;



    explicit BatchScript (const std::string & machineId = "Apple2e")
    {
        options.machine = machineId;
        options.json    = true;
    }



    int Run (const std::vector<std::string> & lines)
    {
        std::string  script;
        HRESULT      hr     = S_OK;



        for (const std::string & line : lines)
        {
            script += line + "\n";
        }

        hr = runner.Run (options, script, result);
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, Widen (result.diagnostics).c_str());

        records.clear();

        for (const std::string & line : SplitLines (result.output))
        {
            records.push_back (Parse (line));
        }

        return result.exitStatus;
    }



    std::vector<JsonValue> GetRecords (const std::string & type) const
    {
        std::vector<JsonValue>  matching;
        std::string             each;



        for (const JsonValue & record : records)
        {
            if (record.HasString ("type", each) && each == type)
            {
                matching.push_back (record);
            }
        }

        return matching;
    }



    //  The reply to the script's nth line, counting from zero.
    JsonValue GetReply (size_t index) const
    {
        std::vector<JsonValue>  replies = GetRecords ("reply");



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (index < replies.size(), L"the line was answered");
        return replies[index];
    }

    std::vector<std::string> GetReplyText (size_t index) const
    {
        return GetText (GetReply (index));
    }



    MachineHost & GetMachine()
    {
        MachineHost  * host = runner.GetHost();



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsNotNull (host, L"the machine was built");
        return *host;
    }



    Byte Read (Word address)
    {
        return GetMachine().GetMemoryBus().ReadByte (address);
    }
};
