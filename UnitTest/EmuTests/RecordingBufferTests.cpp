#include "Pch.h"

#include "HResultAssert.h"
#include "EmuTests/ReverseSessionRig.h"
#include "EmuTests/TestMachine.h"
#include "EmuTests/ThreadAllocationCounter.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/ReverseController.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr Word      s_kBufferLoop        = 0x0800;
static constexpr uint64_t  s_kBufferWarmFrames  = 100;
static constexpr uint64_t  s_kBufferCountFrames = 20;
static constexpr uint64_t  s_kBufferAlignFrames = 25;
static constexpr size_t    s_kJournalRounds     = 40;
static constexpr size_t    s_kJournalBurst      = 7;
static constexpr size_t    s_kJournalKeep       = 5;





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingBufferTests
//
//  Recording reuses its buffers: once the ring and the keyframe store have
//  reached their working size, a capture allocates nothing on the thread that
//  runs the machine, keyframes being packed on the work queue; and a
//  keyframe falls on a checkpoint's capture, so one save serves both.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RecordingBufferTests)
{
public:

#ifdef _DEBUG
    TEST_METHOD (ASteadyCaptureAllocatesNothingOnTheMachineThread)
    {
        TestMachine              machine    ("Apple2e");
        ReverseController        controller (machine);
        ReverseSettings          settings;
        ThreadAllocationCounter  counter;
        size_t                   keyframes  = 0;
        uint64_t                 allocated  = 0;
        HRESULT                  hr         = S_OK;



        PrepareLoop (machine);

        // A keyframe every frame and a store that keeps only its newest
        // group, so both the store and the journal reach their working size.
        settings.keyframes.intervalCycles = KeyframeSettings::kFrameCycles;
        settings.keyframes.budgetBytes    = 1;
        settings.ring.budgetBytes         = 0;

        hr = controller.Start (settings);
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (KeyframeSettings::kFrameCycles * s_kBufferWarmFrames);

        keyframes = CountKeyframesTaken (controller);

        counter.Start();
        machine.RunCycles (KeyframeSettings::kFrameCycles * s_kBufferCountFrames);
        allocated = counter.Stop();

        keyframes = CountKeyframesTaken (controller) - keyframes;

        Logger::WriteMessage (std::format ("{} allocations over {} frames, {} keyframes\n", allocated, s_kBufferCountFrames, keyframes).c_str());

        Assert::IsTrue (keyframes >= s_kBufferCountFrames, L"a keyframe was taken every frame");
        Assert::AreEqual<uint64_t> (0, allocated, L"nothing allocates on the machine thread; a keyframe's packed bytes are made on the work queue");
    }
#endif


    TEST_METHOD (EveryKeyframeIsTakenOnACheckpoint)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        HRESULT            hr         = S_OK;
        size_t             i          = 0;
        size_t             index      = 0;
        bool               isHeld     = false;
        KeyframeInfo       info;



        PrepareLoop (machine);

        hr = controller.Start (ReverseSettings());
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (KeyframeSettings::kFrameCycles * s_kBufferAlignFrames);

        Assert::IsTrue (controller.GetKeyframes().GetCount() >= 3, L"keyframes were taken past the first");

        for (i = 0; i < controller.GetKeyframes().GetCount(); i++)
        {
            info   = controller.GetKeyframes().GetInfo (i);
            isHeld = controller.GetRing().TryFindCheckpointAtOrBefore (info.position, index);

            Assert::IsTrue (isHeld, L"the ring reaches back to the keyframe");
            Assert::AreEqual<uint64_t> (info.position, controller.GetRing().GetCheckpoint (index).position, L"the keyframe has a checkpoint at its position");
            Assert::AreEqual<uint64_t> (info.cycle, controller.GetRing().GetCheckpoint (index).cycle, L"and at its cycle");
        }
    }


    TEST_METHOD (AJournalReusingItsSlotsKeepsEveryRecord)
    {
        InputJournal             journal;
        std::deque<std::string>  expected;
        size_t                   round    = 0;
        size_t                   i        = 0;
        size_t                   index    = 0;
        std::string              payload;



        journal.SetOn (true);

        for (round = 0; round < s_kJournalRounds; round++)
        {
            for (i = 0; i < s_kJournalBurst; i++)
            {
                payload = std::format ("a payload longer than a short string, {} {}", round, i);

                journal.Record (round, InputKind::DiskMount, 0, 0, payload);
                expected.push_back (payload);
            }

            if (round % 3 == 2)
            {
                journal.Truncate (journal.GetEndIndex() - 2);
                expected.pop_back();
                expected.pop_back();
            }

            journal.DiscardBefore (journal.GetEndIndex() - std::min (expected.size(), s_kJournalKeep + round % 4));

            while (expected.size() > journal.GetEndIndex() - journal.GetBeginIndex())
            {
                expected.pop_front();
            }

            for (index = journal.GetBeginIndex(); index < journal.GetEndIndex(); index++)
            {
                Assert::AreEqual<std::string> (expected[index - journal.GetBeginIndex()], journal.GetRecord (index).payload, L"each record kept reads back as written");
                Assert::IsFalse (journal.GetRecord (index).isObserved);
            }
        }

        Assert::IsTrue (journal.GetEndIndex() - journal.GetBeginIndex() > 0, L"records were kept to check");
    }

private:

    //  A loop that counts in RAM and polls the keyboard, with no disk. Its 15
    //  cycles do not divide a frame, so a capture lands past its due cycle by
    //  an amount that changes from frame to frame.
    static void PrepareLoop (TestMachine & machine)
    {
        static constexpr Byte  kLoop[] =
        {
            0xEE, 0x00, 0x03,       // 0800  INC $0300
            0xAD, 0x00, 0xC0,       // 0803  LDA $C000
            0xEA,                   // 0806  NOP
            0x4C, 0x00, 0x08,       // 0807  JMP $0800
        };
        size_t  i = 0;



        machine.PowerCycle();

        for (i = 0; i < sizeof (kLoop); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (s_kBufferLoop + i), kLoop[i]);
        }

        machine.GetCpu()->SetPC (s_kBufferLoop);
    }


    //  The newest keyframe's cycle over the interval: how many have been
    //  taken, whatever the store has dropped.
    static size_t CountKeyframesTaken (ReverseController & controller)
    {
        const KeyframeStore  & store = controller.GetKeyframes();



        return static_cast<size_t> (store.GetInfo (store.GetCount() - 1).cycle / store.GetSettings().intervalCycles);
    }
};
