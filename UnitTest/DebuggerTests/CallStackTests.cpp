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

            Assert::IsTrue   (data.rows[3].chainBreak.has_value() && data.rows[3].chainBreak->kind == CallBreakKind::TrackingBegan,
                              L"below the frames: where recording began");
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


        TEST_METHOD (AReturnAddressRewrittenInPlaceIsABreakAtTheRts)
        {
            Rig            rig;
            Program        program = Load (rig, "rewrite");
            CallStackData  data;



            RunTo (rig, program.Symbol ("rewrto"));
            data = Calls (rig);

            Assert::IsTrue   (Frames (data).empty());
            Assert::IsTrue   (data.rows[0].chainBreak->kind == CallBreakKind::ReturnMismatch);
            Assert::AreEqual (program.Symbol ("rewrrts"), data.rows[0].chainBreak->pc);
            Assert::AreEqual ((Byte) 0x60,                data.rows[0].chainBreak->opcode);
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
    };
}
