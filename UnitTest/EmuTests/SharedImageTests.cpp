#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/Store/ExternalChangeMonitor.h"
#include "Devices/Disk/Store/ExternalChangeActions.h"
#include "Devices/Disk/Store/DiskFlusher.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "SharedImageRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageTests
//
//  A disk image changed by something else while the emulator has it open:
//  the watch and identity taken at mount, one file in two drives, and bay
//  changes.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (SharedImageTests)
{
public:

    static constexpr int           kSlot      = SharedImageRig::kSlot;
    static constexpr int           kDrive     = SharedImageRig::kDrive;
    static constexpr const char *  kDirectory = SharedImageRig::kDirectory;
    static constexpr const char *  kImagePath = SharedImageRig::kImagePath;

    TEST_METHOD (MountRecordsAnIdentityAndTakesUpAWatch)
    {
        SharedImageRig  rig;



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



    //  A WOZ whose write-protect flag the store can patch: ordinary bytes,
    //  nothing damaged, mounted from a path so the bay records an identity.
    static vector<Byte> MakeWoz()
    {
        DiskImage     src;
        vector<Byte>  sectors (NibbleFormat::kImageByteSize, 0);
        vector<Byte>  woz;

        AssertSucceeded (Nibblizer::NibblizeDsk (sectors, src));
        src.SetSourceFormat (DiskFormat::Woz);
        AssertSucceeded (WozLoader::Serialize (src, woz));

        return woz;
    }



    TEST_METHOD (WriteProtectingAWozIsNotReportedAsAChangeFromOutside)
    {
        //  The store wrote this file itself, at the user's request, so the
        //  identity it watches has to move with it. It did not, and the next
        //  poll found bytes that disagreed with the mount-time identity and
        //  reported the disk as changed outside Casso -- over a change made
        //  from the Disk menu two seconds earlier.
        SharedImageRig          rig;
        std::string  wozPath = "C:\\work\\Loader.woz";



        rig.files[wozPath] = MakeWoz();
        rig.Stamp (wozPath);

        AssertSucceeded (rig.store.Mount (kSlot, kDrive, wozPath));
        AssertSucceeded (rig.store.GetFlusher().SetImageWriteProtect (kSlot, kDrive, true));

        rig.FireAndSettle (wozPath);

        Assert::AreEqual ((size_t) 0, rig.reports.size(),
            L"the user's own write-protect must not come back as somebody else's edit");
        Assert::AreEqual ((size_t) 0, rig.questions.size(),
            L"and it must not raise the question either");
    }



    TEST_METHOD (EjectDropsTheWatchAndTheIdentity)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.Eject (kSlot, kDrive);

        Assert::AreEqual ((size_t) 0, rig.watcher.watched.size());
        Assert::AreEqual ((size_t) 1, rig.watcher.unwatched.size());
        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->GetIdentity().recorded);
    }



    //  A failed remount empties the bay, so it drops the watch and the
    //  identity as an eject does.
    TEST_METHOD (AFailedRemountDropsTheWatchAndTheIdentity)
    {
        SharedImageRig                rig;
        std::vector<Byte>  junk (0x10, 0);
        HRESULT            hrMount = S_OK;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        hrMount = rig.store.MountFromBytes (kSlot, kDrive, "junk.dsk", DiskFormat::Dsk, junk);

        Assert::IsTrue  (FAILED (hrMount));
        Assert::AreEqual ((size_t) 0, rig.watcher.watched.size());
        Assert::AreEqual ((size_t) 1, rig.watcher.unwatched.size());
        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->GetIdentity().recorded);
    }



    //  A disk from another folder mounted over the bay's disk moves the watch
    //  to the new folder.
    TEST_METHOD (ARemountFromAnotherFolderMovesTheWatch)
    {
        SharedImageRig          rig;
        std::string  other = "C:\\other\\Data.dsk";



        rig.WriteImage (kImagePath, 0x11);
        rig.WriteImage (other,      0x22);

        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, other));

        Assert::AreEqual ((size_t) 1, rig.watcher.watched.size());
        Assert::IsTrue (rig.watcher.watched[0] == std::string ("C:\\other"), L"only the new folder is watched");
        Assert::AreEqual ((size_t) 1, rig.watcher.unwatched.size());
        Assert::IsTrue (rig.watcher.unwatched[0] == std::string (kDirectory), L"the old folder's watch is dropped");
    }



    TEST_METHOD (AWatchIsKeptWhileAnotherBayStillNeedsItsDirectory)
    {
        SharedImageRig          rig;
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

    //  ONE FILE, ONE DRIVE. Two bays on one file each hold their own copy of
    //  the disk, and a flush writes the whole image -- so from the guest's
    //  first write each drive overwrites whatever the other saved. One
    //  external change raises the conflict twice and puts two dialogs up.
    TEST_METHOD (AFileAlreadyInADriveIsRefusedByTheOther)
    {
        SharedImageRig  rig;
        MountDiagnosis  diagnosis;
        HRESULT         hr        = S_OK;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        hr = rig.store.Mount (kSlot, kDrive + 1, kImagePath, diagnosis);

        Assert::IsTrue (FAILED (hr), L"the second drive refuses it");
        Assert::IsTrue (diagnosis.failure == MountFailure::AlreadyMounted);
        Assert::AreEqual (kDrive, diagnosis.occupiedDrive,
                          L"and specifies which drive has it");
        Assert::IsFalse (rig.store.IsMounted (kSlot, kDrive + 1));

        //  The message specifies which drive has it rather than leaving the
        //  reader to look in both.
        Assert::IsTrue (diagnosis.Describe().find ("drive 1") != std::string::npos);
    }


    //  A //c with nothing on its disk port has no drive 2, so a disk put
    //  there would never be read.
    TEST_METHOD (ABayWithNoDriveFailsTheMount)
    {
        SharedImageRig  rig;
        MountDiagnosis  diagnosis;
        HRESULT         hr        = S_OK;



        rig.WriteImage (kImagePath, 0x11);
        rig.store.SetDriveAttached (kSlot, kDrive, false);

        hr = rig.store.Mount (kSlot, kDrive, kImagePath, diagnosis);

        Assert::IsTrue  (FAILED (hr));
        Assert::IsTrue  (diagnosis.failure == MountFailure::NoDrive);
        Assert::IsFalse (rig.store.IsMounted (kSlot, kDrive));
        Assert::AreEqual (std::wstring (L"This machine has not configured Drive " + std::to_wstring (kDrive + 1)),
                          diagnosis.FormatSentence (L"This file"));

        rig.store.SetDriveAttached (kSlot, kDrive, true);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
    }



    //  AND PUTTING THE SAME FILE BACK INTO THE SAME DRIVE IS NOT THAT. A
    //  machine switch and a reload both re-mount, so the bay being mounted
    //  into is the one bay the check does not look at.
    TEST_METHOD (AReMountOfTheSameFileIntoItsOwnDriveStillWorks)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath),
                         L"the same file back into the drive that has it");

        Assert::IsTrue (rig.store.IsMounted (kSlot, kDrive));
    }



    //  AND THE DRIVE IT LEFT IS FREE AGAIN.
    TEST_METHOD (EjectingAFileLetsTheOtherDriveTakeIt)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.Eject (kSlot, kDrive);

        AssertSucceeded (rig.store.Mount (kSlot, kDrive + 1, kImagePath));

        Assert::IsTrue (rig.store.IsMounted (kSlot, kDrive + 1));
    }



    TEST_METHOD (TwoSpellingsOfOnePathReachTheSameBayAndAnotherFileReachesNone)
    {
        SharedImageRig  rig;



        rig.WriteImage (kImagePath, 0x11);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        //  What a watcher reports and what a mount recorded rarely match as
        //  strings.
        rig.store.GetChangeMonitor().NoteExternalChange ("c:/WORK/loader.dsk", ExternalChangeIntent::Unstated);
        Assert::IsTrue (rig.store.GetSharedState (kSlot, kDrive)->GetPending().seen);

        rig.store.GetSharedState (kSlot, kDrive);
        rig.store.Eject (kSlot, kDrive);
        AssertSucceeded (rig.store.Mount (kSlot, kDrive, kImagePath));

        rig.store.GetChangeMonitor().NoteExternalChange ("C:\\work\\Something Else.dsk", ExternalChangeIntent::Unstated);
        Assert::IsFalse (rig.store.GetSharedState (kSlot, kDrive)->GetPending().seen,
                         L"a directory watch reports every file under it");
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
            auto  rig = std::make_unique<SharedImageRig>();

            rig->WriteImage (kImagePath, 0x11);
            AssertSucceeded (rig->store.Mount (kSlot, kDrive, kImagePath));

            Assert::AreEqual ((size_t) 1, rig->bayChanges.size());
            Assert::IsTrue (rig->bayChanges[0] == BayChange::Inserted);
        }

        //  Pick-up of an external change -> Swapped.
        {
            auto  rig = std::make_unique<SharedImageRig>();

            rig->WriteImage (kImagePath, 0x11);
            AssertSucceeded (rig->store.Mount (kSlot, kDrive, kImagePath));

            rig->WriteImage (kImagePath, 0x22);
            rig->FireAndSettle (kImagePath, ExternalChangeIntent::ReloadInPlace);

            Assert::AreEqual ((size_t) 2, rig->bayChanges.size());
            Assert::IsTrue (rig->bayChanges[1] == BayChange::Swapped);
        }

        //  A restored machine state -> Restored, with a new media id and the
        //  same bytes as inserted.
        {
            auto          rig      = std::make_unique<SharedImageRig>();
            uint64_t      before   = 0;
            vector<Byte>  inserted;

            rig->WriteImage (kImagePath, 0x11);
            AssertSucceeded (rig->store.Mount (kSlot, kDrive, kImagePath));
            before   = rig->store.GetMediaId (kSlot, kDrive);
            inserted = rig->store.GetImage (kSlot, kDrive)->GetSourceBytes();

            rig->store.NoteStateRestored (kSlot, kDrive);

            Assert::AreEqual ((size_t) 2, rig->bayChanges.size());
            Assert::IsTrue   (rig->bayChanges[1] == BayChange::Restored);
            Assert::AreNotEqual (before, rig->store.GetMediaId (kSlot, kDrive), L"a viewer analyzes it again");
            Assert::IsTrue   (inserted == rig->store.GetImage (kSlot, kDrive)->GetSourceBytes(), L"As inserted is the disk as first inserted");
        }

        //  User eject -> Ejected.
        {
            auto  rig = std::make_unique<SharedImageRig>();

            rig->WriteImage (kImagePath, 0x11);
            AssertSucceeded (rig->store.Mount (kSlot, kDrive, kImagePath));
            rig->store.Eject (kSlot, kDrive);

            Assert::AreEqual ((size_t) 2, rig->bayChanges.size());
            Assert::IsTrue (rig->bayChanges[1] == BayChange::Ejected);
        }

        //  A file that vanished, then the drive emptied -> Ejected.
        {
            auto  rig = std::make_unique<SharedImageRig>();

            rig->WriteImage (kImagePath, 0x11);
            AssertSucceeded (rig->store.Mount (kSlot, kDrive, kImagePath));

            rig->files.erase (kImagePath);
            rig->Stamp (kImagePath);
            rig->FireAndSettle (kImagePath);

            rig->store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::Discard);

            Assert::IsTrue (!rig->bayChanges.empty());
            Assert::IsTrue (rig->bayChanges.back() == BayChange::Ejected);
        }

        //  Keeping the disk during a conflict moves its file but leaves the
        //  disk in the drive, so it is NOT a bay change -- no door, no sound.
        {
            auto  rig = std::make_unique<SharedImageRig>();

            rig->WriteImage (kImagePath, 0x11);
            AssertSucceeded (rig->store.Mount (kSlot, kDrive, kImagePath));

            rig->WriteImage (kImagePath, 0x22);
            rig->FireAndSettle (kImagePath);
            rig->store.GetChangeActions().ResolvePendingChange (kSlot, kDrive, ChangeAction::KeepHeld);

            //  One Inserted from the mount, and nothing from the keep.
            Assert::AreEqual ((size_t) 1, rig->bayChanges.size());
            Assert::IsTrue (rig->bayChanges[0] == BayChange::Inserted);
        }

        //  A mount from bytes -> Inserted.
        {
            auto  rig = std::make_unique<SharedImageRig>();

            rig->WriteImage (kImagePath, 0x11);
            AssertSucceeded (rig->store.MountFromBytes (kSlot, kDrive, kImagePath, DiskFormat::Dsk, rig->files[kImagePath]));

            Assert::AreEqual ((size_t) 1, rig->bayChanges.size());
            Assert::IsTrue (rig->bayChanges[0] == BayChange::Inserted);
        }

        //  A failed remount frees the mounted disk and empties the bay ->
        //  Ejected.
        {
            auto               rig      = std::make_unique<SharedImageRig>();
            std::vector<Byte>  junk (0x10, 0);
            HRESULT            hrMount  = S_OK;

            rig->WriteImage (kImagePath, 0x11);
            AssertSucceeded (rig->store.Mount (kSlot, kDrive, kImagePath));

            hrMount = rig->store.MountFromBytes (kSlot, kDrive, "junk.dsk", DiskFormat::Dsk, junk);

            Assert::IsTrue (FAILED (hrMount));
            Assert::AreEqual ((size_t) 2, rig->bayChanges.size());
            Assert::IsTrue (rig->bayChanges[1] == BayChange::Ejected);
            Assert::IsFalse (rig->store.IsMounted (kSlot, kDrive));
        }
    }



    TEST_METHOD (ADirectoryThatCannotBeWatchedDegradesRatherThanFailing)
    {
        SharedImageRig  rig;
        HRESULT         hrFlush = S_OK;



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

        AssertSucceeded (rig.store.GetFlusher().Flush (kSlot, kDrive));

        Assert::AreEqual ((size_t) 1, rig.PreservedPaths().size(),
                          L"nothing was lost for want of a notification");
        IGNORE_RETURN_VALUE (hrFlush, S_OK);
    }



    TEST_METHOD (AnIntentForAnImageNobodyHasMountedIsNotAnError)
    {
        SharedImageRig  rig;



        //  The writer cannot know whether anything is running, and a build
        //  script must behave the same either way.
        rig.store.GetChangeMonitor().NoteExternalChange ("C:\\elsewhere\\Nothing.dsk", ExternalChangeIntent::Restart);
        rig.store.GetChangeMonitor().ApplyPendingReload();

        Assert::AreEqual (0, rig.restarts);
        Assert::AreEqual ((size_t) 0, rig.reports.size());
    }
};
