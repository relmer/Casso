#include "Pch.h"

#include "Assembler.h"
#include "Debugger/CallStack.h"
#include "Debugger/DebugFileWriter.h"
#include "Debugger/Handlers/CallStackHandlers.h"
#include "EmuTests/FixtureProvider.h"
#include "HandlerTestRig.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackTests
//
//  The call stack (FR-067 to FR-069, SC-018): the stack walk as a pure
//  function over memory, and the recorder fed by real runs of the call-stack
//  fixture, one case a label. Each FR-069 break is looked for at the
//  instruction that caused it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (CallStackTests)
    {
    public:

        using Rig = MachineHandlerRig<CallStackHandlers>;

        static constexpr Word  kOrigin         = 0x0800;
        static constexpr Word  kIrqUserVector  = 0x03FE;
        static constexpr Byte  kJsr            = 0x20;

        struct Program
        {
            AssemblyResult  result;
            Word            Symbol (const char * name) const { return result.symbols.at (name); }
        };



        //  The fixture assembled and loaded, its debug file and its labels in
        //  the user symbol table, the PC at `start`, and recording on from
        //  there.
        static Program Load (Rig & rig, const char * start)
        {
            FixtureProvider       provider;
            std::vector<uint8_t>  source;
            TestCpu               cpu;
            AssemblerOptions      opts;
            Program               program;
            Word                  at      = kOrigin;



            Assert::AreEqual (S_OK, provider.OpenFixture ("Debugger/Sources/callstack.a65", source));

            program.result = Assembler (cpu.GetInstructionSet(), opts).Assemble (std::string (source.begin(), source.end()));
            Assert::IsTrue (program.result.success);

            for (Byte b : program.result.bytes)
            {
                rig.target.TryPoke (at++, b);
            }

            for (const auto & [name, address] : program.result.symbols)
            {
                rig.session.GetSymbols().Add (SymbolTableId::User, name, (Word) address);
            }

            rig.Load (kOrigin, {}, program.Symbol (start));
            rig.session.SetDebugFile (DebugFileWriter::Build (program.result, { { "", { "callstack.a65", 0 } } }), L"C:\\Work\\callstack.dbg");
            rig.session.SetCallRecording (true);
            return program;
        }



        static void RunTo (Rig & rig, Word address)
        {
            rig.RunOk (std::format ("G {:04X}", address));
            Assert::AreEqual (address, rig.LastStop().pc, L"the run reached its label");
        }



        //  The machine running freely, as it does with the debugger window open,
        //  one instruction at a time until PC reaches the address. No breakpoint
        //  is set and no hook installed, so the record hears only of the
        //  instructions the CPU reports to it.
        static void FreeRunTo (Rig & rig, Word address)
        {
            static constexpr int  kStepLimit = 100'000;
            int                   steps      = 0;



            do
            {
                rig.machine.StepOne();
                ++steps;
            }
            while (rig.machine.GetCpu()->GetPC() != address && steps < kStepLimit);

            Assert::AreEqual (address, rig.machine.GetCpu()->GetPC(), L"the free run reached its label");
        }


        //  The chain CALLS shows at each address in turn, reached by debugger
        //  runs in one machine and by free runs in another, is the same.
        static void AssertFreeRunRecordsTheSame (const char * start, const std::vector<std::string> & labels)
        {
            Rig      stepped;
            Rig      running;
            Program  program = Load (stepped, start);



            Load (running, start);

            for (const std::string & label : labels)
            {
                Word  address = program.Symbol (label.c_str());



                RunTo     (stepped, address);
                FreeRunTo (running, address);

                Assert::IsTrue (stepped.RunOk ("CALLS").text == running.RunOk ("CALLS").text, L"the free run recorded the same chain");
            }
        }



        static CallStackData Calls (Rig & rig)
        {
            Reply  reply = rig.RunOk ("CALLS");



            Assert::IsTrue (std::holds_alternative<CallStackData> (reply.data));
            return std::get<CallStackData> (reply.data);
        }



        static std::vector<CallStackFrame> Frames (const CallStackData & data)
        {
            std::vector<CallStackFrame>  frames;



            for (const CallStackRow & row : data.rows)
            {
                if (row.frame.has_value())
                {
                    frames.push_back (*row.frame);
                }
            }

            return frames;
        }



        static std::vector<CallStackBreak> Breaks (const CallStackData & data)
        {
            std::vector<CallStackBreak>  breaks;



            for (const CallStackRow & row : data.rows)
            {
                if (row.chainBreak.has_value())
                {
                    breaks.push_back (*row.chainBreak);
                }
            }

            return breaks;
        }



        static bool IsSameName (const std::string & a, const std::string & b)
        {
            return _stricmp (a.c_str(), b.c_str()) == 0;
        }



        //  A memory image the walk reads, with SP at `sp`.
        struct Image
        {
            std::vector<Byte>  bytes = std::vector<Byte> (0x10000, 0);

            CallStackPeek  Peek() const { return [this] (Word address) { return bytes[address]; }; }

            void  Jsr (Word at, Word target)
            {
                bytes[at]                   = kJsr;
                bytes[(Word) (at + 1)]      = (Byte) (target & 0xFF);
                bytes[(Word) (at + 2)]      = (Byte) (target >> 8);
            }

            void  Word16 (Word at, Word value)
            {
                bytes[at]                   = (Byte) (value & 0xFF);
                bytes[(Word) (at + 1)]      = (Byte) (value >> 8);
            }
        };



        //  The walk.

        TEST_METHOD (WalkFindsAReturnByTheOpcodeTwoBytesBeforeIt)
        {
            Image                        image;
            std::vector<CallStackFrame>  frames;



            image.Jsr    (0x0900, 0x0A00);
            image.Jsr    (0x0950, 0x0B00);
            image.Word16 (0x01FC, 0x0902);          //  the JSR at $0900 pushed its third byte
            image.Word16 (0x01FE, 0x0952);
            frames = StackWalker::Walk (0xFB, image.Peek(), nullptr);

            Assert::AreEqual ((size_t) 2, frames.size());
            Assert::AreEqual ((Word) 0x0900, frames[0].callSite, L"innermost first");
            Assert::AreEqual ((Word) 0x0A00, frames[0].target);
            Assert::AreEqual ((Byte) 0xFD,   frames[0].stackLevel);
            Assert::IsTrue   (frames[0].provenance == CallProvenance::Guessed);
            Assert::AreEqual ((Word) 0x0950, frames[1].callSite);
        }


        TEST_METHOD (WalkRejectsAWordThatIsTheReturnAddressItself)
        {
            Image  image;



            //  $0903 is where an RTS lands after the JSR at $0900; the pushed
            //  word is $0902, so a word of $0903 is data, not a return.
            image.Jsr    (0x0900, 0x0A00);
            image.Word16 (0x01FE, 0x0903);

            Assert::IsTrue (StackWalker::Walk (0xFD, image.Peek(), nullptr).empty());
        }


        TEST_METHOD (WalkWithADebugFileKeepsOnlyCallsToRoutineEntries)
        {
            Image                        image;
            std::set<Word>               entries = { 0x0B00 };
            std::vector<CallStackFrame>  frames;



            image.Jsr    (0x0900, 0x0A00);
            image.Jsr    (0x0950, 0x0B00);
            image.Word16 (0x01FC, 0x0902);
            image.Word16 (0x01FE, 0x0952);
            frames = StackWalker::Walk (0xFB, image.Peek(), &entries);

            Assert::AreEqual ((size_t) 1, frames.size());
            Assert::AreEqual ((Word) 0x0950, frames[0].callSite);
        }


        TEST_METHOD (WalkEndsAtTheTopOfTheStackPage)
        {
            Image  image;



            //  A return whose high byte would be $0200 is past the page.
            image.Jsr (0x0900, 0x0A00);
            image.bytes[0x01FF] = 0x02;
            image.bytes[0x0200] = 0x09;

            Assert::IsTrue (StackWalker::Walk (0xFE, image.Peek(), nullptr).empty());
        }


        //  The recorder, over the fixture.

        TEST_METHOD (ThreeCallsDeepAreRecordedInnermostFirst)
        {
            Rig                          rig;
            Program                      program = Load (rig, "deep");
            CallStackData                data;
            std::vector<CallStackFrame>  frames;



            RunTo (rig, program.Symbol ("threein"));
            data   = Calls (rig);
            frames = Frames (data);

            Assert::IsTrue   (data.mechanism == CallStackMechanism::Hybrid, L"hybrid is the default");
            Assert::AreEqual ((size_t) 3, frames.size());
            Assert::AreEqual (program.Symbol ("two"),  frames[0].callSite);
            Assert::AreEqual (program.Symbol ("one"),  frames[1].callSite);
            Assert::AreEqual (program.Symbol ("deep"), frames[2].callSite);
            Assert::IsTrue   (IsSameName ("three", frames[0].symbol), L"the target's symbol");

            for (const CallStackFrame & frame : frames)
            {
                Assert::IsTrue (frame.provenance == CallProvenance::Recorded);
                Assert::IsTrue (frame.isVerified);
            }

            Assert::IsTrue   (data.rows[3].chainBreak.has_value() && data.rows[3].chainBreak->kind == CallBreakKind::PowerOn,
                              L"below the frames: power-on, since the machine had run nothing before recording began");
            Assert::AreEqual (program.Symbol ("deep"), data.rows[3].chainBreak->pc);
        }


        TEST_METHOD (TheWalkFindsTheSameChainGuessed)
        {
            Rig                          rig;
            Program                      program = Load (rig, "deep");
            std::vector<CallStackFrame>  frames;



            RunTo (rig, program.Symbol ("threein"));
            rig.RunOk ("CALLS MODE WALK");
            frames = Frames (Calls (rig));

            Assert::AreEqual ((size_t) 3, frames.size());
            Assert::AreEqual (program.Symbol ("two"),  frames[0].callSite);
            Assert::AreEqual (program.Symbol ("deep"), frames[2].callSite);

            for (const CallStackFrame & frame : frames)
            {
                Assert::IsTrue (frame.provenance == CallProvenance::Guessed);
            }

            Assert::IsTrue (Breaks (Calls (rig)).empty(), L"the walk has no breaks of its own");
        }


        TEST_METHOD (ARecursiveRoutineIsAFrameEachTime)
        {
            Rig                          rig;
            Program                      program = Load (rig, "recur");
            std::vector<CallStackFrame>  frames;



            RunTo (rig, program.Symbol ("recbot"));
            frames = Frames (Calls (rig));

            Assert::AreEqual ((size_t) 3, frames.size());
            Assert::AreEqual (program.Symbol ("rec"), frames[0].target);
            Assert::AreEqual (program.Symbol ("rec"), frames[1].target);
            Assert::AreEqual (program.Symbol ("rec"), frames[2].target);
            Assert::AreEqual ((Word) (program.Symbol ("recur") + 2), frames[2].callSite, L"the outermost call is recur's");
        }


        TEST_METHOD (APullIntoTheReturnAddressIsABreakWhileItLasts)
        {
            Rig                          rig;
            Program                      program = Load (rig, "inl");
            CallStackData                data;
            std::vector<CallStackBreak>  breaks;



            RunTo (rig, (Word) (program.Symbol ("inline") + 1));
            data   = Calls (rig);
            breaks = Breaks (data);

            Assert::IsTrue   (breaks[0].kind == CallBreakKind::PulledReturn);
            Assert::AreEqual (program.Symbol ("inline"), breaks[0].pc);
            Assert::AreEqual ((Byte) 0x68,               breaks[0].opcode);
            Assert::IsTrue   (data.rows[0].chainBreak.has_value(), L"the break is above the frame it doubts");
            Assert::IsFalse  (Frames (data)[0].isVerified);
        }


        TEST_METHOD (AReturnPastInlineParametersIsNotedNotBroken)
        {
            Rig            rig;
            Program        program = Load (rig, "inl");
            CallStackData  data;



            RunTo (rig, program.Symbol ("inlback"));
            data = Calls (rig);

            Assert::IsTrue   (Frames (data).empty());
            Assert::AreEqual ((size_t) 1, Breaks (data).size(), L"only where recording began");
            Assert::IsTrue   (data.lastReturn.has_value());
            Assert::AreEqual (std::string ("returned past inline parameters"), data.lastReturn->note);
            Assert::AreEqual (program.Symbol ("inline"), data.lastReturn->target);
            Assert::IsTrue   (rig.RunOk ("CALLS").text.back().find ("returned past inline parameters") != std::string::npos);
        }


        TEST_METHOD (AFrameEndedByAJumpIsABreakAtTheJump)
        {
            Rig            rig;
            Program        program = Load (rig, "away");
            CallStackData  data;



            RunTo (rig, program.Symbol ("awayto"));
            data = Calls (rig);

            Assert::IsTrue   (Frames (data).empty(), L"the frame ended at the jump");
            Assert::IsTrue   (data.rows[0].chainBreak->kind == CallBreakKind::EndedByJump);
            Assert::AreEqual (program.Symbol ("disjmp"), data.rows[0].chainBreak->pc);
            Assert::AreEqual ((Byte) 0x4C,               data.rows[0].chainBreak->opcode);
        }


        TEST_METHOD (TxsIsABreakAndTheFramesBelowAreUnverified)
        {
            Rig            rig;
            Program        program = Load (rig, "reload");
            CallStackData  data;



            RunTo (rig, program.Symbol ("txsnext"));
            data = Calls (rig);

            Assert::IsTrue   (data.rows[0].chainBreak->kind == CallBreakKind::Txs);
            Assert::AreEqual (program.Symbol ("txsat"), data.rows[0].chainBreak->pc);
            Assert::AreEqual ((Byte) 0x9A,              data.rows[0].chainBreak->opcode);
            Assert::AreEqual (program.Symbol ("reload"), data.rows[1].frame->callSite);
            Assert::IsFalse  (data.rows[1].frame->isVerified);
            Assert::AreEqual (std::format ("-- TXS at ${:04X} --", program.Symbol ("txsat")), rig.RunOk ("CALLS").text.at (0));
        }


        TEST_METHOD (AReturnAddressRewrittenInPlaceMarksTheFrameAtTheStore)
        {
            Rig                          rig;
            Program                      program = Load (rig, "rewrite");
            std::vector<CallStackFrame>  frames;
            CallStackData                data;



            RunTo (rig, program.Symbol ("rewrrts"));
            frames = Frames (Calls (rig));

            Assert::AreEqual ((size_t) 1, frames.size());
            Assert::IsTrue   (frames[0].isRewritten, L"marked when the store ran, before the RTS");
            Assert::AreEqual (std::format ("return address changed by the store at ${:04X}", program.Symbol ("rewrsta")), frames[0].note);

            RunTo (rig, program.Symbol ("rewrto"));
            data = Calls (rig);

            Assert::IsTrue   (Frames (data).empty(), L"the RTS ended the frame");
            Assert::AreEqual ((size_t) 1, Breaks (data).size(), L"and was not a mismatch: the store chose where it went");
        }


        TEST_METHOD (APushIsNotAStoreIntoAFrame)
        {
            Rig                          rig;
            Program                      program = Load (rig, "inl");
            std::vector<CallStackFrame>  frames;



            RunTo (rig, program.Symbol ("inlrts"));
            frames = Frames (Calls (rig));

            Assert::AreEqual ((size_t) 1, frames.size());
            Assert::IsFalse  (frames[0].isRewritten, L"the routine pulled its return address and pushed a new one: pushes, not stores");
        }


        TEST_METHOD (TasAndLasReloadTheStackAsTxsDoes)
        {
            std::array<Byte, 0x10000>  memory   = {};
            CallStackRecorder          recorder;
            CallStackData              data;



            recorder.SetPeek ([&memory] (Word address) { return memory[address]; });
            memory[0x01FE] = 0x02;
            memory[0x01FF] = 0x08;

            recorder.Begin         (0x0800, kJsr);
            recorder.OnInstruction (0x0800, 0xFF, kJsr);
            recorder.OnInstruction (0x0900, 0xFD, 0x9B);
            recorder.Settle        (0x0901, 0xFD);

            Assert::AreEqual ((size_t) 1, recorder.GetBreaks().size(), L"a TAS that left SP alone, as a 65C02's NOP does, is nothing");

            recorder.OnInstruction (0x0901, 0xFD, 0x9B);
            recorder.Settle        (0x0902, 0x80);

            Assert::AreEqual ((size_t) 2, recorder.GetBreaks().size());
            Assert::IsTrue   (recorder.GetBreaks().back().info.kind == CallBreakKind::Txs);
            Assert::AreEqual (std::string ("TAS at $0901"), CallStack::DescribeBreak (recorder.GetBreaks().back().info));

            recorder.OnInstruction (0x0902, 0x80, 0xBB);
            recorder.Settle        (0x0905, 0x40);

            Assert::AreEqual (std::string ("LAS at $0902"), CallStack::DescribeBreak (recorder.GetBreaks().back().info));
        }


        TEST_METHOD (AStackPointerThatWrapsIsABreakAtThePush)
        {
            Rig            rig;
            Program        program = Load (rig, "wrap");
            CallStackData  data;



            RunTo (rig, program.Symbol ("wrapend"));
            data = Calls (rig);

            Assert::IsTrue   (data.rows[0].chainBreak->kind == CallBreakKind::StackWrap);
            Assert::AreEqual (program.Symbol ("wrappha"), data.rows[0].chainBreak->pc);
            Assert::AreEqual ((Byte) 0x48,                data.rows[0].chainBreak->opcode);
            Assert::IsFalse  (data.rows[1].frame->isVerified);
        }


        //  A routine calling itself forever wraps the stack every 128 calls.
        //  The frames are capped, and a wrap below the frames kept goes with
        //  them, so the breaks stay as few as the frames that hold them.
        TEST_METHOD (ARunawayRecursionKeepsItsBreaksBounded)
        {
            std::array<Byte, 0x10000>  memory = {};
            CallStackRecorder          recorder;
            Byte                       sp     = 0xFF;



            recorder.SetPeek ([&memory] (Word address) { return memory[address]; });
            recorder.Begin   (0x0300, kJsr);

            for (int call = 0; call < 2000; call++)
            {
                recorder.OnInstruction (0x0300, sp, kJsr);

                memory[0x0100 + sp]              = 0x03;
                memory[0x0100 + (Byte) (sp - 1)] = 0x02;
                sp                               = (Byte) (sp - 2);
            }

            recorder.Settle (0x0300, sp);

            Assert::AreEqual ((size_t) 256, recorder.GetFrames().size(), L"the frames are capped");
            Assert::IsTrue   (recorder.GetBreaks().size() <= 4,
                              std::format (L"{} breaks: where recording began and one wrap per 128 frames kept, not one per wrap ever",
                                           recorder.GetBreaks().size()).c_str());
        }


        TEST_METHOD (AnInterruptIsAFramePushedOnDispatchAndPoppedOnRti)
        {
            Rig                          rig;
            Program                      program = Load (rig, "irqwait");
            Word                         handler = program.Symbol ("irqh");
            std::vector<CallStackFrame>  frames;
            auto                         isIrq   = [] (const CallStackFrame & frame) { return frame.kind == CallFrameKind::Irq; };



            rig.target.TryPoke (kIrqUserVector,     (Byte) (handler & 0xFF));
            rig.target.TryPoke (kIrqUserVector + 1, (Byte) (handler >> 8));
            rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kMaskable, true);

            RunTo (rig, handler);
            frames = Frames (Calls (rig));

            Assert::IsTrue   (std::any_of (frames.begin(), frames.end(), isIrq), L"the interrupt is a frame");
            Assert::AreEqual (program.Symbol ("irqspin"), std::find_if (frames.begin(), frames.end(), isIrq)->callSite,
                              L"its call site is the interrupted instruction");

            rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kMaskable, false);
            RunTo (rig, program.Symbol ("irqspin"));
            frames = Frames (Calls (rig));

            Assert::IsFalse  (std::any_of (frames.begin(), frames.end(), isIrq), L"RTI popped it");
            Assert::AreEqual ((size_t) 1, Breaks (Calls (rig)).size(), L"and the return was clean");
        }


        //  Out of a handler is back at the instruction the interrupt stopped.
        //  The enhanced //e's ROM pushes a return frame of its own before
        //  jumping to the handler, so the handler's RTI lands in the ROM, and
        //  the ROM then pulls bytes and jumps before its own RTI; neither is
        //  the way out. The interrupt's frame ending is.
        TEST_METHOD (StepOutOfAnInterruptHandlerReturnsToTheInterruptedInstruction)
        {
            for (const char * machine : { "Apple2e", "Apple2eEnhanced" })
            {
                Rig      rig (machine);
                Program  program = Load (rig, "irqwait");
                Word     handler = program.Symbol ("irqh");



                rig.target.TryPoke (kIrqUserVector,     (Byte) (handler & 0xFF));
                rig.target.TryPoke (kIrqUserVector + 1, (Byte) (handler >> 8));
                rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kMaskable, true);

                RunTo (rig, handler);
                rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kMaskable, false);

                rig.RunOk ("RTS");

                Assert::AreEqual (program.Symbol ("irqspin"), rig.LastStop().pc,
                                  std::format (L"{}: one step out of the handler", machine[7] == 'E' ? L"enhanced //e" : L"//e").c_str());
            }
        }


        //  Out of a routine that pulls its return address and jumps away ends
        //  where the jump lands, as the frame ends there.
        TEST_METHOD (StepOutOfARoutineThatJumpsAwayStopsWhereItLands)
        {
            Rig      rig;
            Program  program = Load (rig, "away");



            RunTo (rig, program.Symbol ("discard"));
            rig.RunOk ("RTS");

            Assert::AreEqual (program.Symbol ("awayto"), rig.LastStop().pc);
        }


        TEST_METHOD (AResetDiscardsTheRecord)
        {
            Rig            rig;
            Program        program = Load (rig, "deep");
            CallStackData  data;



            RunTo (rig, program.Symbol ("threein"));
            rig.session.OnReset (false);
            data = Calls (rig);

            Assert::IsTrue (Frames (data).empty(), L"no frame survives, and nothing is walked below a reset");
            Assert::AreEqual ((size_t) 1, data.rows.size());
            Assert::IsTrue (data.rows[0].chainBreak->kind == CallBreakKind::Reset);
        }


        TEST_METHOD (APowerCycleIsTheBottomOfTheChainAndNothingIsWalkedBelowIt)
        {
            Rig            rig;
            Program        program = Load (rig, "deep");
            CallStackData  data;



            RunTo (rig, program.Symbol ("threein"));
            rig.session.OnReset (true);
            data = Calls (rig);

            Assert::AreEqual ((size_t) 1, data.rows.size(), L"hybrid walks nothing below power-on");
            Assert::IsTrue   (data.rows[0].chainBreak->kind == CallBreakKind::PowerOn);
            Assert::AreEqual (std::format ("power-on at ${:04X}, cycle 0", data.rows[0].chainBreak->pc), CallStack::DescribeBreak (*data.rows[0].chainBreak));
        }


        TEST_METHOD (HybridWalksBelowWhereRecordingBegan)
        {
            Rig            rig;
            Program        program = Load (rig, "deep");
            CallStackData  data;



            //  Two calls made before the debugger attached.
            RunTo (rig, program.Symbol ("two"));
            rig.session.SetCallRecording (false);
            rig.session.SetCallRecording (true);

            RunTo (rig, program.Symbol ("threein"));
            data = Calls (rig);

            Assert::AreEqual ((size_t) 4, data.rows.size());
            Assert::IsTrue   (data.rows[0].frame->provenance == CallProvenance::Recorded);
            Assert::IsTrue   (data.rows[0].frame->isVerified);
            Assert::IsTrue   (data.rows[1].chainBreak->kind == CallBreakKind::TrackingBegan, L"the boundary is marked");
            Assert::IsTrue   (data.rows[2].frame->provenance == CallProvenance::Guessed);
            Assert::AreEqual (program.Symbol ("one"),  data.rows[2].frame->callSite);
            Assert::AreEqual (program.Symbol ("deep"), data.rows[3].frame->callSite);
            Assert::IsFalse  (data.rows[3].frame->isVerified);

            rig.RunOk ("CALLS MODE RECORDED");
            Assert::AreEqual ((size_t) 2, Calls (rig).rows.size(), L"recorded alone stops at the boundary");
        }


        //  Free runs, where the record hears only of the instructions the CPU
        //  reports, record what debugger runs record.

        TEST_METHOD (AFreeRunRecordsEachCaseAsADebuggerRunDoes)
        {
            AssertFreeRunRecordsTheSame ("deep",    { "two", "threein" });
            AssertFreeRunRecordsTheSame ("recur",   { "recbot" });
            AssertFreeRunRecordsTheSame ("inl",     { "inlrts", "inlback" });
            AssertFreeRunRecordsTheSame ("away",    { "disjmp", "awayto" });
            AssertFreeRunRecordsTheSame ("reload",  { "txsnext" });
            AssertFreeRunRecordsTheSame ("rewrite", { "rewrrts", "rewrto" });
            AssertFreeRunRecordsTheSame ("wrap",    { "wrapend" });
        }


        TEST_METHOD (AFreeRunDetectsTxs)
        {
            Rig      rig;
            Program  program = Load (rig, "reload");



            FreeRunTo (rig, program.Symbol ("txsnext"));
            Assert::AreEqual (std::format ("-- TXS at ${:04X} --", program.Symbol ("txsat")), rig.RunOk ("CALLS").text.at (0));
        }


        TEST_METHOD (AFreeRunRecordsAnInterruptTakenInALoopWithNoStackOpcode)
        {
            static constexpr Word        kStart      = 0x0900;
            static constexpr Word        kLoop       = 0x0901;
            static constexpr Word        kHandler    = 0x0A00;
            static constexpr Byte        kCli        = 0x58;
            static constexpr Byte        kBne        = 0xD0;
            static constexpr Byte        kNop        = 0xEA;
            static constexpr Byte        kRti        = 0x40;
            static constexpr int         kNops       = 16;
            static constexpr uint64_t    kSpinCycles = 1000;
            Rig                          rig;
            std::vector<CallStackFrame>  frames;
            Word                         at          = kLoop;
            Word                         interrupted = 0;
            auto                         isIrq       = [] (const CallStackFrame & frame) { return frame.kind == CallFrameKind::Irq; };



            //  CLI, then sixteen NOPs and a BNE back to the first, taken with Z
            //  clear from the load: the loop the interrupt lands in has no
            //  opcode the filter marks, and many instructions it can land on.
            rig.Load (kStart, { kCli }, kStart);

            for (int i = 0; i < kNops; ++i)
            {
                rig.target.TryPoke (at++, kNop);
            }

            rig.target.TryPoke (at,     kBne);
            rig.target.TryPoke (at + 1, (Byte) (kLoop - (at + 2)));
            rig.target.TryPoke (kHandler,     kNop);
            rig.target.TryPoke (kHandler + 1, kRti);
            rig.target.TryPoke (kIrqUserVector,     (Byte) (kHandler & 0xFF));
            rig.target.TryPoke (kIrqUserVector + 1, (Byte) (kHandler >> 8));
            rig.session.SetCallRecording (true);

            FreeRunTo (rig, kLoop);
            rig.machine.RunCycles (kSpinCycles);
            interrupted = rig.machine.GetCpu()->GetPC();
            Assert::AreNotEqual (kLoop, interrupted, L"the interrupt lands on an instruction the recorder was not shown");

            rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kMaskable, true);
            FreeRunTo (rig, kHandler);
            frames = Frames (Calls (rig));

            Assert::IsTrue   (std::any_of (frames.begin(), frames.end(), isIrq), L"the interrupt is a frame");
            Assert::AreEqual (interrupted, std::find_if (frames.begin(), frames.end(), isIrq)->callSite, L"its call site is the interrupted instruction");

            rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kMaskable, false);
            FreeRunTo (rig, kLoop);
            frames = Frames (Calls (rig));

            Assert::IsFalse (std::any_of (frames.begin(), frames.end(), isIrq), L"RTI popped it");
        }


        TEST_METHOD (RecordingAloneInstallsNoPerInstructionHook)
        {
            Rig                      rig;
            const DebugHookFilter  & filter = rig.session.GetFilter();



            rig.session.SetCallRecording (true);
            Assert::IsNull (rig.machine.GetDebugHook(), L"the CPU reports what the record needs");

            rig.session.GetBreakpoints().AddAddress (0x1234, 0x1234);
            rig.session.OnStopConditionsChanged();
            Assert::IsNotNull (rig.machine.GetDebugHook());
            Assert::IsFalse   (filter.everyInstruction);
            Assert::IsTrue    (filter.pages[0x12]);
            Assert::IsFalse   (filter.pages[0x13], L"only the breakpoint's page is asked about");

            rig.session.GetBreakpoints().AddCondition (Expression());
            rig.session.OnStopConditionsChanged();
            Assert::IsTrue (filter.everyInstruction, L"a register condition is tested before every instruction");
            Assert::IsTrue (filter.pages[0x13]);

            rig.session.ClearAllBreakpoints();
            Assert::IsNull (rig.machine.GetDebugHook());
        }


        TEST_METHOD (AFreeRunStopsAtABreakpointWithAndWithoutRecording)
        {
            static constexpr uint64_t  kBudget = 1'000'000;



            for (bool isRecording : { false, true })
            {
                Rig      rig;
                Program  program = Load (rig, "deep");
                Word     stop    = program.Symbol ("threein");



                rig.session.SetCallRecording (isRecording);
                rig.session.GetBreakpoints().AddAddress (stop, stop);
                rig.session.OnStopConditionsChanged();
                rig.machine.RunCycles (kBudget);

                Assert::AreEqual (stop, rig.machine.GetCpu()->GetPC(), L"the free run stopped at the breakpoint");

                if (isRecording)
                {
                    Assert::AreEqual ((size_t) 3, Frames (Calls (rig)).size(), L"and recorded the three calls to it");
                }
            }
        }
    };
}