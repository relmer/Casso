#pragma once

#include "Debugger/DebugSession.h"
#include "Debugger/IDebugNotificationSink.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/SynchronousRunDriver.h"
#include "EmuTests/TestMachine.h"
#include "MockDebugTarget.h"
#include "UiTests/InMemoryFileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingNotificationSink
//
////////////////////////////////////////////////////////////////////////////////

class RecordingNotificationSink : public IDebugNotificationSink
{
public:
    std::vector<StopEvent>    stops;
    int                       resumed = 0;
    int                       resets  = 0;
    std::vector<std::string>  machines;
    std::vector<CommandMode>  modes;

    void OnStopped        (const StopEvent & stop) override    { stops.push_back (stop); }
    void OnResumed        () override                          { ++resumed; }
    void OnReset          (bool) override                      { ++resets; }
    void OnMachineChanged (const std::string & name) override  { machines.push_back (name); }
    void OnModeChanged    (CommandMode mode) override          { modes.push_back (mode); }
};





////////////////////////////////////////////////////////////////////////////////
//
//  HandlerRig
//
//  A session over the mock target with one handler family attached and an
//  in-memory file system, driven by command lines. Run parses, executes and
//  formats; RunOk and RunFails assert the outcome.
//
////////////////////////////////////////////////////////////////////////////////

template <typename Handlers>
class HandlerRig
{
public:
    MockDebugTarget            target;
    RecordingNotificationSink  sink;
    InMemoryFileSystem         files;
    DebugSession               session { target, sink, RunState::Paused };
    Handlers                   handlers;

    HandlerRig()
    {
        session.AddHandler          (&handlers);
        session.SetFileSystem       (&files);
        session.SetCurrentDirectory (L"C:\\Work");
    }

    Reply Run (const std::string & line)
    {
        Reply  reply = session.ExecuteLine (line);



        session.FormatReply (reply);
        return reply;
    }

    Reply RunOk (const std::string & line)
    {
        Reply         reply = Run (line);
        std::string   why   = line + ": " + reply.error.label + ", " + reply.error.detail;
        std::wstring  where (why.begin(), why.end());



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, where.c_str());
        return reply;
    }

    Reply RunFails (const std::string & line, const std::string & label)
    {
        Reply         reply = Run (line);
        std::wstring  where (line.begin(), line.end());



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, where.c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (label, reply.error.label, where.c_str());
        return reply;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHandlerRig
//
//  The same, over a real machine run synchronously, for commands whose
//  behavior is in the run: stepping, calls, the branch record, the profile
//  and the trace file.
//
////////////////////////////////////////////////////////////////////////////////

template <typename Handlers>
class MachineHandlerRig
{
public:
    TestMachine                machine;
    MachineDebugTarget         target;
    SynchronousRunDriver       driver;
    RecordingNotificationSink  sink;
    InMemoryFileSystem         files;
    DebugSession               session;
    Handlers                   handlers;

    explicit MachineHandlerRig (const std::string & id = "Apple2e") :
        machine (id, TestMachine::Slots::Empty),
        target  (machine),
        driver  (machine, target.GetRunHook()),
        session (target, sink, RunState::Paused)
    {
        target.SetRunDriver         (&driver);
        machine.PowerCycle();
        session.AddHandler          (&handlers);
        session.SetFileSystem       (&files);
        session.SetCurrentDirectory (L"C:\\Work");
    }

    // Writes bytes and points the CPU at start with interrupts masked.
    void Load (Word address, std::initializer_list<Byte> bytes, Word start)
    {
        static constexpr Byte  kIrqMasked = 0x34;
        static constexpr Byte  kStackTop  = 0xFF;
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

    Reply Run (const std::string & line)
    {
        Reply  reply = session.ExecuteLine (line);



        session.FormatReply (reply);
        return reply;
    }

    Reply RunOk (const std::string & line)
    {
        Reply         reply = Run (line);
        std::string   why   = line + ": " + reply.error.label + ", " + reply.error.detail;
        std::wstring  where (why.begin(), why.end());



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, where.c_str());
        return reply;
    }

    Reply RunFails (const std::string & line, const std::string & label)
    {
        Reply         reply = Run (line);
        std::wstring  where (line.begin(), line.end());



        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, where.c_str());
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (label, reply.error.label, where.c_str());
        return reply;
    }

    const StopEvent & LastStop()
    {
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsFalse (sink.stops.empty(), L"no stop was delivered");
        return sink.stops.back();
    }
};
