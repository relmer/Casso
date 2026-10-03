#include "Pch.h"

#include "HResultAssert.h"
#include "EmuTests/ReverseSessionRig.h"
#include "Core/IMachineState.h"
#include "Core/StateWriter.h"
#include "Debugger/Reverse/ReverseController.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint32_t  s_kReuseTag       = IMachineState::MakeTag ('R', 'E', 'U', 'S');
static constexpr size_t    s_kReuseStale     = 4096;
static constexpr Byte      s_kReuseStaleByte = 0xEE;
static constexpr size_t    s_kReuseSteps     = 400000;
static constexpr uint64_t  s_kReuseSettle    = 200;





////////////////////////////////////////////////////////////////////////////////
//
//  StateWriterReuseTests
//
//  A writer given a used buffer writes over it rather than clearing it first,
//  so the stream must come out the same as from a fresh writer whatever the
//  buffer held; and the buffers reverse execution recycles must stay in its
//  pool rather than leak out of it a capture at a time.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (StateWriterReuseTests)
{
public:

    TEST_METHOD (AReusedBufferGivesTheSameStreamAsAFreshWriter)
    {
        StateWriter        fresh;
        StateWriter        reused;
        std::vector<Byte>  taken;



        WriteSample (fresh);

        reused.Reuse (std::vector<Byte> (s_kReuseStale, s_kReuseStaleByte));
        WriteSample (reused);

        Assert::IsTrue (reused.GetBytes() == fresh.GetBytes(), L"nothing of the old buffer shows in the stream");

        taken = reused.TakeBytes();

        Assert::IsTrue (taken == fresh.GetBytes(), L"and the buffer handed out is cut to the stream");
        Assert::IsTrue (reused.GetBytes().empty(), L"the writer starts over empty after handing it out");

        reused.Reuse (std::move (taken));
        WriteSample (reused);
        WriteSample (fresh);

        Assert::IsTrue (reused.GetBytes() != fresh.GetBytes(), L"a reused writer starts over rather than appending");

        reused.Reuse (std::vector<Byte>());
        WriteSample (reused);
        WriteSample (reused);

        Assert::IsTrue (reused.GetBytes() == fresh.GetBytes(), L"a stream longer than its buffer grows it");
    }


    TEST_METHOD (AKeyframeTakenAloneHandsItsBufferBack)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        ReverseSettings    settings;
        size_t             keyframes  = 0;
        size_t             before     = 0;
        size_t             steps      = 0;
        bool               isAlone    = false;
        HRESULT            hr         = S_OK;



        ReverseSessionRig::Prepare (machine);

        // Checkpoints a frame apart drift a few cycles past each keyframe
        // boundary, so a keyframe is taken in a capture of its own.
        settings.ring.checkpointCycles = KeyframeSettings::kFrameCycles;

        hr = controller.Start (settings);
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (KeyframeSettings::kFrameCycles * s_kReuseSettle);

        Assert::IsTrue (controller.GetRing().GetCheckpointCount() == controller.GetRing().GetCheckpointLimit(), L"the ring is full, so its pool holds spares");

        keyframes = controller.GetKeyframes().GetCount();

        for (steps = 0; steps < s_kReuseSteps && controller.GetKeyframes().GetCount() == keyframes; steps++)
        {
            before = controller.GetRing().GetByteCount();
            machine.StepOne();
        }

        isAlone = controller.GetRing().GetNextCheckpointCycle() > controller.GetKeyframes().GetInfo (controller.GetKeyframes().GetCount() - 1).cycle;

        Assert::IsTrue  (controller.GetKeyframes().GetCount() > keyframes, L"a keyframe was taken");
        Assert::IsTrue  (isAlone, L"without a checkpoint in the same capture");
        Assert::IsTrue   (controller.GetRing().GetByteCount() >= before, L"the buffer it was saved into went back to the pool, grown if it had to");
    }
private:

    static void WriteSample (StateWriter & writer)
    {
        HRESULT  hr = S_OK;



        writer.BeginSection (s_kReuseTag, 1);
        writer.WriteByte    (0x12);
        writer.WriteBool    (true);
        writer.WriteWord    (0x3456);
        writer.WriteUInt32  (0x789ABCDE);
        writer.WriteUInt64  (0x0102030405060708ULL);

        hr = writer.EndSection();
        AssertSucceeded (hr, L"EndSection");
    }
};
