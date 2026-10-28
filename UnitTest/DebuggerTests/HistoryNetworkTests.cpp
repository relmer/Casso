#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "EmuTests/TestMachine.h"
#include "Machines/Apple2/Common/UthernetCard.h"
#include "resource.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/EmulatorShell.h"
#include "Shell/MachineBuilder.h"
#include "Shell/ShellDebugger.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace HistoryNetworkTests
{
    static constexpr Word      kStart       = 0x0800;
    static constexpr Word      kMode        = 0xC0B4;
    static constexpr Word      kAddressHigh = 0xC0B5;
    static constexpr Word      kAddressLow  = 0xC0B6;
    static constexpr Word      kData        = 0xC0B7;
    static constexpr Word      kSocketMode  = 0x0500;
    static constexpr Word      kSocketCmd   = 0x0501;
    static constexpr Byte      kUdp         = 0x02;
    static constexpr Byte      kOpen        = 0x01;
    static constexpr Byte      kClose       = 0x10;
    static constexpr uint64_t  kFrameCycles = 17'030;
    static constexpr int       kBudgetMb    = 16;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ShellRig
    //
    //  A shell over a //e with the Uthernet II in slot 3, running a loop with
    //  history on. The test opens and closes socket 1 through the card's
    //  registers, as a guest would, and runs frames as the CPU thread does.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ShellRig
    {
    public:
        std::unique_ptr<EmulatorShell>  shell = std::make_unique<EmulatorShell>();



        ShellRig()
        {
            static constexpr Byte  kLoop[] = { 0x4C, 0x00, 0x08 };   // JMP $0800
            MachineHost          & machine = shell->GetMachine();
            MachineBuildServices   services;
            MachineBuilder         builder (machine, services);
            MachineConfig          config;
            HRESULT                hr      = S_OK;
            size_t                 i       = 0;



            TestMachine::LoadConfig ("Apple2e", config);

            std::erase_if (config.slots, [] (const SlotConfig & slot) { return slot.device != "uthernet2"; });

            for (SlotConfig & slot : config.slots)
            {
                slot.enabled = true;
            }

            machine.SetPrng               (std::make_unique<Prng> (TestMachine::kSeed));
            machine.SetVideoTiming        (std::make_unique<VideoTiming>());
            machine.SetCurrentMachineName (L"Apple2e");
            machine.GetConfig() = config;

            hr = builder.Build (config);
            AssertSucceeded (hr, L"the shell's machine builds");
            Assert::IsNotNull (machine.GetRefs().uthernet, L"the card must be installed");

            machine.PowerCycle();

            for (i = 0; i < sizeof (kLoop); i++)
            {
                machine.GetMemoryBus().WriteByte (static_cast<Word> (kStart + i), kLoop[i]);
            }

            machine.GetCpu()->SetPC (kStart);

            shell->GetDebugger().PrepareFramebuffers();

            Dispatch (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (true, kBudgetMb));

            machine.GetMemoryBus().WriteByte (kMode, 0x80);
            machine.GetMemoryBus().WriteByte (kMode, 0x03);
        }



        //  A command, dispatched as the CPU thread dispatches one.
        void Dispatch (WORD id, const std::string & payload)
        {
            EmulatorCommand  command;



            command.id      = id;
            command.payload = payload;

            shell->DispatchCpuCommand (command);
        }



        void RunFrames (int count)
        {
            int  i = 0;



            for (i = 0; i < count; i++)
            {
                shell->GetDebugger().RecordHistoryFrame (false);
                shell->GetMachine().RunCycles (kFrameCycles);
            }
        }



        void WriteSocket (Word address, Byte value)
        {
            MemoryBus  & bus = shell->GetMachine().GetMemoryBus();



            bus.WriteByte (kAddressHigh, static_cast<Byte> (address >> 8));
            bus.WriteByte (kAddressLow,  static_cast<Byte> (address));
            bus.WriteByte (kData,        value);
        }



        void OpenSocket()
        {
            WriteSocket (kSocketMode, kUdp);
            WriteSocket (kSocketCmd,  kOpen);

            Assert::IsTrue (shell->GetMachine().GetRefs().uthernet->IsActive(), L"an open socket makes the card active");
        }



        void CloseSocket()
        {
            WriteSocket (kSocketCmd, kClose);

            Assert::IsFalse (shell->GetMachine().GetRefs().uthernet->IsActive(), L"with it closed the card is idle");
        }



        bool IsRecording() const
        {
            return shell->GetDebugger().GetReverseHost() != nullptr && shell->GetDebugger().GetReverseHost()->IsRecording();
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HistoryNetworkTests
    //
    //  The network side's changes to the Uthernet II are journaled and a
    //  replay makes them again from history, so history is on while the card
    //  is in use, and a step back crosses a socket opening.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HistoryNetworkTests)
    {
    public:

        TEST_METHOD (HistoryIsOnWhileASocketIsOpen)
        {
            ShellRig  rig;
            uint64_t  openedAt = 0;



            rig.RunFrames (3);
            Assert::IsTrue (rig.IsRecording(), L"history is on with the card idle");

            openedAt = rig.shell->GetMachine().GetPosition();
            rig.OpenSocket();
            rig.RunFrames (3);
            Assert::IsTrue (rig.IsRecording(), L"history is on while the socket is open");

            rig.CloseSocket();
            rig.RunFrames (3);
            Assert::IsTrue (rig.IsRecording(), L"and after it closes");
            Assert::IsTrue (rig.shell->GetDebugger().GetReverseHost()->GetController().GetOldestPosition() < openedAt,
                            L"history starts before the time the card was in use");
        }


        //  A step back after a socket opened mid-frame replays the
        //  instructions that opened it, from history.
        TEST_METHOD (AStepBackAfterASocketOpensMoves)
        {
            ShellRig  rig;
            uint64_t  position = 0;



            rig.RunFrames (3);

            rig.OpenSocket();
            position = rig.shell->GetMachine().GetPosition();

            rig.Dispatch (IDM_DEBUG_REVERSE, CpuCommandDispatcher::FormatReversePayload (ReverseCommand::StepBack, 0));

            Assert::IsTrue (rig.shell->GetMachine().GetPosition() < position, L"the machine steps back");
            Assert::IsTrue (rig.IsRecording(), L"and history is on");
        }


        //  Turning history on in Options while the card is in use starts it
        //  at once.
        TEST_METHOD (TurningHistoryOnWhileASocketIsOpenStartsIt)
        {
            ShellRig  rig;



            rig.Dispatch (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (false, kBudgetMb));

            rig.OpenSocket();
            rig.RunFrames (1);

            rig.Dispatch (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (true, kBudgetMb));
            Assert::IsTrue (rig.IsRecording(), L"history starts while the socket is open");
        }
    };
}
