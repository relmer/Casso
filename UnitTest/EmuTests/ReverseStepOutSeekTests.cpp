#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/CallStack.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr size_t    s_kSeekWaitSteps    = 60000;
static constexpr size_t    s_kSeekStepLimit    = 400000;
static constexpr uint64_t  s_kSeekMinCycles    = 2;
static constexpr uint64_t  s_kSeekStretchWork  = KeyframeSettings::kFrameCycles / s_kSeekMinCycles;
static constexpr size_t    s_kSeekFarStretches = 4;

//  The callers: a JSR at the top level, and one inside it to the routine
//  that waits. The waiting routine's entry is written in by each test, just
//  ahead of the wait at $0A30, so a prologue of its own runs first.
static constexpr Byte  s_kSeekCallers[] =
{
    0x20, 0x10, 0x0A,       // 0A00  JSR $0A10
    0x4C, 0x00, 0x0A,       // 0A03  JMP $0A00
    0x00, 0x00, 0x00,       // 0A06
    0x00, 0x00, 0x00,       // 0A09
    0x00, 0x00, 0x00,       // 0A0C
    0x00,                   // 0A0F
    0x20, 0x00, 0x0A,       // 0A10  JSR <entry>, written by the test
    0x60,                   // 0A13  RTS
};

//  About thirty frames of DEC and BNE, then a return.
static constexpr Byte  s_kSeekWait[] =
{
    0xA9, 0x00,             // 0A30  LDA #$00
    0x85, 0x07,             // 0A32  STA $07
    0xC6, 0x06,             // 0A34  DEC $06
    0xD0, 0xFC,             // 0A36  BNE $0A34
    0xC6, 0x07,             // 0A38  DEC $07
    0xD0, 0xF8,             // 0A3A  BNE $0A34
    0x60,                   // 0A3C  RTS
};

//  A Mockingboard timer interrupt, once, then a spin at the top level. The
//  handler waits as above and returns.
static constexpr Byte  s_kSeekIrqMain[] =
{
    0xA9, 0x00,             // 0A00  LDA #$00
    0x8D, 0x0B, 0xC4,       // 0A02  STA $C40B    ACR: one shot
    0xA9, 0xC0,             // 0A05  LDA #$C0
    0x8D, 0x0E, 0xC4,       // 0A07  STA $C40E    IER: timer 1
    0xA9, 0x00,             // 0A0A  LDA #$00
    0x8D, 0x04, 0xC4,       // 0A0C  STA $C404    timer 1 low latch
    0xA9, 0x08,             // 0A0F  LDA #$08
    0x8D, 0x05, 0xC4,       // 0A11  STA $C405    timer 1 high: start
    0x58,                   // 0A14  CLI
    0x4C, 0x15, 0x0A,       // 0A15  JMP $0A15
};

static constexpr Byte  s_kSeekIrqHandler[] =
{
    0xAD, 0x04, 0xC4,       // 0B00  LDA $C404    acknowledge
    0xA9, 0x00,             // 0B03  LDA #$00
    0x85, 0x07,             // 0B05  STA $07
    0xC6, 0x06,             // 0B07  DEC $06
    0xD0, 0xFC,             // 0B09  BNE $0B07
    0xC6, 0x07,             // 0B0B  DEC $07
    0xD0, 0xF8,             // 0B0D  BNE $0B07
    0xEA, 0xEA,             // 0B0F  NOP, NOP
    0x40,                   // 0B11  RTI
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepOutSeekTests
//
//  Live, step back out seeks straight to the call the call record holds,
//  instead of searching back through history for it. Each test runs a
//  routine that waits many stretches after its call, then checks the seek
//  against the search: the same landing, for one stretch of replay at most,
//  where the search replays every stretch back to the call. The record is
//  fed every instruction, as the debugger feeds it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseStepOutSeekTests)
{
public:

    static constexpr Word  kTop            = 0x0A00;
    static constexpr Word  kInnerCall      = 0x0A10;
    static constexpr Word  kEntryField     = 0x0A11;
    static constexpr Word  kWait           = 0x0A30;
    static constexpr Word  kWaitLoop       = 0x0A34;
    static constexpr Word  kSpin           = 0x0A15;
    static constexpr Word  kHandler        = 0x0B00;
    static constexpr Word  kIrqLoop        = 0x0B07;
    static constexpr Word  kIrqVector      = 0xFFFE;
    static constexpr Word  kLcRamReadWrite = 0xC08B;


    //  Out of the waiting routine to the JSR that called it, then out of
    //  that one to the JSR at the top level, the second from the landing of
    //  the first. A short call the routine made and returned from first is
    //  passed over.
    TEST_METHOD (StepBackOutSeeksToEachEnclosingCall)
    {
        static constexpr Byte    kShortCall[] = { 0x20, 0x3D, 0x0A };    // JSR $0A3D, an RTS written past the wait
        static constexpr Byte    kRts         = 0x60;
        static constexpr size_t  kOuts        = 2;
        TestMachine                  machine    ("Apple2e");
        ReverseController            controller (machine);
        CallStackRecorder            recorder;
        std::vector<ReverseResult>   fast;



        Prepare (machine, kShortCall, sizeof (kShortCall));
        machine.GetMemoryBus().WriteByte (kWait + sizeof (s_kSeekWait), kRts);

        RunToWait (machine, controller, recorder, kWaitLoop);

        fast = CheckAgainstSearch (machine, controller, recorder, kOuts);

        Assert::AreEqual<Word> (kTop, machine.GetCpu()->GetPC(), L"the second lands on the JSR at the top level");
        Assert::IsTrue         (fast[1].position < fast[0].position, L"the first landed on the inner JSR, after it");
    }


    //  The routine pulls its own return address. The record keeps its frame,
    //  but the stack has risen above where the call left it, so the search
    //  passes it over, and the seek must too.
    TEST_METHOD (StepBackOutPassesOverACallWhoseReturnAddressWasPulled)
    {
        static constexpr Byte  kPull[] = { 0x68, 0x68 };    // PLA, PLA
        TestMachine            machine    ("Apple2e");
        ReverseController      controller (machine);
        CallStackRecorder      recorder;



        Prepare (machine, kPull, sizeof (kPull));
        RunToWait (machine, controller, recorder, kWaitLoop);

        Assert::AreEqual<size_t> (2, recorder.GetFrames().size(), L"the record still holds the pulled frame");

        CheckAgainstSearch (machine, controller, recorder, 1);

        Assert::AreEqual<Word> (kTop, machine.GetCpu()->GetPC(), L"out past the pulled call to the top level");
    }


    //  The routine resets the stack to where it stood before its own call,
    //  which ends that frame; the enclosing call is the way out.
    TEST_METHOD (StepBackOutPassesOverACallTheStackWasResetPast)
    {
        static constexpr Byte  kReset[] = { 0xBA, 0xE8, 0xE8, 0x9A };    // TSX, INX, INX, TXS
        TestMachine            machine    ("Apple2e");
        ReverseController      controller (machine);
        CallStackRecorder      recorder;



        Prepare (machine, kReset, sizeof (kReset));
        RunToWait (machine, controller, recorder, kWaitLoop);

        CheckAgainstSearch (machine, controller, recorder, 1);

        Assert::AreEqual<Word> (kTop, machine.GetCpu()->GetPC(), L"out to the call the reset left");
    }


    //  Out of an interrupt handler to the instruction the interrupt stopped.
    //  The vector is in Language Card RAM and goes straight to the handler:
    //  the ROM's dispatch pulls and pushes the status byte to look at it,
    //  which lifts the stack above where the interrupt left it, and by the
    //  stack-pointer rule that ends the interrupt's frame.
    TEST_METHOD (StepBackOutOfAnInterruptHandlerSeeksToTheInterruptedInstruction)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        CallStackRecorder  recorder;
        Byte               ignored    = 0;
        size_t             i          = 0;



        ReverseSessionRig::Prepare (machine);

        for (i = 0; i < sizeof (s_kSeekIrqMain); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (kTop + i), s_kSeekIrqMain[i]);
        }

        for (i = 0; i < sizeof (s_kSeekIrqHandler); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (kHandler + i), s_kSeekIrqHandler[i]);
        }

        //  Two reads read and write-enable the Language Card RAM.
        ignored = machine.GetMemoryBus().ReadByte (kLcRamReadWrite);
        ignored = machine.GetMemoryBus().ReadByte (kLcRamReadWrite);
        IGNORE_RETURN_VALUE (ignored, 0);

        machine.GetMemoryBus().WriteByte (kIrqVector,     static_cast<Byte> (kHandler & 0xFF));
        machine.GetMemoryBus().WriteByte (kIrqVector + 1, static_cast<Byte> (kHandler >> 8));
        machine.GetCpu()->SetPC (kTop);

        RunToWait (machine, controller, recorder, kIrqLoop);

        CheckAgainstSearch (machine, controller, recorder, 1);

        Assert::AreEqual<Word> (kSpin, machine.GetCpu()->GetPC(), L"out to the interrupted instruction");
    }


    //  A link the landing does not match is no answer: the search runs and
    //  lands where it always did.
    TEST_METHOD (AMismatchedLinkFallsBackToTheSearch)
    {
        static constexpr Byte  kNone[] = { 0xEA };    // NOP
        TestMachine            machine    ("Apple2e");
        ReverseController      controller (machine);
        CallStackRecorder      recorder;
        ReverseResult          result;
        HRESULT                hr         = S_OK;
        uint64_t               replayed   = 0;



        Prepare (machine, kNone, sizeof (kNone));
        RunToWait (machine, controller, recorder, kWaitLoop);

        controller.SetCallerLinksProbe ([&machine, &recorder] (std::vector<CallerLink> & outLinks)
        {
            recorder.Settle (machine.GetCpu()->GetPC(), machine.GetCpu()->GetSP());
            recorder.GetCallerLinks (outLinks);

            Assert::IsFalse (outLinks.empty(), L"the record holds the call");

            outLinks.back().cycle++;
        });

        replayed = controller.GetReplayer().GetReplayedCount();

        hr = controller.StepBackOut (result);
        AssertSucceeded (hr, L"StepBackOut");

        Assert::IsTrue         (result.outcome == ReverseOutcome::Moved, L"the search found the call");
        Assert::AreEqual<Word> (kInnerCall, machine.GetCpu()->GetPC(), L"on the JSR that called the routine");
        Assert::IsTrue         (controller.GetReplayer().GetReplayedCount() - replayed > s_kSeekStretchWork * s_kSeekFarStretches,
                                L"the search replayed history back to the call");
    }


private:

    //  The callers, the wait, and the waiting routine's prologue written in
    //  just ahead of the wait, with the inner JSR pointed at it.
    static void Prepare (TestMachine & machine, const Byte * prologue, size_t length)
    {
        Word    entry = static_cast<Word> (kWait - length);
        size_t  i     = 0;



        ReverseSessionRig::Prepare (machine);

        for (i = 0; i < sizeof (s_kSeekCallers); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (kTop + i), s_kSeekCallers[i]);
        }

        for (i = 0; i < sizeof (s_kSeekWait); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (kWait + i), s_kSeekWait[i]);
        }

        for (i = 0; i < length; i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (entry + i), prologue[i]);
        }

        machine.GetMemoryBus().WriteByte (kEntryField,     static_cast<Byte> (entry & 0xFF));
        machine.GetMemoryBus().WriteByte (kEntryField + 1, static_cast<Byte> (entry >> 8));
        machine.GetCpu()->SetPC (kTop);
    }


    //  Recording and the record begun where the machine stands, then on into
    //  the wait and many stretches through it.
    static void RunToWait (TestMachine & machine, ReverseController & controller, CallStackRecorder & recorder, Word loop)
    {
        HRESULT  hr = S_OK;
        Word     pc = machine.GetCpu()->GetPC();



        recorder.SetPeek  ([&machine] (Word address) { return machine.GetMemoryBus().ReadByte (address); });
        recorder.SetClock ([&machine] { return machine.GetCpu()->GetTotalCycles(); });
        recorder.Begin    (pc, machine.GetMemoryBus().ReadByte (pc));

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        controller.SetCallerProbe ([&machine, &recorder] (uint64_t historyStartCycle)
        {
            recorder.Settle (machine.GetCpu()->GetPC(), machine.GetCpu()->GetSP());

            return recorder.HasNoCallSince (historyStartCycle);
        });

        SetLinksProbe (machine, controller, recorder);

        RunRecordedTo (machine, recorder, loop);
        RunRecorded   (machine, recorder, s_kSeekWaitSteps);
    }


    static void SetLinksProbe (TestMachine & machine, ReverseController & controller, CallStackRecorder & recorder)
    {
        controller.SetCallerLinksProbe ([&machine, &recorder] (std::vector<CallerLink> & outLinks)
        {
            recorder.Settle (machine.GetCpu()->GetPC(), machine.GetCpu()->GetSP());
            recorder.GetCallerLinks (outLinks);
        });
    }


    //  count step back outs with the record's links, then, back at the end
    //  of history, as many without, which leaves them to the search. Each
    //  pair lands on the same position with the same outcome; each seek
    //  loads one keyframe and replays at most one stretch, and the search
    //  replays many more and builds no step table. The machine is left where
    //  the last search landed.
    static std::vector<ReverseResult> CheckAgainstSearch (
        TestMachine        & machine,
        ReverseController  & controller,
        CallStackRecorder  & recorder,
        size_t               count)
    {
        std::vector<ReverseResult>  fast   (count);
        std::vector<ReverseResult>  search (count);
        ReverseResult               result;
        HRESULT                     hr       = S_OK;
        uint64_t                    liveEnd  = machine.GetPosition();
        uint64_t                    replayed = 0;
        size_t                      restores = 0;
        size_t                      builds   = 0;
        size_t                      i        = 0;



        for (i = 0; i < count; i++)
        {
            replayed = controller.GetReplayer().GetReplayedCount();
            restores = controller.GetReplayer().GetRestoreCount();

            hr = controller.StepBackOut (fast[i]);
            AssertSucceeded (hr, L"StepBackOut by the record");

            Assert::IsTrue (controller.GetReplayer().GetReplayedCount() - replayed <= s_kSeekStretchWork,
                            std::format (L"step back out {} replayed {} instructions, more than one stretch",
                                         i + 1, controller.GetReplayer().GetReplayedCount() - replayed).c_str());
            Assert::AreEqual<size_t> (restores + 1, controller.GetReplayer().GetRestoreCount(), L"one keyframe loaded");
        }

        hr = controller.SeekToPosition (liveEnd, result);
        AssertSucceeded (hr, L"SeekToPosition at the end of history");
        Assert::IsFalse (controller.IsInHistory(), L"live again");

        controller.SetCallerLinksProbe (nullptr);

        replayed = controller.GetReplayer().GetReplayedCount();
        builds   = controller.GetTableBuildCount();

        for (i = 0; i < count; i++)
        {
            hr = controller.StepBackOut (search[i]);
            AssertSucceeded (hr, L"StepBackOut by the search");

            Assert::IsTrue             (search[i].outcome == ReverseOutcome::Moved, L"the search found a call");
            Assert::IsTrue             (fast[i].outcome == search[i].outcome,       L"the same outcome");
            Assert::AreEqual<uint64_t> (search[i].position, fast[i].position,       std::format (L"step back out {} lands at {}, where the search does, not {}", i + 1, search[i].position, fast[i].position).c_str());
        }

        Assert::IsTrue (controller.GetReplayer().GetReplayedCount() - replayed > s_kSeekStretchWork * s_kSeekFarStretches,
                        L"the call is many stretches back");
        Assert::AreEqual<size_t> (builds, controller.GetTableBuildCount(), L"the search builds no step table");

        SetLinksProbe (machine, controller, recorder);

        return fast;
    }


    static void RunRecorded (TestMachine & machine, CallStackRecorder & recorder, size_t count)
    {
        size_t  i  = 0;
        Word    pc = 0;



        for (i = 0; i < count; i++)
        {
            pc = machine.GetCpu()->GetPC();

            recorder.OnInstruction (pc, machine.GetCpu()->GetSP(), machine.GetMemoryBus().ReadByte (pc));
            machine.StepOne();
        }
    }


    static void RunRecordedTo (TestMachine & machine, CallStackRecorder & recorder, Word pc)
    {
        size_t  i = 0;



        RunRecorded (machine, recorder, 1);

        for (i = 0; i < s_kSeekStepLimit && machine.GetCpu()->GetPC() != pc; i++)
        {
            RunRecorded (machine, recorder, 1);
        }

        Assert::AreEqual<Word> (pc, machine.GetCpu()->GetPC(), std::format (L"the run reached ${:04X}", pc).c_str());
    }
};
