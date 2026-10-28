#include "Pch.h"
#include "DiskStoreTestSupport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSaveTests
//
//  Saves that cannot be written to the disk's file: the recovery copy of an
//  image that does not serialize, the eject that waits for an answer, the
//  rescue at shutdown, the write-protect change that saves first, and the
//  copy beside a file another program changed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskStoreSaveTests), public DiskStoreTestSupport
{
public:

    TEST_METHOD (FlushEntry_UnserializableImage_LeavesOneRecoveryFile)
    {
        DiskImageStore  store;
        FakeDiskFileIo  io;
        DiskImageStore  reloaded;
        string          copyPath = "Q:/casso-no-such-root/image.recovered.woz";
        int             spin     = 0;



        UseFileIo (store, io);
        io.SeedDurable (kSeamImagePath, MakeDsk (0x5A));

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamImagePath));
        CorruptOneAddressField (*store.GetImage (kSlot, kDrive), 3);
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);

        for (spin = 0; spin < 3; spin++)
        {
            AssertFailed (store.FlushAll());
        }

        Assert::AreEqual (size_t (1), CountRecoveryFiles (io), L"three saves write one recovery file");

        store.GetImage (kSlot, kDrive)->WriteBit (10, 0, 1);
        AssertFailed (store.FlushAll());

        Assert::AreEqual (size_t (1), CountRecoveryFiles (io), L"a guest write replaces it");
        AssertSucceeded (reloaded.MountFromBytes (kSlot, kDrive, copyPath, DiskFormat::Woz, io.files[copyPath]));
        Assert::AreEqual (Byte (1), reloaded.GetImage (kSlot, kDrive)->ReadBit (10, 0), L"and the copy has the new bit");
    }


    TEST_METHOD (FlushEntry_RecoveryCopyThatCannotBeWritten_IsReportedOnceAndKeepsTheWrites)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        FakeDiskFileIo            io;
        wstring                   expected;
        int                       spin = 0;



        UseFileIo (store, io);
        io.SeedDurable (kSeamImagePath, MakeDsk (0x5A));
        io.failWritesContaining = ".recovered";
        io.nextWriteError       = HRESULT_FROM_WIN32 (ERROR_DISK_FULL);

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamImagePath));
        CorruptOneAddressField (*store.GetImage (kSlot, kDrive), 3);
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);

        for (spin = 0; spin < 3; spin++)
        {
            AssertFailed (store.FlushAll());
        }

        expected = L"Casso could not save changes to the disk image:\n\nQ:/casso-no-such-root/image.dsk"
                   L"\n\nThe file on disk is unchanged. Its changes cannot be stored as a .dsk image. Casso tried "
                   L"to save them to image.recovered.woz instead, but failed: "
                 + ChangePrompt::DescribeError (HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

        if (!expected.ends_with (L"."))
        {
            expected += L".";
        }

        Assert::AreEqual (1, s_flushNotifyCount, L"the failure is reported once");
        Assert::AreEqual (expected, s_flushNotifyLast, L"in the approved text, with the copy's error");
        Assert::IsTrue (store.GetImage (kSlot, kDrive)->IsDirty(), L"the writes are in the drive");
        Assert::AreEqual (size_t (0), CountRecoveryFiles (io), L"and in no file");
    }


    TEST_METHOD (FlushEntry_RecoveryCopyChangedOutsideCasso_IsNotOverwritten)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        FakeDiskFileIo            io;
        string                    copyPath = "Q:/casso-no-such-root/image.recovered.woz";
        vector<Byte>              outside (100, 0x42);



        UseFileIo (store, io);
        io.SeedDurable (kSeamImagePath, MakeDsk (0x5A));

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamImagePath));
        CorruptOneAddressField (*store.GetImage (kSlot, kDrive), 3);
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertFailed (store.FlushAll());

        //  Another program writes over the copy.
        io.SeedDurable (copyPath, outside);

        store.GetImage (kSlot, kDrive)->WriteBit (10, 0, 1);
        AssertFailed (store.FlushAll());

        Assert::IsTrue (io.files[copyPath] == outside, L"the changed copy is left as it is");
        Assert::IsTrue (io.files.contains ("Q:/casso-no-such-root/image.recovered.1.woz"), L"the session goes to the next name");
        Assert::AreEqual (2, s_flushNotifyCount, L"and the new copy is reported");
    }


    TEST_METHOD (FlushEntry_AfterASuccessfulSave_ANewFailureStartsANewCopy)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        FakeDiskFileIo            io;
        vector<Byte>              intact;



        UseFileIo (store, io);
        io.SeedDurable (kSeamImagePath, MakeDsk (0x5A));

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamImagePath));
        intact = store.GetImage (kSlot, kDrive)->GetTrackBits (3);

        CorruptOneAddressField (*store.GetImage (kSlot, kDrive), 3);
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertFailed (store.FlushAll());

        //  The guest writes the track back whole, so the disk saves.
        store.GetImage (kSlot, kDrive)->GetTrackBitsForWrite (3) = intact;
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertSucceeded (store.FlushAll());

        CorruptOneAddressField (*store.GetImage (kSlot, kDrive), 3);
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertFailed (store.FlushAll());

        Assert::IsTrue (io.files.contains ("Q:/casso-no-such-root/image.recovered.1.woz"),
            L"the copy from before the save is left, and a new one is written");
        Assert::AreEqual (2, s_flushNotifyCount, L"and the new failure is reported");
    }


    TEST_METHOD (Eject_Unwritable_RaisesTheQuestionAndWaits)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        AskRecorder               asks;
        vector<string>            written;



        FailWritesToTheEjectedFile (store, written);
        RecordAsks (store, asks);

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, kEjectPath, DiskFormat::Dsk, MakeDsk (0)));
        WriteOneBit (store);
        store.Eject (kSlot, kDrive);

        Assert::IsTrue   (store.IsMounted (kSlot, kDrive), L"the disk is in the drive while the question stands");
        Assert::AreEqual (size_t (1), asks.prompts.size(), L"one question");
        Assert::AreEqual (wstring (L"Disk not saved"), asks.prompts[0].title);
        Assert::IsTrue   (asks.prompts[0].message.find (L"could not be saved to\n\nC:\\work\\e.dsk") != wstring::npos,
            L"about the disk's file");
        Assert::AreEqual (0, s_flushNotifyCount, L"and no notice besides");
    }


    TEST_METHOD (Eject_Unwritable_SaveAsFinishes)
    {
        DiskImageStore  store;
        AskRecorder     asks;
        vector<string>  written;



        FailWritesToTheEjectedFile (store, written);
        RecordAsks (store, asks);

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, kEjectPath, DiskFormat::Dsk, MakeDsk (0)));
        WriteOneBit (store);
        store.Eject (kSlot, kDrive);

        Assert::IsTrue (store.IsMounted (kSlot, kDrive), L"the disk waits for the answer");

        store.ResolvePendingChange (kSlot, kDrive, ChangeAction::PreserveCopy, kChosenPath);

        Assert::IsTrue  (written == vector<string> { kChosenPath }, L"the changes are saved to the file chosen");
        Assert::IsFalse (store.IsMounted (kSlot, kDrive), L"and the answer finishes the eject");
    }


    TEST_METHOD (Eject_Unwritable_DiscardFinishes)
    {
        DiskImageStore  store;
        AskRecorder     asks;
        vector<string>  written;



        FailWritesToTheEjectedFile (store, written);
        RecordAsks (store, asks);

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, kEjectPath, DiskFormat::Dsk, MakeDsk (0)));
        WriteOneBit (store);
        store.Eject (kSlot, kDrive);

        Assert::IsTrue (store.IsMounted (kSlot, kDrive), L"the disk waits for the answer");

        store.ResolvePendingChange (kSlot, kDrive, ChangeAction::Discard, string());

        Assert::IsFalse  (store.IsMounted (kSlot, kDrive), L"the answer finishes the eject");
        Assert::IsTrue   (written.empty(), L"and nothing is written");
        Assert::AreEqual (size_t (1), asks.prompts.size(), L"with no second question");
    }


    //  With no sink, and with a question the sink could not deliver, the disk
    //  stays in the drive and the notice states it.
    TEST_METHOD (Eject_Unwritable_NoQuestionPossible_KeepsTheDisk)
    {
        for (bool hasSink : { false, true })
        {
            ScopedFlushNotifyCapture  capture;
            DiskImageStore            store;
            AskRecorder               asks;
            vector<string>            written;



            FailWritesToTheEjectedFile (store, written);
            asks.isDelivered = false;

            if (hasSink)
            {
                RecordAsks (store, asks);
            }

            AssertSucceeded (store.MountFromBytes (kSlot, kDrive, kEjectPath, DiskFormat::Dsk, MakeDsk (0)));
            WriteOneBit (store);
            store.Eject (kSlot, kDrive);

            Assert::IsTrue   (store.IsMounted (kSlot, kDrive), L"the disk is in the drive");
            Assert::IsTrue   (store.GetImage (kSlot, kDrive)->IsDirty(), L"with its changes");
            Assert::AreEqual (1, s_flushNotifyCount, L"one notice");
            Assert::IsTrue   (s_flushNotifyLast.find (L"The disk was not ejected, and the drive still has your changes.") != wstring::npos,
                L"which states that the disk was not ejected");
            Assert::IsTrue   (s_flushNotifyLast.find (L"still has them") == wstring::npos,
                L"in the eject's text, not the background save's");
        }
    }


    //  A replay writes nothing, so an eject during one is declined.
    TEST_METHOD (Eject_DuringAReplay_KeepsTheDisk)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        vector<string>            written;



        store.SetFlushSink ([&written] (const string & path, const vector<Byte> &) { written.push_back (path); return S_OK; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, kEjectPath, DiskFormat::Dsk, MakeDsk (0)));
        WriteOneBit (store);
        store.SetReplaying (true);
        store.Eject (kSlot, kDrive);

        Assert::IsTrue   (store.IsMounted (kSlot, kDrive), L"the disk is in the drive");
        Assert::IsTrue   (written.empty(), L"and nothing is written");
        Assert::AreEqual (1, s_flushNotifyCount, L"one notice");
        Assert::IsTrue   (s_flushNotifyLast.find (L"Casso did not eject the disk in drive 1, because the machine was behind live") == 0,
            L"with the reason");
    }


    TEST_METHOD (Eject_RescuedUnserializableDisk_Ejects)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        FakeDiskFileIo            io;
        AskRecorder               asks;



        UseFileIo (store, io);
        RecordAsks (store, asks);
        io.SeedDurable (kSeamImagePath, MakeDsk (0x5A));

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamImagePath));
        CorruptOneAddressField (*store.GetImage (kSlot, kDrive), 3);
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        store.Eject (kSlot, kDrive);

        Assert::IsFalse  (store.IsMounted (kSlot, kDrive), L"the bay is emptied");
        Assert::AreEqual (size_t (1), CountRecoveryFiles (io), L"its changes are in the recovery copy");
        Assert::AreEqual (size_t (0), asks.prompts.size(), L"with no question");
        Assert::AreEqual (1, s_flushNotifyCount, L"and the copy is reported");
    }


    TEST_METHOD (Eject_UnserializableCopyFailed_OffersAWozCopy)
    {
        DiskImageStore  store;
        FakeDiskFileIo  io;
        AskRecorder     asks;
        DiskImageStore  reloaded;
        string          wozPath = "Q:/casso-no-such-root/chosen.woz";



        UseFileIo (store, io);
        RecordAsks (store, asks);
        io.SeedDurable (kSeamImagePath, MakeDsk (0x5A));
        io.failWritesContaining = ".recovered";
        io.nextWriteError       = HRESULT_FROM_WIN32 (ERROR_DISK_FULL);

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamImagePath));
        CorruptOneAddressField (*store.GetImage (kSlot, kDrive), 3);
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        store.GetImage (kSlot, kDrive)->WriteBit (10, 0, 1);
        store.Eject (kSlot, kDrive);

        Assert::IsTrue   (store.IsMounted (kSlot, kDrive), L"the disk waits for the answer");
        Assert::AreEqual (size_t (1), asks.prompts.size(), L"one question");
        Assert::AreEqual (string ("Q:/casso-no-such-root/image.recovered.woz"), asks.prompts[0].suggestedSavePath,
            L"which offers a WOZ copy");
        Assert::IsTrue   (asks.prompts[0].message.find (L"cannot be stored as a .dsk image") != wstring::npos,
            L"because the disk's format cannot store the changes");

        store.ResolvePendingChange (kSlot, kDrive, ChangeAction::PreserveCopy, "Q:/casso-no-such-root/chosen.dsk");

        Assert::IsFalse (store.IsMounted (kSlot, kDrive), L"the answer finishes the eject");
        Assert::IsFalse (io.files.contains ("Q:/casso-no-such-root/chosen.dsk"), L"no .dsk is written");
        Assert::IsTrue  (io.files.contains (wozPath), L"the changes are in a WOZ copy at the chosen name");
        AssertSucceeded (reloaded.MountFromBytes (kSlot, kDrive, wozPath, DiskFormat::Woz, io.files[wozPath]));
        Assert::AreEqual (Byte (1), reloaded.GetImage (kSlot, kDrive)->ReadBit (10, 0), L"with the guest's write");
    }


    //  The last save of the process offers the rescue for a disk whose file
    //  could not be written.
    TEST_METHOD (FlushAllForShutdown_Unwritable_OffersTheRescue)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        vector<string>            written;
        string                    offered;



        FailWritesToTheEjectedFile (store, written);
        store.SetRescueSink ([&offered] (const string & imagePath, string & outPath)
        {
            offered = imagePath;
            outPath = kChosenPath;
            return true;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, kEjectPath, DiskFormat::Dsk, MakeDsk (0)));
        WriteOneBit (store);
        AssertFailed (store.FlushAllForShutdown(), L"the disk's file is not written");

        Assert::AreEqual (string (kEjectPath), offered, L"the rescue is offered for the disk's file");
        Assert::IsTrue   (written == vector<string> { kChosenPath }, L"and the changes are saved where it chose");
        Assert::AreEqual (0, s_flushNotifyCount, L"with no notice");
    }


    TEST_METHOD (FlushAllForShutdown_RescueDeclined_StatesTheChangesCannotBeKept)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        vector<string>            written;
        int                       rescues = 0;



        FailWritesToTheEjectedFile (store, written);
        store.SetRescueSink ([&rescues] (const string &, string &) { rescues++; return false; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, kEjectPath, DiskFormat::Dsk, MakeDsk (0)));
        WriteOneBit (store);
        AssertFailed (store.FlushAllForShutdown(), L"the disk's file is not written");

        Assert::AreEqual (1, rescues, L"the rescue is offered");
        Assert::AreEqual (1, s_flushNotifyCount, L"and declining it raises one notice");
        Assert::IsTrue   (s_flushNotifyLast.find (L"Casso is closing, so these changes cannot be kept.") != wstring::npos,
            L"which states that the changes cannot be kept");
        Assert::IsTrue   (s_flushNotifyLast.find (L"still has them") == wstring::npos);
    }


    TEST_METHOD (FlushAllForShutdown_UnserializableCopyFailed_RescuesAWozCopy)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        FakeDiskFileIo            io;
        string                    offered;



        UseFileIo (store, io);
        io.SeedDurable (kSeamImagePath, MakeDsk (0x5A));
        io.failWritesContaining = ".recovered";
        io.nextWriteError       = HRESULT_FROM_WIN32 (ERROR_DISK_FULL);

        store.SetRescueSink ([&offered] (const string & imagePath, string & outPath)
        {
            offered = imagePath;
            outPath = "Q:/casso-no-such-root/kept.dsk";
            return true;
        });

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamImagePath));
        CorruptOneAddressField (*store.GetImage (kSlot, kDrive), 3);
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        AssertFailed (store.FlushAllForShutdown(), L"the disk's file is not written");

        Assert::AreEqual (string ("Q:/casso-no-such-root/image.recovered.woz"), offered, L"the rescue offers a WOZ name");
        Assert::IsTrue   (io.files.contains ("Q:/casso-no-such-root/kept.woz"), L"and the changes are in a WOZ copy");
        Assert::AreEqual (0, s_flushNotifyCount, L"with no notice");
    }


    //  Protecting saves the disk first, under the flush hold too: an
    //  explicit save writes under it.
    TEST_METHOD (UserWriteProtect_SavesFirst_EvenWhileHeld)
    {
        DiskImageStore  store;
        vector<string>  written;



        store.SetFlushSink ([&written] (const string & path, const vector<Byte> &) { written.push_back (path); return S_OK; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, kEjectPath, DiskFormat::Dsk, MakeDsk (0)));
        WriteOneBit (store);
        store.SetFlushHold (true);

        AssertSucceeded (store.SetUserWriteProtect (kSlot, kDrive, true));

        Assert::IsTrue (written == vector<string> { kEjectPath }, L"the disk is saved");
        Assert::IsTrue (store.GetImage (kSlot, kDrive)->IsUserWriteProtected(), L"and then protected");
    }


    TEST_METHOD (UserWriteProtect_FlushFailure_LeavesTheDiskWritable)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        vector<string>            written;



        FailWritesToTheEjectedFile (store, written);

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, kEjectPath, DiskFormat::Dsk, MakeDsk (0)));
        WriteOneBit (store);

        AssertFailed (store.SetUserWriteProtect (kSlot, kDrive, true), L"the save fails");

        Assert::IsFalse  (store.GetImage (kSlot, kDrive)->IsUserWriteProtected(), L"so the disk is writable");
        Assert::IsTrue   (store.GetImage (kSlot, kDrive)->IsDirty(), L"with its writes");
        Assert::AreEqual (1, s_flushNotifyCount, L"and the failure is reported");
    }


    TEST_METHOD (UserWriteProtect_UnprotectingACleanDisk_SavesNothing)
    {
        DiskImageStore  store;
        vector<string>  written;



        store.SetFlushSink ([&written] (const string & path, const vector<Byte> &) { written.push_back (path); return S_OK; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, kEjectPath, DiskFormat::Dsk, MakeDsk (0)));
        store.SetUserWriteProtectFlag (kSlot, kDrive, true);

        AssertSucceeded (store.SetUserWriteProtect (kSlot, kDrive, false));

        Assert::IsTrue  (written.empty(), L"nothing is saved");
        Assert::IsFalse (store.GetImage (kSlot, kDrive)->IsUserWriteProtected(), L"and the disk is writable");
    }


    TEST_METHOD (FlushEntry_ConflictCopy_ThroughFileIo_IsDurable)
    {
        DiskImageStore  store;
        FakeDiskFileIo  io;
        vector<Byte>    original = MakeDsk (0x24);
        string          copyPath;
        vector<Byte>    copy;



        UseFileIo (store, io);
        io.SeedDurable (kSeamImagePath, original);

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamImagePath));
        WriteOneBit (store);

        //  The image changes outside Casso, so the save goes to a copy beside it.
        io.mutateStampOnNextStat = true;
        AssertSucceeded (store.Flush (kSlot, kDrive));

        for (const auto & [path, bytes] : io.files)
        {
            if (path != kSeamImagePath)
            {
                copyPath = path;
                copy     = bytes;
            }
        }

        io.SimulatePowerLoss();

        Assert::IsFalse (copyPath.empty(), L"the guest's writes go to a copy");
        Assert::IsTrue (io.files[kSeamImagePath] == original, L"the changed image is left alone");
        Assert::IsTrue (io.files[copyPath] == copy, L"and the copy survives the power cut whole");
        Assert::IsTrue (io.operations == vector<string> { "Write", "Flush", "Replace" },
            L"the copy is flushed before the rename");
    }


    TEST_METHOD (FlushEntry_RecoveryImage_KeepsTheTrackThatCausedTheRefusal)
    {
        // The whole point of choosing WOZ over the denibblized sector buffer:
        // that buffer holds zeros exactly where the unreadable track was, so it
        // would discard the very content the refusal existed to protect.
        DiskImageStore  store;
        DiskImageStore  reloaded;
        vector<Byte>    recoveryBytes;
        vector<Byte>    originalTrackBits;
        size_t          originalBitCount = 0;
        const int       kTrack           = 3;

        store.SetFlushSink ([&](const string & path, const vector<Byte> & bytes)
        {
            if (path.ends_with (".recovered.woz"))
            {
                recoveryBytes = bytes;
            }

            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Session.dsk",
                                               DiskFormat::Dsk, MakeDsk (0x33)));

        CorruptOneAddressField (*store.GetImage (kSlot, kDrive), kTrack);
        originalTrackBits = store.GetImage (kSlot, kDrive)->GetTrackBits (kTrack);
        originalBitCount  = store.GetImage (kSlot, kDrive)->GetTrackBitCount (kTrack);
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);

        store.Eject (kSlot, kDrive);

        Assert::IsTrue (recoveryBytes.size() > 0, L"a recovery image must have been produced");

        AssertSucceeded (reloaded.MountFromBytes (kSlot, kDrive, "C:\\disks\\Session.recovered.woz",
                                                  DiskFormat::Woz, recoveryBytes));

        {
            DiskImage *  after     = reloaded.GetImage (kSlot, kDrive);
            size_t       bit       = 0;
            bool         identical = after->GetTrackBitCount (kTrack) == originalBitCount;

            Assert::IsTrue (originalBitCount > 0, L"the reference track must carry bits");
            Assert::IsTrue (identical, L"the recovery image must keep the track's bit length");

            // Compare the DATA, not the container: WOZ pads packed bytes out to
            // a block boundary, so a raw byte-vector compare would fail on
            // padding while the recorded bits are identical.
            for (bit = 0; identical && bit < originalBitCount; bit++)
            {
                Byte  expected = (Byte) ((originalTrackBits[bit / 8] >> (7 - (bit % 8))) & 1);

                identical = after->ReadBit (kTrack, bit) == expected;
            }

            Assert::IsTrue (identical,
                L"the damaged track's own bits must survive into the recovery image");
        }
    }


    TEST_METHOD (FlushEntry_UnserializableImage_WritesLosslessRecoveryBesideOriginal)
    {
        // The refusal protects the file on disk; the recovery image is what
        // stops it from stranding the session. Both must happen, and the
        // original must NOT be written.
        DiskImageStore                       store;
        std::vector<std::pair<string, int>>  writes;
        string                               recoveryPath;
        vector<Byte>                         recoveryBytes;
        bool                                 wroteOriginal = false;

        store.SetFlushSink ([&](const string & path, const vector<Byte> & bytes)
        {
            writes.push_back ({ path, (int) bytes.size() });

            if (path.ends_with (".recovered.woz"))
            {
                recoveryPath  = path;
                recoveryBytes = bytes;
            }
            else
            {
                wroteOriginal = true;
            }

            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Session.dsk",
                                               DiskFormat::Dsk, MakeDsk (0x5A)));

        CorruptOneAddressField (*store.GetImage (kSlot, kDrive), 3);
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);

        store.Eject (kSlot, kDrive);

        Assert::IsFalse (wroteOriginal,
            L"the original must not be overwritten with a buffer that lost a track");
        Assert::AreEqual (string ("C:\\disks\\Session.recovered.woz"), recoveryPath,
            L"the recovery image sits beside the original under its own name");
        Assert::IsTrue (recoveryBytes.size() > 0, L"the recovery image must have content");
    }


    TEST_METHOD (FlushEntry_UnserializableImage_IsPreservedOnceUntilTheGuestWritesAgain)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        Disk2Controller           ctrl (6);
        int                       recoveryWrites = 0;
        string                    recoveryPath;
        bool                      wroteOriginal  = false;



        store.SetFlushSink ([&] (const string & path, const vector<Byte> &)
        {
            if (path.ends_with (".recovered.woz"))
            {
                recoveryWrites++;
                recoveryPath = path;
            }
            else
            {
                wroteOriginal = true;
            }

            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Session.dsk",
                                               DiskFormat::Dsk, MakeDsk (0x5A)));
        CorruptOneAddressField (*store.GetImage (kSlot, kDrive), 3);
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        ctrl.SetMotorOffFlushCallback ([&] () { store.FlushAllUnlessHeld(); });

        SpinDown (ctrl, 3);

        Assert::AreEqual (1, recoveryWrites, L"one recovery copy for three spindowns");
        Assert::AreEqual (1, s_flushNotifyCount, L"and one notice");
        Assert::IsTrue (store.GetImage (kSlot, kDrive)->IsDirty(), L"the original lacks the writes, so the disk is dirty");
        Assert::IsFalse (wroteOriginal, L"the original is not written");

        store.GetImage (kSlot, kDrive)->WriteBit (10, 0, 1);
        SpinDown (ctrl, 1);

        Assert::AreEqual (2, recoveryWrites, L"a guest write refreshes the copy");
        Assert::AreEqual (string ("C:\\disks\\Session.recovered.woz"), recoveryPath, L"at the same path");
        Assert::AreEqual (1, s_flushNotifyCount, L"with no second notice");
    }


    TEST_METHOD (FlushEntry_LockedImage_ReportsOnceAcrossSpindowns)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        Disk2Controller           ctrl (6);
        int                       writes = 0;



        store.SetFlushSink ([&] (const string &, const vector<Byte> &)
        {
            writes++;
            return HRESULT_FROM_WIN32 (ERROR_SHARING_VIOLATION);
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Locked.dsk",
                                               DiskFormat::Dsk, MakeDsk (0x24)));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);
        ctrl.SetMotorOffFlushCallback ([&] () { store.FlushAllUnlessHeld(); });

        SpinDown (ctrl, 3);

        Assert::AreEqual (3, writes, L"every spindown saves again");
        Assert::AreEqual (1, s_flushNotifyCount, L"and the failure is reported once");
        Assert::IsTrue (store.GetImage (kSlot, kDrive)->IsDirty(), L"the writes are unsaved");
    }


    TEST_METHOD (Flush_RepeatedFailures_ReportEveryTime)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        int                       attempt = 0;



        store.SetFlushSink ([] (const string &, const vector<Byte> &)
        {
            return HRESULT_FROM_WIN32 (ERROR_SHARING_VIOLATION);
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Locked.dsk",
                                               DiskFormat::Dsk, MakeDsk (0x24)));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);

        for (attempt = 0; attempt < 3; attempt++)
        {
            AssertFailed (store.Flush (kSlot, kDrive));
        }

        Assert::AreEqual (3, s_flushNotifyCount, L"a save the user asked for reports every failure");
    }
};
