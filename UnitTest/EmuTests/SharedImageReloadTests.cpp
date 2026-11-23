#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/Store/ExternalChangeMonitor.h"
#include "SharedImageRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageReloadTests
//
//  A changed image picked up after the quiet period, the intent stated with
//  a change, and images that cannot be read again.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (SharedImageReloadTests)
{
public:

    static constexpr int           kSlot      = SharedImageRig::kSlot;
    static constexpr int           kDrive     = SharedImageRig::kDrive;
    static constexpr const char *  kDirectory = SharedImageRig::kDirectory;
    static constexpr const char *  kImagePath = SharedImageRig::kImagePath;

    //  The other half. A write that DID say what it was for keeps the sentence
    //  that makes the feature worth having.
    TEST_METHOD (AStatedIntentIsAttributedToTheOneThingThatCanStateIt)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        Assert::AreEqual ((size_t) 0, rig.questions.size(),
                          L"a stated intent is acted on rather than asked about");
        Assert::AreEqual ((size_t) 1, rig.reports.size());
        Assert::IsTrue (rig.reports[0].message.find (L"CassoCli") != std::wstring::npos,
                        L"the intent channel is the only thing that can identify a writer");
    }



    //  AND THE FILE IS NOT RE-READ WHILE THE USER READS THE QUESTION. The
    //  change stays pending by design until an answer arrives, so every idle
    //  tick used to read the whole image and nibblize it before reaching the
    //  point where an outstanding question was noticed.
    TEST_METHOD (AStandingQuestionDoesNotReReadTheImage)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"an unexplained change is a question");

        {
            const int  before = rig.reads;

            rig.nowMs += 5000;
            rig.store.GetChangeMonitor().ApplyPendingReload();
            rig.store.GetChangeMonitor().ApplyPendingReload();
            rig.store.GetChangeMonitor().ApplyPendingReload();

            Assert::AreEqual (before, rig.reads,
                              L"an idle tick under a standing question reads nothing");
        }
    }




    TEST_METHOD (AChangedImageIsPickedUpAndTheIdentityRefreshed)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->GetPending().seen,
                         L"the change was dealt with");
        Assert::IsTrue (rig.store.GetSharedState (kSlot, kDrive)->GetIdentity()
                            .Matches (rig.identities[kImagePath]),
                        L"the identity moved on with the file, so the swap does not "
                        L"immediately look like another external change");
        Assert::AreEqual ((size_t) 1, rig.reports.size(),
                          L"a disk changing under a running program is not something "
                          L"to do silently");
        Assert::AreEqual (0, rig.restarts);
    }



    //  Casso Explorer states its intent as the command line does, and passes a
    //  window for the reply; the notice credits the change to it, not to
    //  CassoCli.
    TEST_METHOD (AChangeStatedByCassoExplorerIsReportedAsItsOwn)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace, true);

        Assert::AreEqual ((size_t) 1, rig.reports.size());
        Assert::IsTrue (rig.reports[0].message.find (L"Casso Explorer modified") != std::wstring::npos, rig.reports[0].message.c_str());
        Assert::IsTrue (rig.reports[0].message.find (L"CassoCli") == std::wstring::npos);
    }



    TEST_METHOD (ThreeChangesInsideTheQuietPeriodProduceOneReload)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        //  A disk carrying more than one thing is built by more than one
        //  command, and the developer means one build.
        rig.WriteImage (kImagePath, 0x22);
        rig.watcher.Fire (kDirectory, kImagePath);

        rig.nowMs += MountedImageState::kQuietPeriodMs / 2;
        rig.store.GetChangeMonitor().ApplyPendingReload();
        Assert::AreEqual ((size_t) 0, rig.reports.size(), L"still being written");

        rig.WriteImage (kImagePath, 0x33);
        rig.watcher.Fire (kDirectory, kImagePath);

        rig.nowMs += MountedImageState::kQuietPeriodMs / 2;
        rig.store.GetChangeMonitor().ApplyPendingReload();
        Assert::AreEqual ((size_t) 0, rig.reports.size(),
                          L"the third change resets the timer -- the period is "
                          L"measured from the LAST change, not the first");

        rig.WriteImage (kImagePath, 0x44);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        Assert::AreEqual ((size_t) 1, rig.reports.size(), L"one build, one pick-up");
    }



    TEST_METHOD (AStatedRestartRestartsTheMachineWithoutAsking)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.store.GetChangeMonitor().NoteExternalChange (kImagePath, ExternalChangeIntent::Restart);

        rig.nowMs += MountedImageState::kQuietPeriodMs;
        rig.store.GetChangeMonitor().ApplyPendingReload();

        Assert::AreEqual (1, rig.restarts);
        Assert::AreEqual ((size_t) 0, rig.questions.size(),
                          L"the writer knew what they changed");
    }



    TEST_METHOD (ALaterStatedIntentReplacesAnEarlierOne)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetChangeMonitor().NoteExternalChange (kImagePath, ExternalChangeIntent::Restart);
        rig.WriteImage (kImagePath, 0x22);
        rig.store.GetChangeMonitor().NoteExternalChange (kImagePath, ExternalChangeIntent::ReloadInPlace);

        rig.nowMs += MountedImageState::kQuietPeriodMs;
        rig.store.GetChangeMonitor().ApplyPendingReload();

        //  The last writer is the one whose bytes are on the disk, so a stale
        //  intent describes contents that are gone.
        Assert::AreEqual (0, rig.restarts);
        Assert::AreEqual ((size_t) 1, rig.reports.size());
    }



    TEST_METHOD (WithNothingStatedAndNoDeclaredAnswerTheUserIsAskedOnce)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size());

        //  Asking cannot block -- the answer arrives from the thread that owns
        //  the screen -- so the change is still pending on the next idle tick.
        //  Without the outstanding flag the user would be asked again sixty
        //  times a second while reading the first one.
        rig.nowMs += MountedImageState::kQuietPeriodMs;
        rig.store.GetChangeMonitor().ApplyPendingReload();
        rig.store.GetChangeMonitor().ApplyPendingReload();

        Assert::AreEqual ((size_t) 1, rig.questions.size());
    }



    TEST_METHOD (BytesThatCannotBeThisDiskAreRefusedAndTheHeldDiskSurvives)
    {
        SharedImageRig  rig;
        Byte            before = 0;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        before = SharedImageRig::FirstTrackByte (rig.store);

        //  Replaced by something the loader cannot use as this disk.
        rig.files[kImagePath] = vector<Byte> (37, 0xAB);
        rig.Stamp (kImagePath);

        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((int) before, (int) SharedImageRig::FirstTrackByte (rig.store),
                          L"the machine is running and what it holds is known-good");
        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"what Casso holds may be the only copy left, so the user "
                          L"is offered a copy rather than told nothing");
    }



    TEST_METHOD (AnImageThatHasGoneIsRefusedRatherThanTreatedAsUnchanged)
    {
        SharedImageRig  rig;
        Byte            before = 0;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        before = SharedImageRig::FirstTrackByte (rig.store);

        rig.files.erase (kImagePath);
        rig.identities.erase (kImagePath);

        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((int) before, (int) SharedImageRig::FirstTrackByte (rig.store));
        Assert::AreEqual ((size_t) 1, rig.questions.size());
    }



    TEST_METHOD (AChangeArrivingWhileAnEarlierOneIsAppliedIsNotDropped)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        //  The apply runs while a report is being drawn; a further change
        //  updates the pending record rather than being dropped because
        //  something was in progress.
        rig.store.SetChangeReportSink ([&rig] (int, int, const ChangePrompt & prompt)
        {
            rig.reports.push_back (prompt);

            if (rig.reports.size() == 1)
            {
                rig.WriteImage (kImagePath, 0x44);
                rig.store.GetChangeMonitor().NoteExternalChange (kImagePath, ExternalChangeIntent::ReloadInPlace);
            }
        });

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        Assert::IsTrue (rig.store.GetSharedState (kSlot, kDrive)->GetPending().seen,
                        L"the change that arrived mid-apply is still there");

        rig.nowMs += MountedImageState::kQuietPeriodMs;
        rig.store.GetChangeMonitor().ApplyPendingReload();

        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->GetPending().seen);
    }



    TEST_METHOD (AStandingReportAbsorbsFurtherChangesRatherThanStacking)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        rig.WriteImage (kImagePath, 0x33);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        rig.WriteImage (kImagePath, 0x44);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        //  EVERY ONE OF THEM IS REPORTED, so the wording can follow what
        //  actually happened. Emitting only the first kept one notice at the
        //  cost of it going stale -- measured, a reload followed by a restart
        //  left it advising a reboot that had already been done.
        Assert::AreEqual ((size_t) 3, rig.reports.size());

        //  And the contents reloaded are the most recent, not those current
        //  when the report first appeared.
        Assert::IsTrue (rig.store.GetSharedState (kSlot, kDrive)->GetIdentity()
                            .Matches (rig.identities[kImagePath]));
    }



    TEST_METHOD (AStandingReportFollowsWhatActuallyHappened)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  Reloaded with the machine left running.
        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        Assert::AreEqual ((size_t) 1, rig.reports.size());
        Assert::IsTrue (rig.reports[0].message.find (L"rebooted") == std::wstring::npos,
                        L"nothing was rebooted, so the notice must not claim otherwise");

        //  Then one that reboots. The notice standing from the first change is
        //  re-worded in place rather than left saying what stopped being true.
        rig.WriteImage (kImagePath, 0x33);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::Restart);

        Assert::AreEqual (1, rig.restarts);
        Assert::AreEqual ((size_t) 2, rig.reports.size());
        Assert::IsTrue (rig.reports[1].message.find (L"rebooted") != std::wstring::npos,
                        L"the second change did reboot, and the notice has to follow");
    }
};
