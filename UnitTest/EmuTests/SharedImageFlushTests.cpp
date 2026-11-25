#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Devices/Disk/Store/ExternalChangeMonitor.h"
#include "Devices/Disk/Store/ExternalChangeActions.h"
#include "Devices/Disk/Store/DiskFlusher.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "SharedImageRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageFlushTests
//
//  Flushes, ejects and shutdown over an image changed outside: the copy
//  written beside it, the rescue prompt, and conflicts.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (SharedImageFlushTests)
{
public:

    static constexpr int           kSlot      = SharedImageRig::kSlot;
    static constexpr int           kDrive     = SharedImageRig::kDrive;
    static constexpr const char *  kDirectory = SharedImageRig::kDirectory;
    static constexpr const char *  kImagePath = SharedImageRig::kImagePath;

    //  AND A RESERVATION DOES NOT OUTLIVE THE DISK IT WAS MADE FOR. Neither
    //  eject clears one, which costs nothing while the bay stands empty --
    //  nothing reads a reservation on an empty bay. The disk mounted next is
    //  what inherited it, and filed its own copy under the name and the moment
    //  belonging to a disk that had already left the drive.
    TEST_METHOD (AReservationDoesNotOutliveTheDiskItWasMadeFor)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  A question goes up, reserving a name stamped with this moment, and
        //  the user takes the disk out instead of answering it.
        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"an unexplained change is a question");

        rig.store.Eject (kSlot, kDrive);

        //  An hour later a disk goes back in, and it conflicts on its own.
        rig.wallClock += 3600;

        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        AssertSucceeded (rig.store.GetFlusher().Flush (kSlot, kDrive));

        Assert::AreEqual ((size_t) 1, rig.PreservedPaths().size());

        Assert::AreNotEqual (PreservedCopy::MakePath (
                                 kImagePath,
                                 PreservedCopy::MakeStamp (rig.wallClock - 3600), 0),
                             rig.PreservedPaths()[0],
                             L"not under the name reserved for the disk that left");
        Assert::AreEqual (PreservedCopy::MakePath (
                              kImagePath,
                              PreservedCopy::MakeStamp (rig.wallClock), 0),
                          rig.PreservedPaths()[0],
                          L"but under one belonging to the disk that is in the drive");
    }



    TEST_METHOD (TheStoreDoesNotReportItsOwnWriteAsAnExternalChange)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        //  A guest write, flushed the way the motor-off hook flushes it. The
        //  flush changes the file, and a directory watcher reports that.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertSucceeded (rig.store.GetFlusher().Flush (kSlot, kDrive));

        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 0, rig.reports.size(),
                          L"the emulator's own commit is not somebody else's change");
        Assert::AreEqual (0, rig.restarts);
    }



    TEST_METHOD (ASecondFlushIsNotRefusedByTheFirstFlushesOwnChange)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertSucceeded (rig.store.GetFlusher().Flush (kSlot, kDrive));

        //  Without the post-commit refresh this is where every ordinary
        //  session breaks: the first flush changes the file, nothing updates
        //  what was recorded, and the second flush refuses itself.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[1] = 0x7E;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertSucceeded (rig.store.GetFlusher().Flush (kSlot, kDrive));
    }



    //  THE OUTCOME MUST NOT DEPEND ON WHICH END FOUND THE COLLISION. This is
    //  the flush half; the watcher half is below. Both leave the original
    //  holding the other program's version and the guest's version in a file
    //  of its own, so the same collision produces the same two files either
    //  way. It did not use to: whichever side was quicker decided which
    //  version kept the original name.
    TEST_METHOD (AFlushOverAnExternalChangeLeavesTheFileAloneAndMovesTheGuestsVersion)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  The guest writes, and something else rewrites the file before the
        //  flush lands -- the same conflict the watcher finds, discovered at
        //  the other end because notification never arrived.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        AssertSucceeded (rig.store.GetFlusher().Flush (kSlot, kDrive));

        Assert::AreEqual ((int) 0x33, (int) rig.files[kImagePath][0],
                          L"the original still holds what the other program wrote");

        Assert::AreEqual ((size_t) 1, rig.PreservedPaths().size());
        Assert::AreNotEqual ((int) 0x33,
                             (int) rig.files[rig.PreservedPaths()[0]][0],
                             L"and the guest's version went to a file of its own");

        Assert::IsFalse (rig.store.GetImage (kSlot, kDrive)->IsDirty(),
                         L"which leaves nothing unsaved in the bay");

        Assert::AreEqual (rig.PreservedPaths()[0], rig.store.GetSourcePath (kSlot, kDrive),
                          L"and the bay now reads and writes that file");
    }



    TEST_METHOD (AFlushIsRefusedEntirelyWhenTheDisplacedVersionCannotBeKept)
    {
        SharedImageRig  rig;
        HRESULT         hrFlush = S_OK;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        rig.refusePreserve = true;

        hrFlush = rig.store.GetFlusher().Flush (kSlot, kDrive);

        //  The whole promise is that a version is never destroyed. A preserve
        //  that did not happen therefore stops the write that would have done
        //  the destroying.
        Assert::IsTrue (FAILED (hrFlush));
        Assert::AreEqual ((int) 0x33, (int) rig.files[kImagePath][0],
                          L"the file still holds the external version");
        Assert::IsTrue (rig.store.GetImage (kSlot, kDrive)->IsDirty(),
                        L"and the guest's writes are still in memory");
    }



    //  AND THE REFUSAL IS PUT AS A QUESTION, the same one the watcher side
    //  puts. Which end notices the collision first is timing the user cannot
    //  see, and it used to decide whether they were offered somewhere else to
    //  put the disk or merely told the write had not happened.
    TEST_METHOD (AFlushThatCannotKeepTheDisplacedVersionAsksWhereToPutIt)
    {
        SharedImageRig  rig;
        HRESULT         hrFlush = S_OK;
        std::string  chosen  = "C:\\elsewhere\\Rescued.dsk";



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        rig.refusePreserve = true;

        hrFlush = rig.store.GetFlusher().Flush (kSlot, kDrive);

        Assert::IsTrue (FAILED (hrFlush));
        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"asked, not merely announced");
        Assert::AreEqual ((size_t) 0, rig.reports.size(),
                          L"and not put where no answer can be given");

        //  "Save as..." and nothing else destructive.
        Assert::AreEqual ((size_t) 2, rig.questions[0].answers.size());
        Assert::IsTrue (rig.questions[0].answers[0].action == ChangeAction::PreserveCopy);
        Assert::IsTrue (rig.questions[0].answers[rig.questions[0].safeAnswer].action
                            == ChangeAction::Ignore);

        //  And answering it puts the disk where the user chose, with the drive
        //  still running on it.
        rig.refusePreserve = false;

        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::PreserveCopy, chosen);

        Assert::AreEqual (chosen, rig.store.GetSourcePath (kSlot, kDrive),
                          L"the drive reads and writes the rescued file now");
        Assert::AreEqual ((int) 0x33, (int) rig.files[kImagePath][0],
                          L"and the original still holds the external version");
    }



    //  AND AN EJECT THAT CANNOT KEEP IT WAITS TO BE TOLD WHAT TO DO. Emptying
    //  the bay is what destroys a disk whose copy has nowhere to go, so the one
    //  moment the user must be asked is the moment it was about to happen. Both
    //  answers end with the drive empty, so nothing is stuck in it.
    TEST_METHOD (AnEjectThatCannotKeepTheDisplacedVersionAsksBeforeEmptyingTheDrive)
    {
        SharedImageRig          rig;
        std::string  chosen = "C:\\elsewhere\\Rescued.dsk";



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        rig.refusePreserve = true;

        rig.store.Eject (kSlot, kDrive);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"asked, because the drive is about to be emptied");
        Assert::IsTrue (rig.store.IsMounted (kSlot, kDrive),
                        L"and the disk is still there while the question stands");

        //  Neither answer is a dismissal: the drive empties either way, and
        //  the safe one is the one that keeps the disk.
        Assert::AreEqual ((size_t) 2, rig.questions[0].answers.size());
        Assert::IsTrue (rig.questions[0].answers[0].action == ChangeAction::PreserveCopy);
        Assert::IsTrue (rig.questions[0].answers[1].action == ChangeAction::Discard);
        Assert::AreEqual ((size_t) 0, rig.questions[0].safeAnswer);

        rig.refusePreserve = false;

        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::PreserveCopy, chosen);

        Assert::IsFalse (rig.store.IsMounted (kSlot, kDrive),
                         L"the eject finishes once it has an answer");
        Assert::IsTrue (rig.files.count (chosen) != 0,
                        L"and the disk went where the user said");
    }



    //  AND DISCARDING FINISHES THE EJECT TOO. Left dirty, the eject would find
    //  the same collision and put the same question up again, and a disk the
    //  user had just thrown away would be one they could not throw away.
    TEST_METHOD (DiscardingAtAnEjectEmptiesTheDriveRatherThanAskingAgain)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        rig.refusePreserve = true;

        rig.store.Eject (kSlot, kDrive);

        Assert::AreEqual ((size_t) 1, rig.questions.size());

        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::Discard, "");

        Assert::IsFalse (rig.store.IsMounted (kSlot, kDrive),
                         L"the drive empties");
        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"and is not asked about a second time");
        Assert::AreEqual ((int) 0x33, (int) rig.files[kImagePath][0],
                          L"the file keeps the other program's version");
    }



    //  AND A DISK NOBODY CAN BE ASKED ABOUT STILL COMES OUT. Without a sink
    //  there is no question to wait for, and a bay that waited anyway would be
    //  one the user could never empty.
    TEST_METHOD (AnEjectWithNothingToAskStillEmptiesTheDrive)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        rig.refusePreserve  = true;
        rig.askSinkDelivers = false;

        rig.store.Eject (kSlot, kDrive);

        Assert::IsFalse (rig.store.IsMounted (kSlot, kDrive),
                         L"a question that could not be put does not hold the disk in");
    }



    //  THE LAST FLUSH OF THE PROCESS ASKS THROUGH A DIALOG THAT BLOCKS. There
    //  is no pump left to deliver a posted question and no thread to act on the
    //  answer, but the apartment is still up -- so the asking and the writing
    //  both happen inside the call that found the problem.
    TEST_METHOD (ShutdownAsksWhereToPutADiskItCannotOtherwiseSave)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        rig.refusePreserve = true;
        rig.rescueChoice   = "C:\\elsewhere\\OnTheWayOut.dsk";

        //  The flush itself still reports failure, and truthfully: the file it
        //  was asked to write keeps the other program's version. What changed
        //  is where the guest's disk went.
        HRESULT  hrFlush = rig.store.GetFlusher().FlushAllForShutdown();

        Assert::IsTrue (FAILED (hrFlush));

        Assert::AreEqual (1, rig.rescuesAsked,
                          L"asked once, on the way out");
        Assert::AreEqual ((size_t) 0, rig.questions.size(),
                          L"and not through the route that needs an answer later");
        Assert::IsTrue (rig.files.count (rig.rescueChoice) != 0,
                        L"the disk is on disk where the user said");
        Assert::IsFalse (rig.store.GetImage (kSlot, kDrive)->IsDirty(),
                         L"and nothing is left waiting to be written");
    }



    //  AND DECLINING IS ALLOWED. What must not happen is being told the writes
    //  are safe in memory that is about to be released.
    TEST_METHOD (ADeclinedShutdownRescueLeavesNothingBehindToWrite)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        rig.refusePreserve = true;

        //  rescueChoice left empty: the user closed the picker.
        rig.store.GetFlusher().FlushAllForShutdown();

        Assert::AreEqual (1, rig.rescuesAsked);
        Assert::AreEqual ((size_t) 0, rig.PreservedPaths().size(),
                          L"nothing was written anywhere");
        Assert::AreEqual ((int) 0x33, (int) rig.files[kImagePath][0],
                          L"and the file keeps the other program's version");
    }



    //  THE HOLE THIS CLOSES. Keeping used to leave the chosen version in
    //  memory and nothing else: the recorded identity was left stale so that a
    //  LATER flush would preserve the file's version first. But a disk the
    //  guest had not written to was not dirty, and a flush of a clean image is
    //  a no-op, so ejecting or quitting threw away the very version the user
    //  had just chosen to keep, silently. Writing it at the moment of the
    //  choice is the only form of "kept" that survives the session.
    //  THE RACE A REAL WALKTHROUGH FOUND. The external change is noticed
    //  BEFORE the guest write makes the image dirty, so the ordinary question
    //  goes up first. The guest then writes, the flush finds the same
    //  collision from the other end, and settles it while that question is
    //  still on screen.
    //
    //  Both ends used to pick the copy's name independently, so the dialog
    //  offered a file that was never created -- measured five seconds apart --
    //  and the flush moved the bay, leaving the user answering about a file it
    //  no longer had.
    TEST_METHOD (AFlushDuringAnOpenQuestionUsesTheNameTheQuestionShowed)
    {
        SharedImageRig  rig;
        std::wstring    offered;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  Clean when the change lands, so this is a question and not a
        //  conflict.
        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(), L"a question, not a conflict");

        offered = rig.questions[0].message;

        Assert::IsTrue (offered.find (L".dsk") != std::wstring::npos,
                        L"the question tells the user where their disk would go");

        //  NOW the guest writes, and the flush finds the collision from the
        //  other end while that question is still up.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);

        AssertSucceeded (rig.store.GetFlusher().Flush (kSlot, kDrive));

        Assert::AreEqual ((size_t) 1, rig.PreservedPaths().size(),
                          L"the guest's version is written -- that part is not optional");

        Assert::IsTrue (offered.find (ChangePrompt::GetFileName (rig.PreservedPaths()[0]))
                            != std::wstring::npos,
                        L"and under the name the open question is already showing");

        //  THE ANSWER STILL OWNS THE OUTCOME. Moving the bay here is what left
        //  the user answering about a file it no longer had.
        Assert::AreEqual (std::string (kImagePath), rig.store.GetSourcePath (kSlot, kDrive),
                          L"the bay has not moved while the question stands");

        Assert::AreEqual ((size_t) 0, rig.reports.size(),
                          L"and nothing was reported over the top of the question");

        //  Answering it now does what it said, and does not make a second copy
        //  of the same disk.
        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::KeepHeld);

        Assert::AreEqual ((size_t) 1, rig.PreservedPaths().size(),
                          L"still one copy, not two");
        Assert::IsTrue (offered.find (ChangePrompt::GetFileName (
                            rig.store.GetSourcePath (kSlot, kDrive))) != std::wstring::npos,
                        L"and the bay is on the file the user was promised");
    }



    TEST_METHOD (ADirtyImageMeetingAnExternalChangeKeepsBothVersions)
    {
        SharedImageRig  rig;
        Byte            guestWrote = 0x7F;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = guestWrote;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        //  The external version is mounted, because it is the newest thing and
        //  the developer just made it.
        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->GetPending().seen);
        Assert::IsFalse (rig.store.GetImage (kSlot, kDrive)->IsDirty(),
                         L"the guest's writes are on disk under their own name now");

        //  And the guest's version is on disk under a name of its own, which is
        //  what makes this a report rather than a question.
        Assert::AreEqual ((size_t) 1, rig.PreservedPaths().size());

        //  IT HOLDS WHAT THE GUEST HAD, not what replaced it. Preserving after
        //  the swap rather than before would put the external version in both
        //  files and lose the guest's entirely, while every other assertion
        //  here still passed.
        Assert::AreEqual ((int) 0x11, (int) rig.files[rig.PreservedPaths()[0]][0],
                          L"the preserved copy is the version that was displaced");
        Assert::AreEqual ((int) 0x22, (int) rig.files[kImagePath][0],
                          L"and the file still holds the external one");
        Assert::AreEqual ((size_t) 1, rig.reports.size());
        Assert::IsTrue (rig.reports[0].message.find (
                            fs::path (rig.PreservedPaths()[0]).filename().wstring())
                        != std::wstring::npos,
                        L"and the user is told which file it went to");
    }



    TEST_METHOD (AConflictThatCannotBePreservedChangesNothingAtAll)
    {
        SharedImageRig  rig;
        Byte            before = 0;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        before = SharedImageRig::FirstTrackByte (rig.store);

        rig.refusePreserve = true;

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        //  This is the decision most worth asserting: a preserve that silently
        //  did not happen would break the promise exactly where it matters.
        Assert::AreEqual ((int) before, (int) SharedImageRig::FirstTrackByte (rig.store),
                          L"nothing was mounted over the guest's work");
        Assert::IsTrue (rig.store.GetImage (kSlot, kDrive)->IsDirty());
        //  A QUESTION, NOT A NOTICE, because it offers somewhere else to put
        //  the file and a notice has nowhere to put an answer.
        Assert::AreEqual ((size_t) 1, rig.questions.size(), L"the user is told why");
        Assert::IsTrue (rig.questions[0].answers.size() == 2,
                        L"and is offered a way out rather than homework");

        //  AND IT DOES NOT SPIN. The change is dropped rather than left
        //  pending: leaving it meant a full image read and another failed
        //  write on every idle tick, sixty times a second for as long as the
        //  folder stayed full. The next change to the file, or the next flush,
        //  is what tries again.
        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->GetPending().seen,
                         L"the change is not left pending");

        {
            size_t  i = 0;

            for (i = 0; i < 100; i++)
            {
                rig.nowMs += MountedImageState::kQuietPeriodMs;
                rig.store.GetChangeMonitor().ApplyPendingReload();
            }
        }

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"and a hundred idle ticks later it has still done nothing more");
    }



    TEST_METHOD (TwoConflictsInOneSecondProduceTwoDistinctCopies)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        for (int round = 0; round < 2; round++)
        {
            rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] =
                (Byte) (0x70 + round);
            rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);

            rig.WriteImage (kImagePath, (Byte) (0x20 + round));
            rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);
        }

        //  A one-second stamp cannot keep the accumulate-rather-than-overwrite
        //  promise on its own, and a build loop makes exactly this case.
        Assert::AreEqual ((size_t) 2, rig.PreservedPaths().size());
        Assert::IsTrue (rig.PreservedPaths()[0] != rig.PreservedPaths()[1],
                        L"the second copy must not be written over the first");

        //  Both are still there, which is the whole promise: repeated
        //  conflicts accumulate.
        Assert::IsTrue (rig.files.count (rig.PreservedPaths()[0]) == 1);
        Assert::IsTrue (rig.files.count (rig.PreservedPaths()[1]) == 1);

        //  And they sort in the order they happened, which is what the
        //  zero-padded counter is for.
        Assert::IsTrue (rig.PreservedPaths()[0] < rig.PreservedPaths()[1],
                        L"the order things happened must read off the directory");
    }



    TEST_METHOD (EjectingWithWritesTheFileHasNotSeenPreservesThem)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  The guest writes, something else rewrites the file, and the user
        //  ejects rather than answering anything. An eject is a plausible
        //  response to being told something, and it must not become the one
        //  path that loses work.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        rig.store.Eject (kSlot, kDrive);

        Assert::AreEqual ((size_t) 1, rig.PreservedPaths().size(),
                          L"the version the eject displaced is on disk");
        Assert::IsFalse (rig.store.IsMounted (kSlot, kDrive));
    }



    TEST_METHOD (AMountFromBytesIsNeverStaleCheckedIntoARefusedFlush)
    {
        SharedImageRig           rig;
        vector<Byte>  bytes (NibbleFormat::kImageByteSize, 0x55);



        //  Nothing backs it, so there is no identity to compare and nothing to
        //  refuse. Without the recorded-identity gate this flush would fail.
        AssertSucceeded (rig.store.MountFromBytes (kSlot, kDrive, "memory.dsk",
                                                   DiskFormat::Dsk, bytes));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertSucceeded (rig.store.GetFlusher().Flush (kSlot, kDrive));
    }
};
