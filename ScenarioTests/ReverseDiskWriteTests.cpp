#include "Pch.h"

#include "GuestSession.h"
#include "Core/TextEncoding.h"
#include "TestMachine.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/IDebugNotificationSink.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Debugger/Reverse/ReverseStopTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseDiskWriteTests
//
//  Reverse continue to the sectors DOS 3.3 writes when it saves a file: with
//  history recording, the guest saves a program and then waits at its prompt
//  for a few seconds, and a write breakpoint set afterward is found going
//  back.
//
////////////////////////////////////////////////////////////////////////////////

namespace ReverseDiskWriteTests
{
    static constexpr const char *  kpszMachine        = "Apple2eEnhanced";
    static constexpr uint64_t      kFramesPerKeyframe = 10;
    static constexpr uint64_t      kWaitCycles        = 5'000'000;



    struct Rig
    {
        TestMachine                machine    { kpszMachine };
        MachineDebugTarget         target     { machine };
        NullDebugNotificationSink  sink;
        DebugSession               session    { target, sink, RunState::FreeRunning };
        BreakpointHandlers         handlers;
        ReverseController          controller { machine };

        Rig()
        {
            session.AddHandler (&handlers);
            machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
        }

        //  Boots DOS 3.3, starts history and saves a program, then waits.
        void SaveAndWait()
        {
            ReverseSettings  settings;



            GuestSession::BootToPrompt (machine, GuestSession::RequireDos33Master());
            (void) GuestSession::TypeAndCollect (machine, "10 PRINT 1");

            settings.keyframes.intervalCycles = KeyframeSettings::kFrameCycles * kFramesPerKeyframe;
            settings.keyframes.budgetBytes    = 64ull * 1024 * 1024;
            Assert::AreEqual (S_OK, controller.Start (settings), L"Start");

            (void) GuestSession::TypeAndCollect (machine, "SAVE X");
            machine.RunCycles (kWaitCycles);
        }

        //  Reverse continue with one breakpoint, and the time it took.
        ReverseResult  ReverseTo (const std::string & line, uint64_t & elapsedMs)
        {
            ReverseStopTest  stopTest  { session };
            ReverseResult    result;
            uint64_t         startedAt = 0;



            Assert::AreEqual ((int) CommandStatus::Ok, (int) session.ExecuteLine (line).status, TextEncoding::NarrowToWide (line).c_str());
            session.OnStopConditionsChanged();

            startedAt = GetTickCount64();
            Assert::AreEqual (S_OK, controller.ReverseContinue (stopTest, result), L"ReverseContinue");
            elapsedMs = GetTickCount64() - startedAt;

            return result;
        }
    };





    TEST_CLASS (ReverseDiskWriteTests)
    {
    public:

        //  Write mode, which SAVE turns on for each sector.
        TEST_METHOD (ReverseContinueFindsTheSavesWriteMode)
        {
            Rig            rig;
            uint64_t       elapsed = 0;
            ReverseResult  result;



            rig.SaveAndWait();
            result = rig.ReverseTo ("BPDISK D1 WRITE", elapsed);

            Logger::WriteMessage (std::format ("write mode: {} ms, outcome {}\n", elapsed, (int) result.outcome).c_str());
            Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"the last write is found");
        }


        //  A written sector, as the dialog's Write, A sector sets it.
        TEST_METHOD (ReverseContinueFindsTheSavesLastSector)
        {
            Rig            rig;
            uint64_t       elapsed = 0;
            ReverseResult  result;



            rig.SaveAndWait();
            result = rig.ReverseTo ("BPDISK D1 WRITE * *", elapsed);

            Logger::WriteMessage (std::format ("written sector: {} ms, outcome {}\n", elapsed, (int) result.outcome).c_str());
            Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"the last written sector is found");
        }
    };
}
