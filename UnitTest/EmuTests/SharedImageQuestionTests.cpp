#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Devices/Disk/Store/ExternalChangeMonitor.h"
#include "Devices/Disk/Store/ExternalChangeActions.h"
#include "Devices/Disk/Store/DiskFlusher.h"
#include "SharedImageRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageQuestionTests
//
//  The question put to the user about a changed image, each answer, and the
//  copies and saves that follow from it.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (SharedImageQuestionTests)
{
public:

    static constexpr int           kSlot      = SharedImageRig::kSlot;
    static constexpr int           kDrive     = SharedImageRig::kDrive;
    static constexpr const char *  kDirectory = SharedImageRig::kDirectory;
    static constexpr const char *  kImagePath = SharedImageRig::kImagePath;

    //  A CHANGE NOBODY EXPLAINED IS A QUESTION, AND ANSWERING IT IS NOT A
    //  RELOAD BY WHOEVER WROTE. The notice raised afterwards claimed CassoCli
    //  had modified the file and inserted it -- on a change the watcher found
    //  and an insertion the user asked for -- and claimed a preserved copy
    //  under the name the question had merely reserved. A disk the guest never
    //  writes to reaches all of it with no dirty bit anywhere.
    TEST_METHOD (AnAnsweredQuestionReportsNeitherAWriterNorACopy)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  Something rewrote the file and said nothing about why.
        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"an unexplained change is a question");
        Assert::AreEqual ((size_t) 0, rig.reports.size(), L"and not yet a notice");
        Assert::IsFalse (rig.store.GetImage (kSlot, kDrive)->IsDirty(),
                         L"a disk that boots and is read has nothing to save");
        Assert::AreEqual ((size_t) 0, rig.PreservedPaths().size(),
                          L"putting the question reserves a name, it does not write a file");

        //  "Insert the modified <file>."
        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::ReloadInPlace);

        Assert::AreEqual ((size_t) 1, rig.reports.size());
        Assert::AreEqual ((size_t) 0, rig.PreservedPaths().size(),
                          L"and answering it does not write one either");

        Assert::IsTrue (rig.reports[0].message.find (L"renamed") == std::wstring::npos,
                        L"so the notice must not report a rename that never happened");
        Assert::IsTrue (rig.reports[0].message.find (L"CassoCli") == std::wstring::npos,
                        L"and nothing said who wrote, so it must not name a program");
    }



    //  A WRITE THAT LANDS UNDER THE QUESTION DOES NOT RE-LABEL THE ANSWER. A
    //  bay keeps only the newest change, so a build finishing while the dialog
    //  is on screen replaces the intent the question was raised for. Reading
    //  the record when the answer came back credited the user's own insertion
    //  to CassoCli -- the very sentence saying somebody else pressed the button
    //  they had just pressed.
    TEST_METHOD (AStatedIntentArrivingUnderTheQuestionDoesNotReattributeTheAnswer)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  Something rewrote the file and said nothing about why, so it asks.
        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"an unexplained change is a question");

        //  A build finishes while the user is still reading it.
        rig.WriteImage (kImagePath, 0x33);
        rig.store.GetChangeMonitor().NoteExternalChange (kImagePath, ExternalChangeIntent::ReloadInPlace);

        //  "Insert the modified <file>."
        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::ReloadInPlace);

        Assert::AreEqual ((size_t) 1, rig.reports.size());
        Assert::IsTrue (rig.reports[0].message.find (L"CassoCli") == std::wstring::npos,
                        L"the user pressed the button, whoever wrote last");
        Assert::IsTrue (rig.reports[0].message.find (L"inserted it") == std::wstring::npos,
                        L"and the insertion stays theirs");
    }

    //  A QUESTION WAVED AWAY GIVES ITS NAME BACK. Putting the question reserves
    //  a preserved name stamped with that moment, and a copy written under a
    //  name it is handed uses it as it stands. Holding the reservation through
    //  a dismissal meant a genuine conflict an hour later filed the guest's
    //  disk under an hour-old timestamp.
    TEST_METHOD (AnIgnoredQuestionDoesNotNameTheNextCopyAfterIt)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  Something rewrote the file and said nothing about why, so it asks.
        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"an unexplained change is a question");
        Assert::AreEqual ((size_t) 0, rig.PreservedPaths().size(),
                          L"which reserves a name without writing anything to it");

        //  "Leave it alone."
        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::Ignore);

        Assert::AreEqual ((size_t) 0, rig.PreservedPaths().size(),
                          L"and dismissing it writes nothing either");

        //  An hour goes by. The guest writes, and the file changes underneath
        //  it -- a real conflict this time, with a copy to make.
        rig.wallClock += 3600;

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        AssertSucceeded (rig.store.GetFlusher().Flush (kSlot, kDrive));

        Assert::AreEqual ((size_t) 1, rig.PreservedPaths().size(),
                          L"the guest's version goes to a file of its own");

        {
            const std::string  stale = PreservedCopy::MakePath (
                                           kImagePath,
                                           PreservedCopy::MakeStamp (rig.wallClock - 3600), 0);
            const std::string  fresh = PreservedCopy::MakePath (
                                           kImagePath,
                                           PreservedCopy::MakeStamp (rig.wallClock), 0);

            Assert::AreNotEqual (stale, rig.PreservedPaths()[0],
                                 L"not under the name the dismissed question had reserved");
            Assert::AreEqual (fresh, rig.PreservedPaths()[0],
                              L"but under one stamped with the moment the copy was made");
        }
    }

    //  The other question that reserves a name and can end without a copy: the
    //  one raised when the copy could not be written. Dismissing it leaves the
    //  name it tried standing, and the next copy inherited it.
    TEST_METHOD (ADismissedSaveFailureDoesNotNameTheNextCopyAfterIt)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  The guest has written, the file changed underneath it, and the copy
        //  that would settle the conflict cannot be written.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        rig.refusePreserve = true;
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"a copy that cannot be written is a question, not a notice");
        Assert::AreEqual ((size_t) 0, rig.PreservedPaths().size(),
                          L"and nothing reached the disk under the name it tried");

        //  "Leave it alone" rather than "Save as...".
        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::Ignore);

        //  An hour later the conflict is real again, and this time it can write.
        rig.wallClock     += 3600;
        rig.refusePreserve = false;

        AssertSucceeded (rig.store.GetFlusher().Flush (kSlot, kDrive));

        Assert::AreEqual ((size_t) 1, rig.PreservedPaths().size());

        {
            const std::string  stale = PreservedCopy::MakePath (
                                           kImagePath,
                                           PreservedCopy::MakeStamp (rig.wallClock - 3600), 0);

            Assert::AreNotEqual (stale, rig.PreservedPaths()[0],
                                 L"not under the name the failed write had tried");
            Assert::AreEqual (PreservedCopy::MakePath (
                                  kImagePath,
                                  PreservedCopy::MakeStamp (rig.wallClock), 0),
                              rig.PreservedPaths()[0],
                              L"but under one stamped with the moment the copy was made");
        }
    }



    //  A QUESTION THAT NEVER REACHED ANYBODY DOES NOT COUNT AS ONE. The shell
    //  posts it to its own window, and it installs the sink before that window
    //  exists; a post can also fail on a full queue. The bay used to be marked
    //  as having a question outstanding either way, and since nothing acts on
    //  a bay while one stands, that bay went silent until it was ejected.
    TEST_METHOD (AQuestionThatCouldNotBePutLeavesTheBayStillListening)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.askSinkDelivers = false;

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"the sink was called");
        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->IsAskOutstanding(),
                         L"but nothing is waiting on an answer nobody can give");

        //  So the next change is acted on rather than swallowed.
        rig.askSinkDelivers = true;

        rig.WriteImage (kImagePath, 0x33);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        Assert::AreEqual ((size_t) 1, rig.reports.size(),
                          L"the bay is still listening");
    }



    //  A QUESTION ON SCREEN OWNS THE BAY UNTIL IT IS ANSWERED. Mount and eject
    //  both clear the outstanding-question flag, so a disk that leaves the
    //  drive takes its question with it and the answer has nothing left to
    //  apply. Acting anyway carried the answer onto whatever went in next:
    //  "keep the one I have" moved the new bay onto the departed disk's rescue
    //  copy, which the next flush then wrote over.
    TEST_METHOD (AnAnswerToAQuestionAboutADiskThatHasLeftIsDropped)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"an unexplained change is a question");

        //  The user takes the disk out and puts one back, and only then
        //  reaches for the dialog that is still on screen.
        rig.store.Eject (kSlot, kDrive);

        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::KeepHeld, "");

        Assert::AreEqual ((size_t) 0, rig.reports.size(),
                          L"nothing is reported about a disk that is gone");
        Assert::AreEqual ((size_t) 0, rig.PreservedPaths().size(),
                          L"and nothing is written on its behalf");
        Assert::AreEqual (std::string (kImagePath), rig.store.GetSourcePath (kSlot, kDrive),
                          L"the disk now in the drive keeps its own file");
    }



    //  AND A LATER CHANGE WAITS BEHIND THE ANSWER, however it arrived. A
    //  stated intent used to skip the question entirely -- it needs no answer
    //  of its own -- and swapped the disk out from under the dialog, so
    //  "keep your current version" came to mean the other program's version.
    TEST_METHOD (AStatedIntentWaitsWhileAQuestionStands)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"an unexplained change is a question");

        {
            const Byte  held = SharedImageRig::FirstTrackByte (rig.store);

            rig.WriteImage (kImagePath, 0x33);
            rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

            Assert::AreEqual ((size_t) 1, rig.questions.size(),
                              L"and it is not asked a second time");
            Assert::AreEqual ((size_t) 0, rig.reports.size(),
                              L"nor reported over the top of the question");
            Assert::AreEqual ((int) held, (int) SharedImageRig::FirstTrackByte (rig.store),
                              L"the disk under the dialog is the one it asked about");
        }

        //  Answering lands the newest bytes, not the ones the question was
        //  composed from. Nibblization makes a fill byte unrecognizable on the
        //  track, so what each fill looks like there is measured rather than
        //  written down.
        {
            SharedImageRig  reference;

            reference.WriteImage (kImagePath, 0x33);
            AssertSucceeded (reference.store.Mount (kSlot, kDrive, kImagePath));

            rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::ReloadInPlace, "");

            Assert::AreEqual ((int) SharedImageRig::FirstTrackByte (reference.store),
                              (int) SharedImageRig::FirstTrackByte (rig.store),
                              L"the answer takes up whatever is on disk when it comes");
        }
    }



    TEST_METHOD (AnAnswerOfTakeItUpSwapsTheContents)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size());

        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::ReloadInPlace);

        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->GetPending().seen);
        Assert::IsTrue (rig.store.GetSharedState (kSlot, kDrive)->GetIdentity()
                            .Matches (rig.identities[kImagePath]));
    }



    TEST_METHOD (KeepingACleanDiskWritesItOutRatherThanLeavingItInMemory)
    {
        SharedImageRig  rig;
        Byte            kept  = 0;
        string          where;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        kept = SharedImageRig::FirstTrackByte (rig.store);

        //  Nothing from the guest, so the bay is clean -- which is exactly the
        //  case that used to lose the disk.
        Assert::IsFalse (rig.store.GetImage (kSlot, kDrive)->IsDirty());

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::KeepHeld);

        Assert::AreEqual ((size_t) 1, rig.PreservedPaths().size(),
                          L"keeping wrote the kept version to a file of its own");

        where = rig.PreservedPaths()[0];

        Assert::AreEqual (where, rig.store.GetSourcePath (kSlot, kDrive),
                          L"and the bay reads and writes that file from here on");

        Assert::AreEqual ((int) 0x22, (int) rig.files[kImagePath][0],
                          L"while the original keeps what the other program wrote");

        //  The disk in the drive did not move. Keeping is a save-as, not a
        //  swap, so a guest mid-read sees nothing at all.
        Assert::AreEqual ((int) kept, (int) SharedImageRig::FirstTrackByte (rig.store),
                          L"the disk in the drive is untouched");

        //  And it outlives the session, which is the whole point.
        rig.store.Eject (kSlot, kDrive);

        Assert::IsTrue (rig.files.find (where) != rig.files.end(),
                        L"the kept version is still on disk after the eject");
    }



    TEST_METHOD (AnAnswerOfKeepWhatIHaveLetsALaterFlushProceed)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::KeepHeld);

        //  The user saw both and chose the one in memory. Without recording
        //  the file as seen, the re-check before every commit would refuse
        //  forever and strand the guest's work in memory for the session.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertSucceeded (rig.store.GetFlusher().Flush (kSlot, kDrive));
    }



    TEST_METHOD (ADeletedImageAndAnUnreadableOneAreDistinguished)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  Gone.
        rig.files.erase (kImagePath);
        rig.Stamp (kImagePath);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size());
        Assert::IsTrue (rig.questions[0].title.find (L"removed") != std::wstring::npos,
                        L"a user whose file was deleted needs to be told it is gone");

        //  Present, but not this disk any more.
        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::Discard);

        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.files[kImagePath] = vector<Byte> (37, 0xAB);
        rig.Stamp (kImagePath);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 2, rig.questions.size());
        Assert::IsTrue (rig.questions[1].title.find (L"can't be read")
                            != std::wstring::npos,
                        L"and one whose share dropped needs to be told that instead");
    }



    //  SAVING KEEPS THE DISK IN THE DRIVE, on the file the user picked.
    //  Serialize writes a whole image rather than a fragment, so once it lands
    //  the drive has somewhere to live -- and it used to be emptied anyway,
    //  which handed back a complete disk and then made the user go and find it.
    //  Keeping a version during a conflict already behaved this way; one act
    //  should not have two outcomes.
    TEST_METHOD (SavingTheInMemoryCopyKeepsItInTheDrive)
    {
        SharedImageRig          rig;
        std::string  chosen = "C:\\elsewhere\\Rescued.dsk";
        Byte         held   = 0;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        held = SharedImageRig::FirstTrackByte (rig.store);

        rig.files.erase (kImagePath);
        rig.Stamp (kImagePath);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size());

        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::PreserveCopy, chosen);

        Assert::IsTrue (rig.files.count (chosen) == 1,
                        L"what the emulator held may be the only copy left");

        Assert::IsTrue (rig.store.IsMounted (kSlot, kDrive),
                        L"the disk is still in the drive");
        Assert::AreEqual (chosen, rig.store.GetSourcePath (kSlot, kDrive),
                          L"and the drive reads and writes the rescued file now");
        Assert::AreEqual ((int) held, (int) SharedImageRig::FirstTrackByte (rig.store),
                          L"the disk itself is untouched -- only its file moved");
    }



    //  A FILE FOUND GONE WHILE ACTING IS ASKED ABOUT, NOT REPORTED. The user
    //  answers "take up the new version", the re-read finds the file has since
    //  been deleted, and what is left to offer is the same pair the watcher
    //  would have offered. Sent to the notice bar instead, both buttons did
    //  nothing: the bar routes no answers.
    TEST_METHOD (ALostFileFoundWhileActingIsAskedNotReported)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"an unexplained change is a question");

        //  Gone between the question and the answer.
        rig.files.erase (kImagePath);
        rig.Stamp (kImagePath);

        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::ReloadInPlace);

        Assert::AreEqual ((size_t) 2, rig.questions.size(),
                          L"asked again, about the file that has gone");
        Assert::AreEqual ((size_t) 0, rig.reports.size(),
                          L"and not put where no answer can be given");

        Assert::AreEqual ((size_t) 2, rig.questions[1].answers.size());
        Assert::IsTrue (rig.questions[1].answers[0].action == ChangeAction::PreserveCopy);
        Assert::IsTrue (rig.questions[1].answers[1].action == ChangeAction::Discard);
    }



    TEST_METHOD (DecliningToSaveStillEmptiesTheDrive)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.files.erase (kImagePath);
        rig.Stamp (kImagePath);
        rig.FireAndSettle (kImagePath);

        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::Discard);

        Assert::IsFalse (rig.store.IsMounted (kSlot, kDrive));
        Assert::AreEqual ((size_t) 0, rig.PreservedPaths().size(),
                          L"declining saves nothing, and that is the whole of it");
    }



    TEST_METHOD (ASaveThatFailsDoesNotThenThrowTheOnlyCopyAway)
    {
        SharedImageRig          rig;
        std::string  chosen = "C:\\elsewhere\\Rescued.dsk";



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.files.erase (kImagePath);
        rig.Stamp (kImagePath);
        rig.FireAndSettle (kImagePath);

        rig.refusePreserve = true;

        rig.store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::PreserveCopy, chosen);

        //  A save the user asked for and did not get must not be followed by
        //  emptying the drive it was the only copy in.
        Assert::IsTrue (rig.store.IsMounted (kSlot, kDrive));
        Assert::IsTrue (rig.files.count (chosen) == 0);
    }
};
