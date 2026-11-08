#pragma once

#include "Cli/DebugAttachRunner.h"
#include "Cli/DebugBatchRunner.h"
#include "ControllerRig.h"
#include "Debugger/Channel/IInstanceDirectory.h"
#include "Debugger/CommandModeHelp.h"
#include "Debugger/CommandModeNames.h"
#include "EmuTests/FixtureRomSource.h"
#include "UiTests/InMemoryFileSystem.h"

#include "CppUnitTest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyCheck
//
//  Exact assertions on a reply: its status, its error's label and detail, and
//  every line of the text it rendered. Text is compared as one string, a
//  newline between lines, so a mismatch prints both whole.
//
////////////////////////////////////////////////////////////////////////////////

class ReplyCheck
{
public:
    static std::wstring Widen (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }


    static std::string Join (const std::vector<std::string> & lines)
    {
        std::string  joined;



        for (size_t i = 0; i < lines.size(); ++i)
        {
            joined += (i == 0 ? "" : "\n") + lines[i];
        }

        return joined;
    }


    static std::string GetStatusName (CommandStatus status)
    {
        switch (status)
        {
        case CommandStatus::Ok:           return "ok";
        case CommandStatus::Error:        return "error";
        case CommandStatus::NotAvailable: return "not available";
        default:                          return "unknown";
        }
    }


    //  Succeeded, and rendered exactly this text.
    static void IsOk (const Reply & reply, const std::string & text, const std::string & where)
    {
        std::string  context = where + ": " + reply.error.label + ", " + reply.error.detail;



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (std::string ("ok"), GetStatusName (reply.status), Widen (context).c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (text, Join (reply.text), Widen (where).c_str());
    }


    //  Failed with this status, label and detail, rendered as the AppleWin,
    //  GSSquared and WinDbg formats render an error.
    static void IsError (const Reply & reply, CommandStatus status, const std::string & label, const std::string & detail, const std::string & where)
    {
        IsErrorAfter (reply, "", status, label, detail, where);
    }


    //  The same, with the Monitor format's ERR line first.
    static void IsMonitorError (const Reply & reply, CommandStatus status, const std::string & label, const std::string & detail, const std::string & where)
    {
        IsErrorAfter (reply, "ERR", status, label, detail, where);
    }


    //  Failed as IsError says, after text the reply wrote ahead of the error.
    static void IsErrorAfter (const Reply & reply, const std::string & before, CommandStatus status,
                              const std::string & label, const std::string & detail, const std::string & where)
    {
        std::string  expected = before.empty() ? std::string() : before + "\n";



        expected += "Error: " + label + "\n" + std::string (7, ' ') + detail;

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (GetStatusName (status), GetStatusName (reply.status), Widen (where).c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (label,    reply.error.label,  Widen (where).c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (detail,   reply.error.detail, Widen (where).c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (expected, Join (reply.text),  Widen (where).c_str());
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleRig
//
//  The debugger the running emulator holds -- a controller over a real machine
//  with every handler, the channel open, the current directory as production
//  leaves it (unset) -- reached the ways a user reaches it: a line typed into
//  the window's console, and a request on the channel.
//
//  Type is what the window's command box does: the line runs in the session's
//  mode, with the window's own commands (PANEL, CODE, M1 ...) carried out on
//  the panes. Request is what the channel does with a request's line, in the
//  mode the request carried or the session's. Runs go through
//  CpuManagerRunDriver, whose Start returns at once; FinishRun stands in for
//  the frame loop that would execute them, as CpuManagerRunDriverTests does.
//
////////////////////////////////////////////////////////////////////////////////

class ConsoleRig : public ControllerRig
{
public:
    ConsoleRig()
    {
        HRESULT  hr = controller.Open();



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr, L"the debugger opened");
    }


    DebugSession & Session()
    {
        return controller.GetSession();
    }


    Reply Type (const std::string & line)
    {
        return view.ExecuteWindowLine (Session(), line, Session().GetMode());
    }


    Reply Request (const std::string & line, std::optional<CommandMode> mode = std::nullopt)
    {
        return controller.RunLine (line, mode, std::nullopt);
    }


    //  MODE typed as the dialect in force writes it: `/MODE` in Monitor mode,
    //  `!MODE` in WinDbg mode, the bare name elsewhere.
    void SwitchTo (CommandMode mode)
    {
        std::string  line  = std::string (CommandModeHelp::GetMarker (Session().GetMode())) + "MODE " + CommandModeNames::GetUpperName (mode);
        Reply        reply = Type (line);



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (reply.status), ReplyCheck::Widen (line).c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue   (Session().GetMode() == mode, ReplyCheck::Widen (line).c_str());
    }


    //  Slices of the machine, each reported to the run driver as the frame
    //  loop reports it, until the run in progress stops or the cycles run out.
    void FinishRun (uint64_t maxCycles = kFinishCycles)
    {
        uint64_t  spent = 0;



        while (controller.GetRunDriver().IsRunning() && spent < maxCycles)
        {
            uint64_t  ran = machine.RunCycles (kSliceCycles);



            spent += ran;
            controller.GetRunDriver().OnSliceExecuted ((uint32_t) ran);
        }
    }


    Word GetPc()
    {
        return Session().GetTarget().GetRegisters().pc;
    }


    Byte Peek (Word address)
    {
        Byte  value = 0;



        (void) Session().GetTarget().TryPeek (address, value);
        return value;
    }


    static constexpr uint64_t  kSliceCycles  = 1023;
    static constexpr uint64_t  kFinishCycles = 2'000'000;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BatchScriptRig
//
//  `CassoCli debug` over the fixture ROMs and an in-memory file system: the
//  whole runner, so the output, the exit status and the files a script wrote
//  are what the command line would produce. The machine outlives the run, so
//  a test reads what the script did to it.
//
////////////////////////////////////////////////////////////////////////////////

class BatchScriptRig
{
public:
    FixtureRomSource                   roms;
    InMemoryFileSystem                 files;
    CommandLineOptions::DebugOptions   options;
    DebugBatchResult                   result;
    std::unique_ptr<DebugBatchRunner>  runner;


    BatchScriptRig()
    {
        options.machine = "Apple2e";
    }


    //  The script's lines, then options.commands; the exit status.
    int Run (const std::string & script)
    {
        HRESULT  hr = S_OK;



        runner = std::make_unique<DebugBatchRunner> (roms, files);
        hr     = runner->Run (options, script, result);

        IGNORE_RETURN_VALUE (hr, S_OK);
        return result.exitStatus;
    }


    Cpu6502Registers GetRegisters()
    {
        return runner->GetHost()->GetCpu()->GetCpu6502()->GetRegisters();
    }


    Byte Peek (Word address)
    {
        return runner->GetHost()->GetMemoryBus().ReadByte (address);
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  LoopbackChannelClient
//
//  `CassoCli debug --attach`'s end of the channel, joined to a ConsoleRig's
//  transport: a line written is a request the controller runs at once, and a
//  line read is whatever the controller sent this client. DebugAttachRunner
//  run over it goes the whole way into the emulator's session.
//
////////////////////////////////////////////////////////////////////////////////

class LoopbackChannelClient : public IChannelClient
{
public:
    explicit LoopbackChannelClient (ConsoleRig & rig) :
        m_rig (rig)
    {
        m_connection = rig.transport.Connect();
        rig.controller.Pump();
    }


    bool WriteLine (const std::string & line) override
    {
        m_rig.transport.Send (m_connection, line);
        m_rig.controller.Pump();
        return true;
    }


    bool ReadLine (std::string & line, DWORD) override
    {
        const std::vector<std::string> & written = m_rig.transport.Written (m_connection);



        if (m_read >= written.size())
        {
            return false;
        }

        line = written[m_read++];
        return true;
    }


    bool IsClosed () const override
    {
        return false;
    }


private:
    ConsoleRig           & m_rig;
    ChannelConnectionId    m_connection = 0;
    size_t                 m_read       = 0;
};
