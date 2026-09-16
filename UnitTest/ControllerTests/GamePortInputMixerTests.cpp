#include "Pch.h"

#include "Controllers/GamePortInputMixer.h"
#include "RecordingGamePortSink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GamePortInputMixerTests
//
//  The mixer is the only writer of the paddles and pushbuttons, so two rules
//  carry everything: a button reads pressed while ANY source holds it, and the
//  axes come from exactly one owner. Before the mixer, each source wrote the
//  machine directly and the last writer won, so a mouse-button release could
//  release a button the keyboard still held.
//
//  A write the machine refuses must not be lost. The refusal happens while a
//  machine rebuild holds the devices, and a lost release is a button stuck
//  down for as long as nothing else changes.
//
//  Writes happen on one thread. A submission from another thread (the
//  controller thread) records its values and asks for a flush, once, instead
//  of writing.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (GamePortInputMixerTests)
    {
    public:

        static GamePortContribution MakeButtons (bool pb0, bool pb1, bool pb2)
        {
            GamePortContribution  contribution;

            contribution.buttons.set (0, pb0);
            contribution.buttons.set (1, pb1);
            contribution.buttons.set (2, pb2);
            return contribution;
        }


        static GamePortContribution MakePaddle (Byte x, Byte y)
        {
            GamePortContribution  contribution;

            contribution.paddle[0] = x;
            contribution.paddle[1] = y;
            return contribution;
        }


        static GamePortContribution MakeAxis (size_t axis, Byte value)
        {
            GamePortContribution  contribution;

            contribution.paddle[axis] = value;
            return contribution;
        }


        static std::thread::id MakeOtherThreadId()
        {
            std::thread      other ([] {});
            std::thread::id  id = other.get_id();

            other.join();
            return id;
        }


        TEST_METHOD (Buttons_ReleasingOneSourceKeepsAnotherSourcesPress)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;

            mixer.SetSink (&sink);
            mixer.Submit (GamePortSource::FireKeys,   MakeButtons (true, false, false));
            mixer.Submit (GamePortSource::Controller, MakeButtons (true, false, false));
            mixer.ReleaseSource (GamePortSource::Controller);

            Assert::IsTrue (mixer.GetTargetState().buttons.test (0),
                L"PB0 must stay pressed while the fire keys still hold it");
            Assert::IsTrue (sink.writes.back().state.buttons.test (0),
                L"the machine must still read PB0 pressed");

            mixer.ReleaseSource (GamePortSource::FireKeys);

            Assert::IsFalse (sink.writes.back().state.buttons.test (0),
                L"PB0 must release once no source holds it");
        }


        TEST_METHOD (Buttons_Pb2OrsLikeTheOthers)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;

            // The source that lets go is the controller, the last one the
            // mixer reads, so a mixer that let the last source overwrite the
            // rest would read PB2 released here.
            mixer.SetSink (&sink);
            mixer.Submit (GamePortSource::AppleModifierKeys, MakeButtons (false, false, true));
            mixer.Submit (GamePortSource::Controller,        MakeButtons (false, false, true));
            mixer.ReleaseSource (GamePortSource::Controller);

            Assert::IsTrue (sink.writes.back().state.buttons.test (2), L"PB2 must stay pressed while the Shift key holds it");
        }


        TEST_METHOD (Axes_ComeFromTheOwnerOnly)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::ArrowKeys);
            mixer.Submit (GamePortSource::ArrowKeys,  MakePaddle (0, 255));
            mixer.Submit (GamePortSource::Controller, MakePaddle (200, 10));

            Assert::AreEqual (static_cast<Byte> (0),   sink.writes.back().state.paddle[0], L"X must follow the arrow keys");
            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[1], L"Y must follow the arrow keys");
        }


        TEST_METHOD (Axes_OwnerSwitchUsesTheNewOwnersLastValuesImmediately)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            mixer.Submit (GamePortSource::Controller, MakePaddle (200, 10));
            mixer.Submit (GamePortSource::ArrowKeys,  MakePaddle (0, 255));
            mixer.SetAxisOwner (AxisOwner::ArrowKeys);

            Assert::AreEqual (static_cast<Byte> (0),   sink.writes.back().state.paddle[0], L"X must switch to the arrows' held value");
            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[1], L"Y must switch to the arrows' held value");
        }


        TEST_METHOD (Axes_NoOwnerRestsAtCenter)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;

            mixer.SetSink (&sink);
            mixer.Submit (GamePortSource::Controller, MakePaddle (200, 10));

            Assert::AreEqual (GamePortState::kPaddleCenter, sink.writes.back().state.paddle[0], L"X must rest at center with no owner");
            Assert::AreEqual (GamePortState::kPaddleCenter, sink.writes.back().state.paddle[1], L"Y must rest at center with no owner");
        }


        TEST_METHOD (Axes_EachAxisHasItsOwnOwner)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (0, AxisOwner::Controller);
            mixer.SetAxisOwner (1, AxisOwner::ArrowKeys);
            mixer.Submit (GamePortSource::Controller, MakePaddle (200, 10));
            mixer.Submit (GamePortSource::ArrowKeys,  MakePaddle (0, 255));

            Assert::AreEqual (static_cast<Byte> (200), sink.writes.back().state.paddle[0], L"PDL0 follows its owner, the controller");
            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[1], L"PDL1 follows its owner, the arrow keys");
            Assert::AreEqual (GamePortState::kPaddleCenter, sink.writes.back().state.paddle[2], L"an axis nobody owns rests at center");
        }


        TEST_METHOD (Axes_TwoControllersHoldPdl0AndPdl1Independently)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;
            GamePortContribution   both;

            // Two controllers reach the mixer as one source whose axes are held
            // separately: the first player's on PDL0, the second's on PDL1.
            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            mixer.Submit (GamePortSource::Controller, MakeAxis (0, 30));

            Assert::AreEqual (static_cast<Byte> (30), sink.writes.back().state.paddle[0], L"the first controller drives PDL0");
            Assert::AreEqual (GamePortState::kPaddleCenter, sink.writes.back().state.paddle[1], L"and leaves PDL1 alone, since it does not hold it");

            both            = MakeAxis (0, 30);
            both.paddle[1]  = 220;
            mixer.Submit (GamePortSource::Controller, both);

            Assert::AreEqual (static_cast<Byte> (30),  sink.writes.back().state.paddle[0], L"PDL0 keeps the first controller's value");
            Assert::AreEqual (static_cast<Byte> (220), sink.writes.back().state.paddle[1], L"while PDL1 takes the second's");

            mixer.Submit (GamePortSource::Controller, MakeAxis (1, 220));

            Assert::AreEqual (GamePortState::kPaddleCenter, sink.writes.back().state.paddle[0], L"the first letting go centers only its own axis");
            Assert::AreEqual (static_cast<Byte> (220), sink.writes.back().state.paddle[1], L"and the second's reading is untouched");
        }


        TEST_METHOD (Axes_AssigningAnAxisDisplacesOnlyThatAxissOwner)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            mixer.Submit (GamePortSource::Controller,  MakePaddle (200, 10));
            mixer.Submit (GamePortSource::MousePaddle, MakePaddle (50, 60));
            mixer.SetAxisOwner (1, AxisOwner::MousePaddle);

            Assert::AreEqual (static_cast<Byte> (60),  sink.writes.back().state.paddle[1], L"the new owner takes PDL1 at once");
            Assert::AreEqual (static_cast<Byte> (200), sink.writes.back().state.paddle[0], L"and the displaced owner keeps PDL0");
        }


        TEST_METHOD (Buttons_OrAcrossTwoControllersOnSeparateAxes)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;
            GamePortContribution   controllers = MakeAxis (0, 30);
            GamePortContribution   mouse       = MakeAxis (1, 220);

            controllers.buttons.set (0);
            mouse.buttons.set (1);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (0, AxisOwner::Controller);
            mixer.SetAxisOwner (1, AxisOwner::MousePaddle);
            mixer.Submit (GamePortSource::Controller,  controllers);
            mixer.Submit (GamePortSource::MousePaddle, mouse);

            Assert::IsTrue (sink.writes.back().state.buttons.test (0), L"each axis owner's button reaches its own line");
            Assert::IsTrue (sink.writes.back().state.buttons.test (1), L"with both held at once");
        }


        TEST_METHOD (Writes_UnchangedSubmissionWritesNothing)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;
            size_t                 before = 0;

            mixer.SetSink (&sink);
            mixer.Submit (GamePortSource::Controller, MakeButtons (true, false, false));
            before = sink.writes.size();

            mixer.Submit (GamePortSource::Controller, MakeButtons (true, false, false));
            mixer.Submit (GamePortSource::FireKeys,   MakeButtons (true, false, false));

            Assert::AreEqual (before, sink.writes.size(),
                L"a submission that leaves the final state unchanged must not write");
        }


        TEST_METHOD (Writes_FirstWriteAfterSinkChangeCoversEveryField)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  first;
            RecordingGamePortSink  second;

            mixer.SetSink (&first);
            mixer.Submit (GamePortSource::Controller, MakeButtons (true, false, false));
            mixer.SetSink (&second);

            Assert::AreEqual (static_cast<size_t> (2), first.writes.size(), L"the first sink must see its attach write and the submission");
            Assert::IsFalse  (first.writes[1].wroteEveryField,              L"a write to an up-to-date sink is incremental");
            Assert::AreEqual (static_cast<size_t> (1), second.writes.size(), L"attaching a sink must write the current state");
            Assert::IsTrue   (second.writes[0].wroteEveryField,             L"a new sink's first write must cover every field");
        }


        TEST_METHOD (Rebuild_RewritesEveryField)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;

            mixer.SetSink (&sink);
            mixer.Submit (GamePortSource::Controller, MakeButtons (true, false, false));
            mixer.NotifyMachineRebuilt();

            Assert::IsTrue (sink.writes.back().wroteEveryField, L"a rebuilt machine must receive every field");
            Assert::IsTrue (sink.writes.back().state.buttons.test (0), L"and the held button with them");
        }


        TEST_METHOD (Refused_ReleaseIsDeliveredByFlush)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;
            bool                   upToDate = false;

            mixer.SetSink (&sink);
            mixer.Submit (GamePortSource::Controller, MakeButtons (true, false, false));

            sink.refuseNext = 1;
            mixer.ReleaseSource (GamePortSource::Controller);

            Assert::AreEqual (1, sink.refusedCount,        L"the release write must have been refused");
            Assert::IsTrue   (mixer.HasPendingWrite(),     L"a refused write must leave a pending write");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0), L"the machine still holds the press");

            upToDate = mixer.FlushPending();

            Assert::IsTrue  (upToDate,                                   L"the flush must report the machine up to date");
            Assert::IsFalse (mixer.HasPendingWrite(),                    L"nothing may remain pending");
            Assert::IsFalse (sink.writes.back().state.buttons.test (0),  L"the release must reach the machine");
        }


        TEST_METHOD (Refused_ReleaseIsDeliveredByTheNextSubmission)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;

            mixer.SetSink (&sink);
            mixer.Submit (GamePortSource::Controller, MakeButtons (true, false, false));

            sink.refuseNext = 1;
            mixer.ReleaseSource (GamePortSource::Controller);
            mixer.Submit (GamePortSource::FireKeys, MakeButtons (false, true, false));

            Assert::IsFalse (sink.writes.back().state.buttons.test (0), L"the refused release must ride along with the next write");
            Assert::IsTrue  (sink.writes.back().state.buttons.test (1), L"the new submission must be written too");
        }


        TEST_METHOD (Refused_ReleaseIsDeliveredByRebuild)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;

            mixer.SetSink (&sink);
            mixer.Submit (GamePortSource::Controller, MakeButtons (true, false, false));

            sink.refuseNext = 1;
            mixer.ReleaseSource (GamePortSource::Controller);
            mixer.NotifyMachineRebuilt();

            Assert::IsFalse (mixer.HasPendingWrite(),                   L"the rebuild notification must flush the refused write");
            Assert::IsFalse (sink.writes.back().state.buttons.test (0), L"the release must reach the rebuilt machine");
        }


        TEST_METHOD (NoSink_KeepsTheWritePending)
        {
            GamePortInputMixer  mixer;
            bool                upToDate = true;

            mixer.Submit (GamePortSource::Controller, MakeButtons (true, false, false));
            upToDate = mixer.FlushPending();

            Assert::IsFalse (upToDate,                L"with no machine attached nothing can be up to date");
            Assert::IsTrue  (mixer.HasPendingWrite(), L"the state must wait for a sink");
        }


        TEST_METHOD (OtherThread_RequestsOneFlushInsteadOfWriting)
        {
            GamePortInputMixer     mixer;
            RecordingGamePortSink  sink;
            int                    requests = 0;

            mixer.SetSink (&sink);
            mixer.FlushPending();
            mixer.SetApplyThread (MakeOtherThreadId(), [&requests] { requests++; });

            mixer.Submit (GamePortSource::Controller, MakeButtons (true, false, false));
            mixer.Submit (GamePortSource::Controller, MakeButtons (true, true,  false));

            Assert::AreEqual (static_cast<size_t> (1), sink.writes.size(), L"a submission off the apply thread must not write");
            Assert::AreEqual (1, requests,                                   L"two submissions before a flush must request one flush");

            mixer.FlushPending();

            Assert::IsTrue (sink.writes.back().state.buttons.test (1), L"the flush must write the latest values");

            mixer.Submit (GamePortSource::Controller, MakeButtons (false, false, false));

            Assert::AreEqual (2, requests, L"a submission after the flush must request a new one");
        }
    };
}
