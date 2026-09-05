#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeImageWatcher.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/CommitPlan.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageTests
//
//  A disk image changed by something else while the emulator holds it.
//
//  NOT ONE BYTE OF THIS TOUCHES THE FILESYSTEM. The store's read, write and
//  identity seams are all redirected into memory, and the watcher is fired by
//  hand, so a change, a burst of changes and a whole quiet period all happen
//  instantly and in any order the test wants.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SharedImageTests)
{
public:

    static constexpr int   kSlot  = 6;
    static constexpr int   kDrive = 0;

    static constexpr const char *  kDirectory = "C:\\work";
    static constexpr const char *  kImagePath = "C:\\work\\Loader.dsk";



    struct Rig;


    //
    //  Only the two questions the store asks the platform outside its read,
    //  write and identity seams: whether a path is taken, and whether somebody
    //  else is holding it.
    //
    //  IT ANSWERS OUT OF THE RIG'S OWN FILE MAP, which is what makes the
    //  preserved-name collision loop testable: a candidate name a test has
    //  already used has to read as taken.
    //
    struct RigFileIo : public IDiskFileIo
    {
        Rig &  rig;

        explicit RigFileIo (Rig & owner) : rig (owner) {}

        bool     Exists (const std::string & path) override;
        bool     IsHeldByAnotherProcess (const std::string &) override { return false; }

        //  Nothing below is reached: the store reads, writes and stats through
        //  its own seams, and this exists for the two questions above.
        HRESULT  ReadAllBytes  (const std::string &, std::vector<Byte> &) override { return E_NOTIMPL; }
        HRESULT  WriteAllBytes (const std::string &, const std::vector<Byte> &) override { return E_NOTIMPL; }
        HRESULT  Stat          (const std::string &, FileStamp &) override { return E_NOTIMPL; }
        HRESULT  Remove        (const std::string &) override { return E_NOTIMPL; }
        HRESULT  ReplaceAtomically (const std::string &, const std::string &) override { return E_NOTIMPL; }
        HRESULT  WritePayloadToStandardOutput (const std::vector<Byte> &) override { return E_NOTIMPL; }
    };


    //
    //  A store with every platform seam replaced: reads and writes land in
    //  this object, identities come from a number this object controls, and
    //  changes are fired by hand.
    //
    struct Rig
    {
        DiskImageStore                                 store;
        FakeImageWatcher                               watcher;
        RigFileIo                                      fileIo { *this };
        std::unordered_map<std::string, vector<Byte>>  files;
        std::unordered_map<std::string, ImageIdentity> identities;
        int64_t                                        nowMs = 0;

        //  Fixed, so every preserved copy in one test is stamped alike unless
        //  the test says otherwise.
        time_t                                         wallClock = 1756500000;

        //  Makes every attempt to write a preserved copy fail, which is the
        //  only way to reach the rule that a version is never destroyed for
        //  want of somewhere to put the other one.
        bool                                           refusePreserve = false;

        //  Makes the ask sink report that the question never reached anywhere
        //  it could be answered -- what the shell says when it has no window
        //  yet, or when the post fails.
        bool                                           askSinkDelivers = true;

        //  Where the shutdown rescue dialog says to put the disk, and how many
        //  times it was raised. An empty path stands for the user declining.
        std::string                                    rescueChoice;
        int                                            rescuesAsked = 0;

        //  What the store reported and did.
        std::vector<ChangePrompt>                      reports;
        std::vector<ChangePrompt>                      questions;
        int                                            restarts = 0;

        //  How many times the store has read an image file. Reading is the
        //  expensive half of noticing a change, so a test can hold this still
        //  to prove nothing is being re-read on every tick.
        int                                            reads = 0;

        //  Every bay change the store announced, in order, so a test can prove
        //  the one signal the shell reacts to fires on each path.
        std::vector<BayChange>         bayChanges;



        Rig()
        {
            store.SetImageWatcher (&watcher);

            store.SetImageReader ([this] (const string & path, vector<Byte> & bytes) -> HRESULT
            {
                auto  found = files.find (path);

                reads++;

                if (found == files.end())
                {
                    return E_FAIL;
                }

                bytes = found->second;

                return S_OK;
            });

            store.SetFlushSink ([this] (const string & path, const vector<Byte> & bytes) -> HRESULT
            {
                bool  isPreserved = (path != kImagePath);

                if (isPreserved)
                {
                    if (refusePreserve)
                    {
                        return E_ACCESSDENIED;
                    }

                    preserved.push_back (path);
                }

                files[path] = bytes;
                Stamp (path);

                return S_OK;
            });

            store.SetIdentityReader ([this] (const string & path) -> ImageIdentity
            {
                auto  found = identities.find (path);

                return (found != identities.end()) ? found->second : ImageIdentity();
            });

            store.SetFileIo (&fileIo);

            store.SetClock ([this] () { return nowMs; });

            //  A wall clock that does not move unless a test moves it, which
            //  is what makes the same-second collision case reachable at all.
            store.SetTimestampSource ([this] () { return wallClock; });

            store.SetChangeReportSink ([this] (int, int, const ChangePrompt & prompt)
            {
                reports.push_back (prompt);
            });

            store.SetAskSink ([this] (int, int, const ChangePrompt & prompt) -> bool
            {
                questions.push_back (prompt);

                return askSinkDelivers;
            });

            //  The blocking last-chance dialog. Present from the start, the way
            //  the shell installs it, so a test that wants it declined simply
            //  leaves rescueChoice empty.
            store.SetRescueSink ([this] (const std::string &, std::string & outPath) -> bool
            {
                rescuesAsked++;

                if (rescueChoice.empty())
                {
                    return false;
                }

                //  Picking somewhere else is what makes the write work: the
                //  folder this store chose for itself is the one that refused
                //  it. Without this the rig would refuse the user's choice too
                //  and the test would prove the opposite of what it says.
                refusePreserve = false;

                outPath = rescueChoice;

                return true;
            });

            store.SetMachineRestartCallback ([this] () { restarts++; });

            store.SetBayChangeSink ([this] (int, int, BayChange change)
            {
                bayChanges.push_back (change);
            });
        }



        //  Gives a path a fresh identity, as a real write would.
        void  Stamp (const std::string & path)
        {
            ImageIdentity  identity;

            identity.recorded           = true;
            identity.stamp.sizeBytes    = files.count (path) ? files[path].size() : 0;
            identity.stamp.modifiedUnix = ++m_tick;

            identities[path] = identity;
        }



        //  Every preserved copy written so far, in the order they were made.
        const std::vector<std::string> &  PreservedPaths() const { return preserved; }



        //  Puts an image on "disk" with a distinct byte pattern.
        void  WriteImage (const std::string & path, Byte fill)
        {
            files[path] = vector<Byte> (NibblizationLayer::kImageByteSize, fill);
            Stamp (path);
        }



        //  A change arrives, settles, and the machine reaches a quiet moment.
        //
        //  THE INTENT IS OPTIONAL BECAUSE IT IS OPTIONAL IN LIFE. A watcher
        //  reports every change; only this project's own command line also
        //  says what it meant by one, and a real session that gets both gets
        //  them in this order.
        void  FireAndSettle (const std::string & path,
                             ExternalChangeIntent        intent = ExternalChangeIntent::Unstated)
        {
            watcher.Fire (kDirectory, path);

            if (intent != ExternalChangeIntent::Unstated)
            {
                store.NoteExternalChange (path, intent);
            }

            nowMs += MountedImageState::kQuietPeriodMs;
            store.ApplyPendingReload();
        }

        std::vector<std::string>  preserved;

    private:
        int64_t  m_tick = 100;
    };



    //  What the guest sees on a track, as a stand-in for "the disk changed".
    static Byte  FirstTrackByte (DiskImageStore & store)
    {
        DiskImage *  image = store.GetImage (kSlot, kDrive);

        Assert::IsNotNull (image, L"a mounted bay has an image");

        return image->GetTrackBits (0).empty() ? 0 : image->GetTrackBits (0)[0];
    }



    TEST_METHOD (MountRecordsAnIdentityAndTakesUpAWatch)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);

        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        Assert::IsTrue (rig.store.GetSharedState (kSlot, kDrive)->GetIdentity().recorded,
                        L"nothing can answer 'has this changed' without one");
        Assert::IsTrue (rig.store.GetSharedState (kSlot, kDrive)->IsWatching());
        Assert::AreEqual ((size_t) 1, rig.watcher.watched.size());
        Assert::IsTrue (rig.watcher.watched[0] == std::string (kDirectory),
                        L"the directory, not the file: a rename over the image "
                        L"would take a file watch out with it");
    }



    TEST_METHOD (EjectDropsTheWatchAndTheIdentity)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.Eject (kSlot, kDrive);

        Assert::AreEqual ((size_t) 0, rig.watcher.watched.size());
        Assert::AreEqual ((size_t) 1, rig.watcher.unwatched.size());
        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->GetIdentity().recorded);
    }



    TEST_METHOD (AWatchIsKeptWhileAnotherBayStillNeedsItsDirectory)
    {
        Rig          rig;
        std::string  second = "C:\\work\\Data.dsk";



        rig.WriteImage (kImagePath, 0x11);
        rig.WriteImage (second,     0x22);

        AssertSucceeded (rig.store.Mount (kSlot, 0, kImagePath));
        AssertSucceeded (rig.store.Mount (kSlot, 1, second));

        rig.store.Eject (kSlot, 0);

        //  A boot disk and a work disk built by one script sit in one folder,
        //  and ejecting the first must not blind the second.
        Assert::AreEqual ((size_t) 1, rig.watcher.watched.size());
        Assert::AreEqual ((size_t) 0, rig.watcher.unwatched.size());
    }



    //  A CHANGE NOBODY EXPLAINED IS A QUESTION, AND ANSWERING IT IS NOT A
    //  RELOAD BY WHOEVER WROTE. The notice raised afterwards claimed CassoCli
    //  had modified the file and inserted it -- on a change the watcher found
    //  and an insertion the user asked for -- and claimed a preserved copy
    //  under the name the question had merely reserved. A disk the guest never
    //  writes to reaches all of it with no dirty bit anywhere.
    TEST_METHOD (AnAnsweredQuestionReportsNeitherAWriterNorACopy)
    {
        Rig  rig;



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
        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::ReloadInPlace);

        Assert::AreEqual ((size_t) 1, rig.reports.size());
        Assert::AreEqual ((size_t) 0, rig.PreservedPaths().size(),
                          L"and answering it does not write one either");

        Assert::IsTrue (rig.reports[0].message.find (L"renamed") == std::wstring::npos,
                        L"so the notice must not report a rename that never happened");
        Assert::IsTrue (rig.reports[0].message.find (L"CassoCli") == std::wstring::npos,
                        L"and nothing said who wrote, so it must not name a program");
    }



    //  The other half. A write that DID say what it was for keeps the sentence
    //  that makes the feature worth having.
    TEST_METHOD (AStatedIntentIsAttributedToTheOneThingThatCanStateIt)
    {
        Rig  rig;



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



    //  A WRITE THAT LANDS UNDER THE QUESTION DOES NOT RE-LABEL THE ANSWER. A
    //  bay keeps only the newest change, so a build finishing while the dialog
    //  is on screen replaces the intent the question was raised for. Reading
    //  the record when the answer came back credited the user's own insertion
    //  to CassoCli -- the very sentence saying somebody else pressed the button
    //  they had just pressed.
    TEST_METHOD (AStatedIntentArrivingUnderTheQuestionDoesNotReattributeTheAnswer)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  Something rewrote the file and said nothing about why, so it asks.
        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"an unexplained change is a question");

        //  A build finishes while the user is still reading it.
        rig.WriteImage (kImagePath, 0x33);
        rig.store.NoteExternalChange (kImagePath, ExternalChangeIntent::ReloadInPlace);

        //  "Insert the modified <file>."
        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::ReloadInPlace);

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
        Rig  rig;



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
        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::Ignore);

        Assert::AreEqual ((size_t) 0, rig.PreservedPaths().size(),
                          L"and dismissing it writes nothing either");

        //  An hour goes by. The guest writes, and the file changes underneath
        //  it -- a real conflict this time, with a copy to make.
        rig.wallClock += 3600;

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        AssertSucceeded (rig.store.Flush (kSlot, kDrive));

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
        Rig  rig;



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
        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::Ignore);

        //  An hour later the conflict is real again, and this time it can write.
        rig.wallClock     += 3600;
        rig.refusePreserve = false;

        AssertSucceeded (rig.store.Flush (kSlot, kDrive));

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

    //  ONE FILE, ONE DRIVE. Two bays on one file each hold their own copy of
    //  the disk, and a flush writes the whole image -- so from the guest's
    //  first write each drive overwrites whatever the other saved. One
    //  external change raises the conflict twice and puts two dialogs up.
    TEST_METHOD (AFileAlreadyInADriveIsRefusedByTheOther)
    {
        Rig             rig;
        MountDiagnosis  diagnosis;
        HRESULT         hr = S_OK;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        hr = rig.store.Mount (kSlot, kDrive + 1, kImagePath, diagnosis);

        Assert::IsTrue (FAILED (hr), L"the second drive refuses it");
        Assert::IsTrue (diagnosis.failure == MountFailure::AlreadyMounted);
        Assert::AreEqual (kDrive, diagnosis.occupiedDrive,
                          L"and says which drive has it");
        Assert::IsFalse (rig.store.IsMounted (kSlot, kDrive + 1));

        //  The message says which drive has it rather than leaving the reader
        //  to look in both.
        Assert::IsTrue (diagnosis.Describe().find ("drive 1") != std::string::npos);
    }



    //  AND PUTTING THE SAME FILE BACK INTO THE SAME DRIVE IS NOT THAT. A
    //  machine switch and a reload both re-mount, so the bay being mounted
    //  into is the one bay the check does not look at.
    TEST_METHOD (AReMountOfTheSameFileIntoItsOwnDriveStillWorks)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath),
                         L"the same file back into the drive that has it");

        Assert::IsTrue (rig.store.IsMounted (kSlot, kDrive));
    }



    //  AND THE DRIVE IT LEFT IS FREE AGAIN.
    TEST_METHOD (EjectingAFileLetsTheOtherDriveTakeIt)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.Eject (kSlot, kDrive);

        AssertSucceeded (rig.store.Mount (kSlot, kDrive + 1, kImagePath));

        Assert::IsTrue (rig.store.IsMounted (kSlot, kDrive + 1));
    }



    //  A QUESTION THAT NEVER REACHED ANYBODY DOES NOT COUNT AS ONE. The shell
    //  posts it to its own window, and it installs the sink before that window
    //  exists; a post can also fail on a full queue. The bay used to be marked
    //  as having a question outstanding either way, and since nothing acts on
    //  a bay while one stands, that bay went silent until it was ejected.
    TEST_METHOD (AQuestionThatCouldNotBePutLeavesTheBayStillListening)
    {
        Rig  rig;



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
        Rig  rig;



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

        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::KeepHeld, "");

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
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"an unexplained change is a question");

        {
            const Byte  held = FirstTrackByte (rig.store);

            rig.WriteImage (kImagePath, 0x33);
            rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

            Assert::AreEqual ((size_t) 1, rig.questions.size(),
                              L"and it is not asked a second time");
            Assert::AreEqual ((size_t) 0, rig.reports.size(),
                              L"nor reported over the top of the question");
            Assert::AreEqual ((int) held, (int) FirstTrackByte (rig.store),
                              L"the disk under the dialog is the one it asked about");
        }

        //  Answering lands the newest bytes, not the ones the question was
        //  composed from. Nibblization makes a fill byte unrecognizable on the
        //  track, so what each fill looks like there is measured rather than
        //  written down.
        {
            Rig  reference;

            reference.WriteImage (kImagePath, 0x33);
            AssertSucceeded (reference.store.Mount (kSlot, kDrive, kImagePath));

            rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::ReloadInPlace, "");

            Assert::AreEqual ((int) FirstTrackByte (reference.store),
                              (int) FirstTrackByte (rig.store),
                              L"the answer takes up whatever is on disk when it comes");
        }
    }



    //  AND THE FILE IS NOT RE-READ WHILE THE USER READS THE QUESTION. The
    //  change stays pending by design until an answer arrives, so every idle
    //  tick used to read the whole image and nibblize it before reaching the
    //  point where an outstanding question was noticed.
    TEST_METHOD (AStandingQuestionDoesNotReReadTheImage)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"an unexplained change is a question");

        {
            const int  before = rig.reads;

            rig.nowMs += 5000;
            rig.store.ApplyPendingReload();
            rig.store.ApplyPendingReload();
            rig.store.ApplyPendingReload();

            Assert::AreEqual (before, rig.reads,
                              L"an idle tick under a standing question reads nothing");
        }
    }



    //  AND A RESERVATION DOES NOT OUTLIVE THE DISK IT WAS MADE FOR. Neither
    //  eject clears one, which costs nothing while the bay stands empty --
    //  nothing reads a reservation on an empty bay. The disk mounted next is
    //  what inherited it, and filed its own copy under the name and the moment
    //  belonging to a disk that had already left the drive.
    TEST_METHOD (AReservationDoesNotOutliveTheDiskItWasMadeFor)
    {
        Rig  rig;



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

        AssertSucceeded (rig.store.Flush (kSlot, kDrive));

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




    TEST_METHOD (AChangedImageIsPickedUpAndTheIdentityRefreshed)
    {
        Rig  rig;



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



    TEST_METHOD (TheStoreDoesNotReportItsOwnWriteAsAnExternalChange)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        //  A guest write, flushed the way the motor-off hook flushes it. The
        //  flush changes the file, and a directory watcher reports that.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertSucceeded (rig.store.Flush (kSlot, kDrive));

        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 0, rig.reports.size(),
                          L"the emulator's own commit is not somebody else's change");
        Assert::AreEqual (0, rig.restarts);
    }



    TEST_METHOD (ASecondFlushIsNotRefusedByTheFirstFlushesOwnChange)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertSucceeded (rig.store.Flush (kSlot, kDrive));

        //  Without the post-commit refresh this is where every ordinary
        //  session breaks: the first flush changes the file, nothing updates
        //  what was recorded, and the second flush refuses itself.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[1] = 0x7E;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertSucceeded (rig.store.Flush (kSlot, kDrive));
    }



    //  THE OUTCOME MUST NOT DEPEND ON WHICH END FOUND THE COLLISION. This is
    //  the flush half; the watcher half is below. Both leave the original
    //  holding the other program's version and the guest's version in a file
    //  of its own, so the same collision produces the same two files either
    //  way. It did not use to: whichever side was quicker decided which
    //  version kept the original name.
    TEST_METHOD (AFlushOverAnExternalChangeLeavesTheFileAloneAndMovesTheGuestsVersion)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  The guest writes, and something else rewrites the file before the
        //  flush lands -- the same conflict the watcher finds, discovered at
        //  the other end because notification never arrived.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        AssertSucceeded (rig.store.Flush (kSlot, kDrive));

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
        Rig      rig;
        HRESULT  hrFlush = S_OK;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        rig.refusePreserve = true;

        hrFlush = rig.store.Flush (kSlot, kDrive);

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
        Rig          rig;
        HRESULT      hrFlush = S_OK;
        std::string  chosen  = "C:\\elsewhere\\Rescued.dsk";



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        rig.refusePreserve = true;

        hrFlush = rig.store.Flush (kSlot, kDrive);

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

        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::PreserveCopy, chosen);

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
        Rig          rig;
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

        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::PreserveCopy, chosen);

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
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        rig.refusePreserve = true;

        rig.store.Eject (kSlot, kDrive);

        Assert::AreEqual ((size_t) 1, rig.questions.size());

        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::Discard, "");

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
        Rig  rig;



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
        Rig  rig;



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
        HRESULT  hrFlush = rig.store.FlushAllForShutdown();

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
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        rig.refusePreserve = true;

        //  rescueChoice left empty: the user closed the picker.
        rig.store.FlushAllForShutdown();

        Assert::AreEqual (1, rig.rescuesAsked);
        Assert::AreEqual ((size_t) 0, rig.PreservedPaths().size(),
                          L"nothing was written anywhere");
        Assert::AreEqual ((int) 0x33, (int) rig.files[kImagePath][0],
                          L"and the file keeps the other program's version");
    }



    TEST_METHOD (ThreeChangesInsideTheQuietPeriodProduceOneReload)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        //  A disk carrying more than one thing is built by more than one
        //  command, and the developer means one build.
        rig.WriteImage (kImagePath, 0x22);
        rig.watcher.Fire (kDirectory, kImagePath);

        rig.nowMs += MountedImageState::kQuietPeriodMs / 2;
        rig.store.ApplyPendingReload();
        Assert::AreEqual ((size_t) 0, rig.reports.size(), L"still being written");

        rig.WriteImage (kImagePath, 0x33);
        rig.watcher.Fire (kDirectory, kImagePath);

        rig.nowMs += MountedImageState::kQuietPeriodMs / 2;
        rig.store.ApplyPendingReload();
        Assert::AreEqual ((size_t) 0, rig.reports.size(),
                          L"the third change resets the timer -- the period is "
                          L"measured from the LAST change, not the first");

        rig.WriteImage (kImagePath, 0x44);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        Assert::AreEqual ((size_t) 1, rig.reports.size(), L"one build, one pick-up");
    }



    TEST_METHOD (TwoSpellingsOfOnePathReachTheSameBayAndAnotherFileReachesNone)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  What a watcher reports and what a mount recorded rarely match as
        //  strings.
        rig.store.NoteExternalChange ("c:/WORK/loader.dsk", ExternalChangeIntent::Unstated);
        Assert::IsTrue (rig.store.GetSharedState (kSlot, kDrive)->GetPending().seen);

        rig.store.GetSharedState (kSlot, kDrive);
        rig.store.Eject (kSlot, kDrive);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.NoteExternalChange ("C:\\work\\Something Else.dsk", ExternalChangeIntent::Unstated);
        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->GetPending().seen,
                         L"a directory watch reports every file under it");
    }



    TEST_METHOD (AStatedRestartRestartsTheMachineWithoutAsking)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.store.NoteExternalChange (kImagePath, ExternalChangeIntent::Restart);

        rig.nowMs += MountedImageState::kQuietPeriodMs;
        rig.store.ApplyPendingReload();

        Assert::AreEqual (1, rig.restarts);
        Assert::AreEqual ((size_t) 0, rig.questions.size(),
                          L"the writer knew what they changed");
    }



    TEST_METHOD (ALaterStatedIntentReplacesAnEarlierOne)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.NoteExternalChange (kImagePath, ExternalChangeIntent::Restart);
        rig.WriteImage (kImagePath, 0x22);
        rig.store.NoteExternalChange (kImagePath, ExternalChangeIntent::ReloadInPlace);

        rig.nowMs += MountedImageState::kQuietPeriodMs;
        rig.store.ApplyPendingReload();

        //  The last writer is the one whose bytes are on the disk, so a stale
        //  intent describes contents that are gone.
        Assert::AreEqual (0, rig.restarts);
        Assert::AreEqual ((size_t) 1, rig.reports.size());
    }



    TEST_METHOD (WithNothingStatedAndNoDeclaredAnswerTheUserIsAskedOnce)
    {
        Rig  rig;



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
        rig.store.ApplyPendingReload();
        rig.store.ApplyPendingReload();

        Assert::AreEqual ((size_t) 1, rig.questions.size());
    }



    TEST_METHOD (AnAnswerOfTakeItUpSwapsTheContents)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size());

        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::ReloadInPlace);

        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->GetPending().seen);
        Assert::IsTrue (rig.store.GetSharedState (kSlot, kDrive)->GetIdentity()
                            .Matches (rig.identities[kImagePath]));
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
        Rig           rig;
        std::wstring  offered;



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

        AssertSucceeded (rig.store.Flush (kSlot, kDrive));

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
        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::KeepHeld);

        Assert::AreEqual ((size_t) 1, rig.PreservedPaths().size(),
                          L"still one copy, not two");
        Assert::IsTrue (offered.find (ChangePrompt::GetFileName (
                            rig.store.GetSourcePath (kSlot, kDrive))) != std::wstring::npos,
                        L"and the bay is on the file the user was promised");
    }



    //  THE ONE SIGNAL EVERY PATH FIRES. The shell turns a bay change into the
    //  drive door, its sounds, the debug event and the controller pointer, in
    //  one handler -- so every path that can move a disk has to announce it,
    //  and each announces the right kind. A pick-up is a Swap (door opens and
    //  closes), a mount is an Insert (door closes), and both a user eject and
    //  a vanished file are an Eject (door opens).
    //  THE RIGS ARE ON THE HEAP, not the stack. Each one carries a
    //  DiskImageStore and two maps, and five of them in one frame come to
    //  about seventeen kilobytes, which trips C6262 -- an error here, since CI
    //  builds with CodeAnalysisTreatWarningsAsErrors. Scoping them separately
    //  does not help: the analyzer measures the frame, not the live set.
    TEST_METHOD (EveryBayChangeIsAnnouncedWithItsKind)
    {
        using BayChange = BayChange;



        //  Mount -> Inserted.
        {
            auto  rig = std::make_unique<Rig>();

            rig->WriteImage (kImagePath, 0x11);
            AssertSucceeded (rig->store.Mount (kSlot, kDrive, kImagePath));

            Assert::AreEqual ((size_t) 1, rig->bayChanges.size());
            Assert::IsTrue (rig->bayChanges[0] == BayChange::Inserted);
        }

        //  Pick-up of an external change -> Swapped.
        {
            auto  rig = std::make_unique<Rig>();

            rig->WriteImage (kImagePath, 0x11);
            AssertSucceeded (rig->store.Mount (kSlot, kDrive, kImagePath));

            rig->WriteImage (kImagePath, 0x22);
            rig->FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

            Assert::AreEqual ((size_t) 2, rig->bayChanges.size());
            Assert::IsTrue (rig->bayChanges[1] == BayChange::Swapped);
        }

        //  User eject -> Ejected.
        {
            auto  rig = std::make_unique<Rig>();

            rig->WriteImage (kImagePath, 0x11);
            AssertSucceeded (rig->store.Mount (kSlot, kDrive, kImagePath));
            rig->store.Eject (kSlot, kDrive);

            Assert::AreEqual ((size_t) 2, rig->bayChanges.size());
            Assert::IsTrue (rig->bayChanges[1] == BayChange::Ejected);
        }

        //  A file that vanished, then the drive emptied -> Ejected.
        {
            auto  rig = std::make_unique<Rig>();

            rig->WriteImage (kImagePath, 0x11);
            AssertSucceeded (rig->store.Mount (kSlot, kDrive, kImagePath));

            rig->files.erase (kImagePath);
            rig->Stamp (kImagePath);
            rig->FireAndSettle (kImagePath);

            rig->store.ResolvePendingChange (kSlot, kDrive, ChangeAction::Discard);

            Assert::IsTrue (!rig->bayChanges.empty());
            Assert::IsTrue (rig->bayChanges.back() == BayChange::Ejected);
        }

        //  Keeping the disk during a conflict moves its file but leaves the
        //  disk in the drive, so it is NOT a bay change -- no door, no sound.
        {
            auto  rig = std::make_unique<Rig>();

            rig->WriteImage (kImagePath, 0x11);
            AssertSucceeded (rig->store.Mount (kSlot, kDrive, kImagePath));

            rig->WriteImage (kImagePath, 0x22);
            rig->FireAndSettle (kImagePath);
            rig->store.ResolvePendingChange (kSlot, kDrive, ChangeAction::KeepHeld);

            //  One Inserted from the mount, and nothing from the keep.
            Assert::AreEqual ((size_t) 1, rig->bayChanges.size());
            Assert::IsTrue (rig->bayChanges[0] == BayChange::Inserted);
        }
    }



    TEST_METHOD (KeepingACleanDiskWritesItOutRatherThanLeavingItInMemory)
    {
        Rig     rig;
        Byte    kept = 0;
        string  where;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        kept = FirstTrackByte (rig.store);

        //  Nothing from the guest, so the bay is clean -- which is exactly the
        //  case that used to lose the disk.
        Assert::IsFalse (rig.store.GetImage (kSlot, kDrive)->IsDirty());

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::KeepHeld);

        Assert::AreEqual ((size_t) 1, rig.PreservedPaths().size(),
                          L"keeping wrote the kept version to a file of its own");

        where = rig.PreservedPaths()[0];

        Assert::AreEqual (where, rig.store.GetSourcePath (kSlot, kDrive),
                          L"and the bay reads and writes that file from here on");

        Assert::AreEqual ((int) 0x22, (int) rig.files[kImagePath][0],
                          L"while the original keeps what the other program wrote");

        //  The disk in the drive did not move. Keeping is a save-as, not a
        //  swap, so a guest mid-read sees nothing at all.
        Assert::AreEqual ((int) kept, (int) FirstTrackByte (rig.store),
                          L"the disk in the drive is untouched");

        //  And it outlives the session, which is the whole point.
        rig.store.Eject (kSlot, kDrive);

        Assert::IsTrue (rig.files.find (where) != rig.files.end(),
                        L"the kept version is still on disk after the eject");
    }



    TEST_METHOD (AnAnswerOfKeepWhatIHaveLetsALaterFlushProceed)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::KeepHeld);

        //  The user saw both and chose the one in memory. Without recording
        //  the file as seen, the re-check before every commit would refuse
        //  forever and strand the guest's work in memory for the session.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertSucceeded (rig.store.Flush (kSlot, kDrive));
    }



    TEST_METHOD (ADirtyImageMeetingAnExternalChangeKeepsBothVersions)
    {
        Rig   rig;
        Byte  guestWrote = 0x7F;



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
        Rig   rig;
        Byte  before = 0;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        before = FirstTrackByte (rig.store);

        rig.refusePreserve = true;

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        //  This is the decision most worth asserting: a preserve that silently
        //  did not happen would break the promise exactly where it matters.
        Assert::AreEqual ((int) before, (int) FirstTrackByte (rig.store),
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
                rig.store.ApplyPendingReload();
            }
        }

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"and a hundred idle ticks later it has still done nothing more");
    }



    TEST_METHOD (TwoConflictsInOneSecondProduceTwoDistinctCopies)
    {
        Rig  rig;



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



    TEST_METHOD (BytesThatCannotBeThisDiskAreRefusedAndTheHeldDiskSurvives)
    {
        Rig   rig;
        Byte  before = 0;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        before = FirstTrackByte (rig.store);

        //  Replaced by something the loader cannot use as this disk.
        rig.files[kImagePath] = vector<Byte> (37, 0xAB);
        rig.Stamp (kImagePath);

        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((int) before, (int) FirstTrackByte (rig.store),
                          L"the machine is running and what it holds is known-good");
        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"what Casso holds may be the only copy left, so the user "
                          L"is offered a copy rather than told nothing");
    }



    TEST_METHOD (ADeletedImageAndAnUnreadableOneAreDistinguished)
    {
        Rig  rig;



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
        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::Discard);

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
        Rig          rig;
        std::string  chosen = "C:\\elsewhere\\Rescued.dsk";
        Byte         held   = 0;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        held = FirstTrackByte (rig.store);

        rig.files.erase (kImagePath);
        rig.Stamp (kImagePath);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size());

        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::PreserveCopy, chosen);

        Assert::IsTrue (rig.files.count (chosen) == 1,
                        L"what the emulator held may be the only copy left");

        Assert::IsTrue (rig.store.IsMounted (kSlot, kDrive),
                        L"the disk is still in the drive");
        Assert::AreEqual (chosen, rig.store.GetSourcePath (kSlot, kDrive),
                          L"and the drive reads and writes the rescued file now");
        Assert::AreEqual ((int) held, (int) FirstTrackByte (rig.store),
                          L"the disk itself is untouched -- only its file moved");
    }



    //  A FILE FOUND GONE WHILE ACTING IS ASKED ABOUT, NOT REPORTED. The user
    //  answers "take up the new version", the re-read finds the file has since
    //  been deleted, and what is left to offer is the same pair the watcher
    //  would have offered. Sent to the notice bar instead, both buttons did
    //  nothing: the bar routes no answers.
    TEST_METHOD (ALostFileFoundWhileActingIsAskedNotReported)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((size_t) 1, rig.questions.size(),
                          L"an unexplained change is a question");

        //  Gone between the question and the answer.
        rig.files.erase (kImagePath);
        rig.Stamp (kImagePath);

        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::ReloadInPlace);

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
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.files.erase (kImagePath);
        rig.Stamp (kImagePath);
        rig.FireAndSettle (kImagePath);

        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::Discard);

        Assert::IsFalse (rig.store.IsMounted (kSlot, kDrive));
        Assert::AreEqual ((size_t) 0, rig.PreservedPaths().size(),
                          L"declining saves nothing, and that is the whole of it");
    }



    TEST_METHOD (ASaveThatFailsDoesNotThenThrowTheOnlyCopyAway)
    {
        Rig          rig;
        std::string  chosen = "C:\\elsewhere\\Rescued.dsk";



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.files.erase (kImagePath);
        rig.Stamp (kImagePath);
        rig.FireAndSettle (kImagePath);

        rig.refusePreserve = true;

        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::PreserveCopy, chosen);

        //  A save the user asked for and did not get must not be followed by
        //  emptying the drive it was the only copy in.
        Assert::IsTrue (rig.store.IsMounted (kSlot, kDrive));
        Assert::IsTrue (rig.files.count (chosen) == 0);
    }



    TEST_METHOD (EjectingWithWritesTheFileHasNotSeenPreservesThem)
    {
        Rig  rig;



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



    TEST_METHOD (AnImageThatHasGoneIsRefusedRatherThanTreatedAsUnchanged)
    {
        Rig   rig;
        Byte  before = 0;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        before = FirstTrackByte (rig.store);

        rig.files.erase (kImagePath);
        rig.identities.erase (kImagePath);

        rig.FireAndSettle (kImagePath);

        Assert::AreEqual ((int) before, (int) FirstTrackByte (rig.store));
        Assert::AreEqual ((size_t) 1, rig.questions.size());
    }



    TEST_METHOD (AChangeArrivingWhileAnEarlierOneIsAppliedIsNotDropped)
    {
        Rig  rig;



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
                rig.store.NoteExternalChange (kImagePath, ExternalChangeIntent::ReloadInPlace);
            }
        });

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

        Assert::IsTrue (rig.store.GetSharedState (kSlot, kDrive)->GetPending().seen,
                        L"the change that arrived mid-apply is still there");

        rig.nowMs += MountedImageState::kQuietPeriodMs;
        rig.store.ApplyPendingReload();

        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->GetPending().seen);
    }



    TEST_METHOD (AStandingReportAbsorbsFurtherChangesRatherThanStacking)
    {
        Rig  rig;



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
        Rig  rig;



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



    TEST_METHOD (ADirectoryThatCannotBeWatchedDegradesRatherThanFailing)
    {
        Rig      rig;
        HRESULT  hrFlush = S_OK;



        rig.watcher.failWatch = true;
        rig.WriteImage (kImagePath, 0x11);

        //  A network share or a synchronizing folder is exactly the case that
        //  produces this, and it must not stop a disk being mounted.
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->IsWatching());

        //  The guarantee is the check made before every write, and it still
        //  holds with no notification at all: the flush finds the change
        //  itself and keeps the version it is about to displace.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        AssertSucceeded (rig.store.Flush (kSlot, kDrive));

        Assert::AreEqual ((size_t) 1, rig.PreservedPaths().size(),
                          L"nothing was lost for want of a notification");
        IGNORE_RETURN_VALUE (hrFlush, S_OK);
    }



    TEST_METHOD (AnIntentForAnImageNobodyHasMountedIsNotAnError)
    {
        Rig  rig;



        //  The writer cannot know whether anything is running, and a build
        //  script must behave the same either way.
        rig.store.NoteExternalChange ("C:\\elsewhere\\Nothing.dsk", ExternalChangeIntent::Restart);
        rig.store.ApplyPendingReload();

        Assert::AreEqual (0, rig.restarts);
        Assert::AreEqual ((size_t) 0, rig.reports.size());
    }



    //
    //  ------------------------------------------------------------------
    //  Two writers.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (TwoWritersDeriveDifferentTemporariesFromOneImage)
    {
        std::string  mine   = DiskImageStore::GetCommitTemporaryPath (kImagePath, 0);
        std::string  again  = DiskImageStore::GetCommitTemporaryPath (kImagePath, 0);
        std::string  second = DiskImageStore::GetCommitTemporaryPath (kImagePath, 1);



        //  Within one process the name is stable, which is what lets a caller
        //  ask where its own commit went.
        Assert::AreEqual (mine, again);

        //  And stepping the attempt gives a different one, which is what walks
        //  past a temporary an earlier writer abandoned.
        Assert::IsTrue (mine != second);

        //  THE NAME IS NOT DERIVED FROM THE IMAGE PATH ALONE, which is the
        //  whole defect: it used to be `<image>.casso-tmp` in every process, so
        //  two emulators holding one image wrote into each other's temporary
        //  and one renamed the other's bytes over the target as its own. The
        //  invocation tag is what makes two processes disagree.
        Assert::IsTrue (mine != std::string (kImagePath) + ".casso-tmp",
                        L"a name a second process would also have chosen");
        Assert::IsTrue (mine.find (kImagePath) == 0,
                        L"and it still sits beside the image, so a leftover is "
                        L"findable by someone reading the folder");
    }



    TEST_METHOD (AWriteThatFailsLeavesTheImageByteForByte)
    {
        fs::path           folder = fs::temp_directory_path() / L"CassoTwoWriterTests";
        fs::path           target = folder / L"work.dsk";
        std::vector<Byte>  before (512, 0x5A);
        std::vector<Byte>  after;
        std::error_code    ec;



        fs::remove_all      (folder, ec);
        fs::create_directories (folder, ec);

        AssertSucceeded (DiskImageStore::WriteFileAtomically (target.string(), before));

        //  EVERY name the commit could take is occupied by a directory, which
        //  no stream can open -- so the write fails after the target already
        //  holds something worth keeping. All of them, because stepping past an
        //  occupied name is exactly what the commit does.
        for (unsigned attempt = 0; attempt < CommitPlan::kMaxAttempts; attempt++)
        {
            fs::create_directories (
                fs::path (DiskImageStore::GetCommitTemporaryPath (target.string(), attempt)), ec);
        }

        {
            std::vector<Byte>  replacement (512, 0xA5);
            HRESULT            hrWrite     = DiskImageStore::WriteFileAtomically (
                                                 target.string(), replacement);

            Assert::IsTrue (FAILED (hrWrite), L"the write could not have succeeded");
        }

        AssertSucceeded (DiskImageStore::ReadFileBytes (target.string(), after));

        //  The guarantee that makes a failed flush survivable: the file on disk
        //  is what it was, not a truncated or half-written version of what was
        //  attempted.
        Assert::IsTrue (after == before,
                        L"a failed write leaves the image exactly as it was");

        fs::remove_all (folder, ec);
    }



    TEST_METHOD (ACommitIsVisibleOnlyAsAWholeVersion)
    {
        fs::path           folder = fs::temp_directory_path() / L"CassoAtomicCommitTests";
        fs::path           target = folder / L"work.dsk";
        std::vector<Byte>  bytes (4096, 0x33);
        std::error_code    ec;



        fs::remove_all      (folder, ec);
        fs::create_directories (folder, ec);

        AssertSucceeded (DiskImageStore::WriteFileAtomically (target.string(), bytes));

        //  The target was never opened directly: had it been, a reader could
        //  have seen it truncated. The evidence a test can hold is that the
        //  bytes arrived whole and the temporary the commit went through is
        //  gone -- so a later refactor that writes in place fails here rather
        //  than silently reintroducing a partly written image.
        Assert::AreEqual ((size_t) 4096, fs::file_size (target, ec));
        Assert::IsFalse (fs::exists (
                             DiskImageStore::GetCommitTemporaryPath (target.string(), 0), ec),
                         L"the commit went through a temporary and consumed it");

        fs::remove_all (folder, ec);
    }



    TEST_METHOD (ATemporaryLeftByAKilledWriterIsSteppedPastRatherThanAdopted)
    {
        fs::path           folder    = fs::temp_directory_path() / L"CassoAbandonedTempTests";
        fs::path           target    = folder / L"work.dsk";
        std::string        abandoned = DiskImageStore::GetCommitTemporaryPath (target.string(), 0);
        std::vector<Byte>  bytes (256, 0x77);
        std::vector<Byte>  readBack;
        std::error_code    ec;



        fs::remove_all      (folder, ec);
        fs::create_directories (folder, ec);

        //  What a writer killed mid-commit leaves behind.
        AssertSucceeded (DiskImageStore::WriteFileAtomically (abandoned, std::vector<Byte> (99, 0x11)));

        AssertSucceeded (DiskImageStore::WriteFileAtomically (target.string(), bytes));

        AssertSucceeded (DiskImageStore::ReadFileBytes (target.string(), readBack));

        //  Adopting it would have appended this image to the remains of
        //  somebody else's, or renamed those remains over the target.
        Assert::IsTrue (readBack == bytes);
        Assert::IsTrue (fs::exists (abandoned, ec),
                        L"and the leftover is left alone rather than consumed");

        fs::remove_all (folder, ec);
    }



    TEST_METHOD (AMountFromBytesIsNeverStaleCheckedIntoARefusedFlush)
    {
        Rig           rig;
        vector<Byte>  bytes (NibblizationLayer::kImageByteSize, 0x55);



        //  Nothing backs it, so there is no identity to compare and nothing to
        //  refuse. Without the recorded-identity gate this flush would fail.
        AssertSucceeded (rig.store.MountFromBytes (kSlot, kDrive, "memory.dsk",
                                                   DiskFormat::Dsk, bytes));

        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertSucceeded (rig.store.Flush (kSlot, kDrive));
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageTests::RigFileIo::Exists
//
//  Defined after Rig so it can see the file map it answers out of.
//
////////////////////////////////////////////////////////////////////////////////

inline bool SharedImageTests::RigFileIo::Exists (const std::string & path)
{
    return rig.files.find (path) != rig.files.end();
}
