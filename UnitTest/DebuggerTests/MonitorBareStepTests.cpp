#include "Pch.h"

#include "Debugger/CpuManagerRunDriver.h"
#include "Debugger/MachineDebugTarget.h"
#include "Shell/CpuManager.h"
#include "EmuTests/TestMachine.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/ExecutionHandlers.h"
#include "Debugger/Handlers/MemoryHandlers.h"
#include "HandlerTestRig.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorBareStepTests
//
//  In Monitor mode a bare S steps one instruction from the current PC, as
//  the Monitor does after the first S, run slice by slice as the CPU thread
//  runs it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MonitorBareStepTests)
{
public:

    class Rig
    {
    public:
        TestMachine                machine;
        MachineDebugTarget         target;
        CpuManager                 cpuManager;
        CpuManagerRunDriver        driver;
        RecordingNotificationSink  sink;
        DebugSession               session;
        BreakpointHandlers         breakpoints;
        ExecutionHandlers          execution;
        MemoryHandlers             memory;



        //  $0300: INX / INY / JMP $0300, paused with the PC on the INX.
        Rig() :
            machine (std::string ("Apple2e"), TestMachine::Slots::Empty),
            target  (machine),
            driver  (machine, cpuManager, target.GetRunHook()),
            session (target, sink, RunState::Paused)
        {
            Cpu6502Registers  r = {};



            target.SetRunDriver (&driver);
            session.AddHandler  (&breakpoints);
            session.AddHandler  (&execution);
            session.AddHandler  (&memory);

            (void) target.TryPoke (0x0300, 0xE8);
            (void) target.TryPoke (0x0301, 0xC8);
            (void) target.TryPoke (0x0302, 0x4C);
            (void) target.TryPoke (0x0303, 0x00);
            (void) target.TryPoke (0x0304, 0x03);

            r    = target.GetRegisters();
            r.pc = 0x0300;
            r.x  = 0x00;
            r.y  = 0x00;
            r.sp = 0xFF;
            r.p  = 0x34;
            target.SetRegisters (r);
            cpuManager.SetPaused (true);
        }



        //  One Monitor line, then the CPU thread's slices until the run ends.
        Reply Step (const char * line)
        {
            Reply  reply = session.ExecuteLine (line, CommandMode::Monitor);



            for (int i = 0; i < 50 && !cpuManager.IsPaused(); i++)
            {
                uint32_t  actual = (uint32_t) machine.RunCycles (1000);



                if (driver.OnSliceExecuted (actual) || actual == 0)
                {
                    break;
                }
            }

            return reply;
        }
    };



    TEST_METHOD (ABareSStepsOneInstructionFromTheCurrentPc)
    {
        Rig    rig;
        Reply  reply;



        reply = rig.Step ("S");

        Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status,       L"a bare S is accepted");
        Assert::IsTrue   (rig.cpuManager.IsPaused(),                         L"stopped after one instruction");
        Assert::AreEqual ((Word) 0x0301, rig.target.GetRegisters().pc,       L"the INX ran");
        Assert::AreEqual ((Byte) 1,      rig.target.GetRegisters().x,        L"X");
        Assert::AreEqual ((Byte) 0,      rig.target.GetRegisters().y,        L"nothing past it");

        reply = rig.Step ("S");

        Assert::AreEqual ((Word) 0x0302, rig.target.GetRegisters().pc,       L"the next S goes on from there");
        Assert::AreEqual ((Byte) 1,      rig.target.GetRegisters().y,        L"the INY ran");
    }



    //  After an S from an address, a bare S steps from where that one left
    //  the PC, not from the address again.
    TEST_METHOD (ABareSAfterAnAddressedSGoesOnFromThePc)
    {
        Rig  rig;



        (void) rig.Step ("301S");

        Assert::AreEqual ((Word) 0x0302, rig.target.GetRegisters().pc, L"301S ran the INY");

        (void) rig.Step ("S");

        Assert::AreEqual ((Word) 0x0300, rig.target.GetRegisters().pc, L"the bare S ran the JMP");
        Assert::AreEqual ((Byte) 0,      rig.target.GetRegisters().x,  L"and not the INX");
        Assert::AreEqual ((Byte) 1,      rig.target.GetRegisters().y,  L"Y from the one INY");
    }
};
