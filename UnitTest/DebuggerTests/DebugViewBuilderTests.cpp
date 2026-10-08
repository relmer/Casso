#include "Pch.h"

#include "ControllerRig.h"
#include "Core/ParallelWorkPool.h"
#include "Ui/Debugger/DebugViewBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebugViewBuilderTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugViewBuilderTests
    //
    //  The panes built from a capture on another session are the panes built
    //  from the live session: the same code, memory, registers, breakpoints,
    //  watches, stack, call stack, trace and annotations.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugViewBuilderTests)
    {
    public:

        TEST_METHOD (APaneBuiltFromACaptureIsThePaneBuiltLive)
        {
            static constexpr uint64_t  kRunCycles = 200;

            ControllerRig         rig;
            DebugSession        & session  = rig.controller.GetSession();
            DebuggerViewState     liveView;
            DebuggerViewState     capturedView;
            DebugViewBuilder      builder;
            DebugViewInput        input;
            DebuggerViewSnapshot  live;
            DebuggerViewSnapshot  built;
            auto                  capture  = std::make_shared<DebugViewCapture>();
            uint64_t              first    = 0;



            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Run ("HISTORY ON").status);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Run ("BP 1234").status);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Run ("WA 400").status);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Run ("SYM START = 300").status);
            //  JMP $0300 in place of the RTS, so the program loops and the trace
            //  fills.
            rig.machine.GetMemoryBus().WriteByte (0x0305, 0x4C);
            rig.machine.GetMemoryBus().WriteByte (0x0306, 0x00);
            rig.machine.GetMemoryBus().WriteByte (0x0307, 0x03);
            rig.machine.RunCycles (kRunCycles);

            live = liveView.BuildCaptured (session, true);

            first = DebuggerViewState::GetTraceWindowFirst (session.GetTarget().GetTraceSize(), std::nullopt, DebuggerViewState::kTraceRows);

            DebugViewCapture::Take (session.GetTarget(), CallRecord(), (size_t) first, DebuggerViewState::kTraceRows, *capture);
            session.TakeView (input.session);

            input.capture  = capture;
            input.isPaused = true;

            built = builder.Build (capturedView, input);

            AssertSamePanes (live, built);
        }

        //  Built at once on a pool, the panes are the panes built one after
        //  another, every time: the jobs write only their own parts.
        TEST_METHOD (PanesBuiltAtOnceOnAPoolAreThePanesBuiltInTurn)
        {
            static constexpr uint64_t  kRunCycles   = 200;
            static constexpr int       kBuilds      = 50;
            static constexpr DWORD     kMaxThreads  = 3;

            ControllerRig         rig;
            DebugSession        & session  = rig.controller.GetSession();
            DebuggerViewState     liveView;
            DebuggerViewState     capturedView;
            DebugViewBuilder      builder;
            ParallelWorkPool      pool;
            DebugViewInput        input;
            DebuggerViewSnapshot  live;
            auto                  capture  = std::make_shared<DebugViewCapture>();
            uint64_t              first    = 0;
            HRESULT               hr       = S_OK;



            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Run ("HISTORY ON").status);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Run ("WA 400").status);
            rig.machine.GetMemoryBus().WriteByte (0x0305, 0x4C);
            rig.machine.GetMemoryBus().WriteByte (0x0306, 0x00);
            rig.machine.GetMemoryBus().WriteByte (0x0307, 0x03);
            rig.machine.RunCycles (kRunCycles);

            hr = pool.Create (kMaxThreads);
            Assert::AreEqual (S_OK, hr);

            live  = liveView.BuildCaptured (session, true);
            first = DebuggerViewState::GetTraceWindowFirst (session.GetTarget().GetTraceSize(), std::nullopt, DebuggerViewState::kTraceRows);

            DebugViewCapture::Take (session.GetTarget(), CallRecord(), (size_t) first, DebuggerViewState::kTraceRows, *capture);
            session.TakeView (input.session);

            input.capture  = capture;
            input.isPaused = true;
            builder.SetRunner (&pool);

            for (int i = 0; i < kBuilds; i++)
            {
                AssertSamePanes (live, builder.Build (capturedView, input));
            }
        }
    private:

        static void AssertSamePanes (const DebuggerViewSnapshot & live, const DebuggerViewSnapshot & built)
        {
            Assert::AreEqual (live.pc, built.pc, L"pc");
            Assert::AreEqual (live.flags, built.flags, L"flags");
            Assert::AreEqual (live.registers.size(), built.registers.size(), L"register rows");

            for (size_t i = 0; i < live.registers.size(); i++)
            {
                Assert::AreEqual (live.registers[i].value, built.registers[i].value, L"a register");
            }

            Assert::AreEqual (live.code.size(), built.code.size(), L"code rows");
            Assert::IsTrue   (live.code.size() > 0, L"the code pane has rows");

            for (size_t i = 0; i < live.code.size(); i++)
            {
                Assert::AreEqual (live.code[i].address,     built.code[i].address,     L"a code row's address");
                Assert::AreEqual (live.code[i].bytes,       built.code[i].bytes,       L"its bytes");
                Assert::AreEqual (live.code[i].instruction, built.code[i].instruction, L"its instruction");
                Assert::AreEqual (live.code[i].label,       built.code[i].label,       L"its label");
                Assert::AreEqual (live.code[i].annotation,  built.code[i].annotation,  L"its annotation");
            }

            Assert::AreEqual (live.memory.size(), built.memory.size(), L"memory rows");

            for (size_t i = 0; i < live.memory.size(); i++)
            {
                Assert::AreEqual (live.memory[i].bytes, built.memory[i].bytes, L"a memory row");
            }

            Assert::AreEqual (live.breakpoints.size(), built.breakpoints.size(), L"breakpoints");
            Assert::AreEqual (live.watches.size(),     built.watches.size(),     L"watches");
            Assert::AreEqual (live.stack.size(),       built.stack.size(),       L"stack rows");
            Assert::AreEqual (live.callStack.rows.size(), built.callStack.rows.size(), L"call stack rows");
            Assert::AreEqual (live.autoWatches.size(), built.autoWatches.size(), L"automatic watches");
            Assert::AreEqual (live.trace.total,        built.trace.total,        L"trace size");
            Assert::AreEqual (live.trace.entries.size(), built.trace.entries.size(), L"trace rows");
            Assert::IsTrue   (live.trace.entries.size() > 0, L"the trace has rows");

            for (size_t i = 0; i < live.trace.entries.size(); i++)
            {
                Assert::AreEqual (live.trace.entries[i].pc, built.trace.entries[i].pc, L"a trace row");
            }

            if (!live.watches.empty())
            {
                Assert::AreEqual (live.watches[0].value, built.watches[0].value, L"the watch's value");
            }
        }
    };
}
