#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeImageWatcher.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/NibblizationLayer.h"

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



    //
    //  A store with every platform seam replaced: reads and writes land in
    //  this object, identities come from a number this object controls, and
    //  changes are fired by hand.
    //
    struct Rig
    {
        DiskImageStore                                 store;
        FakeImageWatcher                               watcher;
        std::unordered_map<std::string, vector<Byte>>  files;
        std::unordered_map<std::string, ImageIdentity> identities;
        int64_t                                        nowMs = 0;

        //  What the store reported and did.
        std::vector<ChangePrompt>                      reports;
        std::vector<ChangePrompt>                      questions;
        int                                            restarts = 0;



        Rig()
        {
            store.SetImageWatcher (&watcher);

            store.SetImageReader ([this] (const string & path, vector<Byte> & bytes) -> HRESULT
            {
                auto  found = files.find (path);

                if (found == files.end())
                {
                    return E_FAIL;
                }

                bytes = found->second;

                return S_OK;
            });

            store.SetFlushSink ([this] (const string & path, const vector<Byte> & bytes) -> HRESULT
            {
                files[path] = bytes;
                Stamp (path);

                return S_OK;
            });

            store.SetIdentityReader ([this] (const string & path) -> ImageIdentity
            {
                auto  found = identities.find (path);

                return (found != identities.end()) ? found->second : ImageIdentity();
            });

            store.SetClock ([this] () { return nowMs; });

            store.SetChangeReportSink ([this] (int, int, const ChangePrompt & prompt)
            {
                reports.push_back (prompt);
            });

            store.SetAskSink ([this] (int, int, const ChangePrompt & prompt)
            {
                questions.push_back (prompt);
            });

            store.SetMachineRestartCallback ([this] () { restarts++; });
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
                             PickUpIntent        intent = PickUpIntent::Unstated)
        {
            watcher.Fire (kDirectory, path);

            if (intent != PickUpIntent::Unstated)
            {
                store.NoteExternalChange (path, intent);
            }

            nowMs += MountedImageState::kQuietPeriodMs;
            store.ApplyPendingPickUp();
        }

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

        Assert::IsTrue (rig.store.SharedState (kSlot, kDrive)->Identity().recorded,
                        L"nothing can answer 'has this changed' without one");
        Assert::IsTrue (rig.store.SharedState (kSlot, kDrive)->IsWatching());
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
        Assert::IsFalse (rig.store.SharedState (kSlot, kDrive)->Identity().recorded);
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



    TEST_METHOD (AChangedImageIsPickedUpAndTheIdentityRefreshed)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath, PickUpIntent::TakeUpInPlace);

        Assert::IsFalse (rig.store.SharedState (kSlot, kDrive)->Pending().seen,
                         L"the change was dealt with");
        Assert::IsTrue (rig.store.SharedState (kSlot, kDrive)->Identity()
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



    TEST_METHOD (AFlushOverAnExternalChangeIsRefusedAndTheWritesAreKept)
    {
        Rig      rig;
        HRESULT  hrFlush = S_OK;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  The guest writes, and something else rewrites the file before the
        //  flush lands.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        hrFlush = rig.store.Flush (kSlot, kDrive);

        Assert::IsTrue (FAILED (hrFlush), L"writing now would discard the change");
        Assert::IsTrue (rig.store.GetImage (kSlot, kDrive)->IsDirty(),
                        L"the writes are still in memory and still flushable -- "
                        L"refused is not lost");
    }



    TEST_METHOD (ThreeChangesInsideTheQuietPeriodProduceOnePickUp)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        //  A disk carrying more than one thing is built by more than one
        //  command, and the developer means one build.
        rig.WriteImage (kImagePath, 0x22);
        rig.watcher.Fire (kDirectory, kImagePath);

        rig.nowMs += MountedImageState::kQuietPeriodMs / 2;
        rig.store.ApplyPendingPickUp();
        Assert::AreEqual ((size_t) 0, rig.reports.size(), L"still being written");

        rig.WriteImage (kImagePath, 0x33);
        rig.watcher.Fire (kDirectory, kImagePath);

        rig.nowMs += MountedImageState::kQuietPeriodMs / 2;
        rig.store.ApplyPendingPickUp();
        Assert::AreEqual ((size_t) 0, rig.reports.size(),
                          L"the third change resets the timer -- the period is "
                          L"measured from the LAST change, not the first");

        rig.WriteImage (kImagePath, 0x44);
        rig.FireAndSettle (kImagePath, PickUpIntent::TakeUpInPlace);

        Assert::AreEqual ((size_t) 1, rig.reports.size(), L"one build, one pick-up");
    }



    TEST_METHOD (TwoSpellingsOfOnePathReachTheSameBayAndAnotherFileReachesNone)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  What a watcher reports and what a mount recorded rarely match as
        //  strings.
        rig.store.NoteExternalChange ("c:/WORK/loader.dsk", PickUpIntent::Unstated);
        Assert::IsTrue (rig.store.SharedState (kSlot, kDrive)->Pending().seen);

        rig.store.SharedState (kSlot, kDrive);
        rig.store.Eject (kSlot, kDrive);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.NoteExternalChange ("C:\\work\\Something Else.dsk", PickUpIntent::Unstated);
        Assert::IsFalse (rig.store.SharedState (kSlot, kDrive)->Pending().seen,
                         L"a directory watch reports every file under it");
    }



    TEST_METHOD (AStatedRestartRestartsTheMachineWithoutAsking)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.WriteImage (kImagePath, 0x22);
        rig.store.NoteExternalChange (kImagePath, PickUpIntent::Restart);

        rig.nowMs += MountedImageState::kQuietPeriodMs;
        rig.store.ApplyPendingPickUp();

        Assert::AreEqual (1, rig.restarts);
        Assert::AreEqual ((size_t) 0, rig.questions.size(),
                          L"the writer knew what they changed");
    }



    TEST_METHOD (ALaterStatedIntentReplacesAnEarlierOne)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.NoteExternalChange (kImagePath, PickUpIntent::Restart);
        rig.WriteImage (kImagePath, 0x22);
        rig.store.NoteExternalChange (kImagePath, PickUpIntent::TakeUpInPlace);

        rig.nowMs += MountedImageState::kQuietPeriodMs;
        rig.store.ApplyPendingPickUp();

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
        rig.store.ApplyPendingPickUp();
        rig.store.ApplyPendingPickUp();

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

        rig.store.ResolvePendingChange (kSlot, kDrive, ChangeAction::TakeUpInPlace);

        Assert::IsFalse (rig.store.SharedState (kSlot, kDrive)->Pending().seen);
        Assert::IsTrue (rig.store.SharedState (kSlot, kDrive)->Identity()
                            .Matches (rig.identities[kImagePath]));
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



    TEST_METHOD (ADirtyImageDefersThePickUpAndLosesNothing)
    {
        Rig   rig;
        Byte  before = 0;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        before = FirstTrackByte (rig.store);

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath);

        //  Resolving a two-sided conflict means preserving the version the
        //  user does not keep. Until that exists, picking up here would
        //  discard the guest's work -- so nothing happens, and nothing is
        //  lost: the change stays pending and the writes stay in memory.
        Assert::AreEqual ((size_t) 0, rig.reports.size());
        Assert::AreEqual ((size_t) 0, rig.questions.size());
        Assert::AreEqual ((int) before, (int) FirstTrackByte (rig.store));
        Assert::IsTrue (rig.store.SharedState (kSlot, kDrive)->Pending().seen,
                        L"still pending, so it happens once the conflict can be resolved");
        Assert::IsTrue (rig.store.GetImage (kSlot, kDrive)->IsDirty());
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
                rig.store.NoteExternalChange (kImagePath, PickUpIntent::TakeUpInPlace);
            }
        });

        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath, PickUpIntent::TakeUpInPlace);

        Assert::IsTrue (rig.store.SharedState (kSlot, kDrive)->Pending().seen,
                        L"the change that arrived mid-apply is still there");

        rig.nowMs += MountedImageState::kQuietPeriodMs;
        rig.store.ApplyPendingPickUp();

        Assert::IsFalse (rig.store.SharedState (kSlot, kDrive)->Pending().seen);
    }



    TEST_METHOD (AStandingReportAbsorbsFurtherChangesRatherThanStacking)
    {
        Rig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        rig.WriteImage (kImagePath, 0x22);
        rig.FireAndSettle (kImagePath, PickUpIntent::TakeUpInPlace);

        rig.WriteImage (kImagePath, 0x33);
        rig.FireAndSettle (kImagePath, PickUpIntent::TakeUpInPlace);

        rig.WriteImage (kImagePath, 0x44);
        rig.FireAndSettle (kImagePath, PickUpIntent::TakeUpInPlace);

        //  Three builds before the developer turns back to the emulator are
        //  three pick-ups and one report: three reports about one disk say
        //  nothing three times.
        Assert::AreEqual ((size_t) 1, rig.reports.size());
        Assert::IsTrue (rig.store.SharedState (kSlot, kDrive)->IsReportStanding());

        //  And the contents taken up are the most recent, not those current
        //  when the report first appeared.
        Assert::IsTrue (rig.store.SharedState (kSlot, kDrive)->Identity()
                            .Matches (rig.identities[kImagePath]));

        rig.store.ClearChangeReport (kSlot, kDrive);
        Assert::IsFalse (rig.store.SharedState (kSlot, kDrive)->IsReportStanding());
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
        Assert::IsFalse (rig.store.SharedState (kSlot, kDrive)->IsWatching());

        //  The guarantee is the check made before every write, and it still
        //  holds with no notification at all.
        rig.store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] = 0x7F;
        rig.store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        rig.WriteImage (kImagePath, 0x33);

        hrFlush = rig.store.Flush (kSlot, kDrive);

        Assert::IsTrue (FAILED (hrFlush));
        Assert::IsTrue (rig.store.GetImage (kSlot, kDrive)->IsDirty());
    }



    TEST_METHOD (AnIntentForAnImageNobodyHasMountedIsNotAnError)
    {
        Rig  rig;



        //  The writer cannot know whether anything is running, and a build
        //  script must behave the same either way.
        rig.store.NoteExternalChange ("C:\\elsewhere\\Nothing.dsk", PickUpIntent::Restart);
        rig.store.ApplyPendingPickUp();

        Assert::AreEqual (0, rig.restarts);
        Assert::AreEqual ((size_t) 0, rig.reports.size());
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
