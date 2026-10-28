#include "Pch.h"
#include "DiskStoreTestSupport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreMountTests
//
//  A mount over a bay and why a mount fails: the save before a replace, the
//  load diagnosis, and the message for each reason.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskStoreMountTests), public DiskStoreTestSupport
{
public:

    TEST_METHOD (Mount_UnloadableOverAMountedDisk_LeavesThatDiskInTheDrive)
    {
        DiskImageStore   store;
        Disk2Controller  controller (6);
        DiskImage      * image = nullptr;
        HRESULT          hr    = S_OK;



        store.SetFlushSink ([] (const string &, const vector<Byte> &) { return S_OK; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Kept.dsk", DiskFormat::Dsk, MakeDsk (0x11)));
        image = store.GetImage (kSlot, kDrive);
        controller.SetExternalDisk (kDrive, image);

        hr = store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Short.dsk", DiskFormat::Dsk, vector<Byte> (4096, 0));

        Assert::IsTrue (FAILED (hr), L"a file that does not load is not mounted");
        Assert::IsTrue (store.GetImage (kSlot, kDrive) == image, L"the disk that was there is in the drive");
        Assert::AreEqual (string ("C:\\disks\\Kept.dsk"), store.GetSourcePath (kSlot, kDrive));

        controller.SoftReset();

        Assert::IsTrue (controller.HasExternalDisk (kDrive), L"and the drive is on it");
    }


    TEST_METHOD (MountFromBytes_UnloadableOverADirtyDisk_WritesNothing)
    {
        DiskImageStore  store;
        int             writes = 0;



        store.SetFlushSink ([&writes] (const string &, const vector<Byte> &) { writes++; return S_OK; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Kept.dsk", DiskFormat::Dsk, MakeDsk (0x11)));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);

        AssertFailed (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Short.dsk", DiskFormat::Dsk, vector<Byte> (4096, 0)));

        Assert::AreEqual (0, writes, L"nothing is saved for a mount that does not happen");
        Assert::IsTrue (store.GetImage (kSlot, kDrive)->IsDirty(), L"and the writes are in the drive");
    }


    TEST_METHOD (MountFromBytes_OtherFileOverAnUnwritableDisk_KeepsTheDisk)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        MountDiagnosis            diagnosis;
        DiskImage               * image = nullptr;
        HRESULT                   hr    = S_OK;



        store.SetFlushSink ([] (const string &, const vector<Byte> &) { return HRESULT_FROM_WIN32 (ERROR_DISK_FULL); });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Kept.dsk", DiskFormat::Dsk, MakeDsk (0x11)));
        image = store.GetImage (kSlot, kDrive);
        image->WriteBit (0, 0, 1);

        hr = store.MountFromBytes (kSlot, kDrive, "C:\\disks\\New.dsk", DiskFormat::Dsk, MakeDsk (0x22), diagnosis);

        Assert::IsTrue (FAILED (hr), L"the insert is declined");
        Assert::IsTrue (diagnosis.failure == MountFailure::UnsavedWrites);
        Assert::AreEqual ((int) HRESULT_FROM_WIN32 (ERROR_DISK_FULL), (int) diagnosis.saveError, L"with the save's error");
        Assert::AreEqual (string ("C:\\disks\\Kept.dsk"), diagnosis.keptPath);
        Assert::IsFalse (diagnosis.isSameFile);
        Assert::IsTrue (store.GetImage (kSlot, kDrive) == image, L"the disk is in the drive");
        Assert::IsTrue (image->IsDirty(), L"with its writes");
        Assert::AreEqual (0, s_flushNotifyCount, L"the mount's decline is the report");
    }


    TEST_METHOD (MountFromBytes_OtherFileOverASavableDisk_WritesOnceAndReplaces)
    {
        DiskImageStore  store;
        vector<string>  written;



        store.SetFlushSink ([&written] (const string & path, const vector<Byte> &) { written.push_back (path); return S_OK; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Kept.dsk", DiskFormat::Dsk, MakeDsk (0x11)));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\New.dsk", DiskFormat::Dsk, MakeDsk (0x22)));

        Assert::AreEqual ((size_t) 1, written.size(), L"one save");
        Assert::AreEqual (string ("C:\\disks\\Kept.dsk"), written[0], L"of the disk that was there");
        Assert::AreEqual (string ("C:\\disks\\New.dsk"), store.GetSourcePath (kSlot, kDrive), L"and the new disk is in the drive");
    }


    TEST_METHOD (Mount_OtherFileOverARescuedUnserializableDisk_Replaces)
    {
        DiskImageStore  store;
        int             recoveryWrites = 0;



        store.SetFlushSink ([&recoveryWrites] (const string & path, const vector<Byte> &)
        {
            if (path.ends_with (".recovered.woz"))
            {
                recoveryWrites++;
            }

            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Session.dsk", DiskFormat::Dsk, MakeDsk (0x5A)));
        CorruptOneAddressField (*store.GetImage (kSlot, kDrive), 3);
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);

        AssertFailed (store.FlushAll());
        Assert::AreEqual (1, recoveryWrites, L"the session is in a recovery copy");

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\New.dsk", DiskFormat::Dsk, MakeDsk (0x22)));

        Assert::AreEqual (string ("C:\\disks\\New.dsk"), store.GetSourcePath (kSlot, kDrive), L"the copy lets the new disk in");
        Assert::AreEqual (1, recoveryWrites, L"with no second copy");
    }


    TEST_METHOD (MountRestored_UnloadableBytes_LeavesTheBay)
    {
        DiskImageStore  store;
        DiskImage     * image = nullptr;



        store.SetFlushSink ([] (const string &, const vector<Byte> &) { return S_OK; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Kept.dsk", DiskFormat::Dsk, MakeDsk (0x11)));
        image = store.GetImage (kSlot, kDrive);

        AssertFailed (store.MountRestored (kSlot, kDrive, "C:\\disks\\Saved.dsk", DiskFormat::Dsk, vector<Byte> (4096, 0)));

        Assert::IsTrue (store.GetImage (kSlot, kDrive) == image, L"the disk that was there is in the drive");
        Assert::AreEqual (string ("C:\\disks\\Kept.dsk"), store.GetSourcePath (kSlot, kDrive));
    }


    TEST_METHOD (SetFileBacked_False_WritesNothingAndEjects)
    {
        DiskImageStore  store;
        int             writes = 0;



        store.SetFileBacked (false);
        store.SetFlushSink ([&writes] (const string &, const vector<Byte> &) { writes++; return HRESULT_FROM_WIN32 (ERROR_DISK_FULL); });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Scratch.dsk", DiskFormat::Dsk, MakeDsk (0x11)));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);

        store.Eject (kSlot, kDrive);

        Assert::AreEqual (0, writes, L"a store with no files writes nothing");
        Assert::IsFalse (store.IsMounted (kSlot, kDrive), L"and its eject goes ahead");
    }


    //  The same file mounted again over a disk with guest writes: the disk is
    //  saved first, and the file read after the save, so the drive and the
    //  file both have the writes.
    TEST_METHOD (Mount_SameFileOverADirtyDisk_ReadsWhatTheFlushWrote)
    {
        DiskImageStore  store;
        FakeDiskFileIo  io;
        DiskImage     * image   = nullptr;
        Byte            written = 0;



        UseFileIo (store, io);
        io.SeedDurable (kSeamWozPath, MakeWoz());

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamWozPath));
        image   = store.GetImage (kSlot, kDrive);
        written = static_cast<Byte> (image->ReadBit (0, 200) ^ 1);
        image->WriteBit (0, 200, written);

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamWozPath));

        Assert::AreEqual (written, store.GetImage (kSlot, kDrive)->ReadBit (0, 200), L"the drive has the write");
        Assert::IsFalse (store.GetImage (kSlot, kDrive)->IsDirty(), L"and it is in the file");
    }


    TEST_METHOD (Mount_SameFileOverAnUnwritableDisk_KeepsTheDisk)
    {
        DiskImageStore  store;
        FakeDiskFileIo  io;
        MountDiagnosis  diagnosis;
        DiskImage     * image = nullptr;



        UseFileIo (store, io);
        io.SeedDurable (kSeamWozPath, MakeWoz());

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamWozPath));
        image = store.GetImage (kSlot, kDrive);
        image->WriteBit (0, 200, static_cast<Byte> (image->ReadBit (0, 200) ^ 1));

        io.failWritesContaining = "casso-no-such-root";
        io.nextWriteError       = HRESULT_FROM_WIN32 (ERROR_DISK_FULL);

        AssertFailed (store.Mount (kSlot, kDrive, kSeamWozPath, diagnosis));
        Assert::IsTrue (diagnosis.failure == MountFailure::UnsavedWrites);
        Assert::IsTrue (diagnosis.isSameFile);
        Assert::IsFalse (diagnosis.wasReported, L"the first decline is reported");

        AssertFailed (store.Mount (kSlot, kDrive, kSeamWozPath, diagnosis));
        Assert::IsTrue (diagnosis.wasReported, L"the second is not");

        Assert::IsTrue (store.GetImage (kSlot, kDrive) == image, L"the disk is in the drive");
        Assert::IsTrue (image->IsDirty(), L"with its writes");
    }


    TEST_METHOD (Mount_SameFileOverARescuedUnserializableDisk_KeepsTheSession)
    {
        DiskImageStore  store;
        FakeDiskFileIo  io;
        DiskImage     * image = nullptr;



        UseFileIo (store, io);
        io.SeedDurable (kSeamImagePath, MakeDsk (0x5A));

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamImagePath));
        image = store.GetImage (kSlot, kDrive);
        CorruptOneAddressField (*image, 3);
        image->SetLoadedForTest (true, true);

        AssertFailed (store.FlushAll());
        AssertFailed (store.Mount (kSlot, kDrive, kSeamImagePath), L"the file is not read over the session");

        Assert::IsTrue (store.GetImage (kSlot, kDrive) == image, L"the session is in the drive");
        Assert::IsTrue (image->IsDirty());
    }


    TEST_METHOD (Mount_SameFileOverAnUnserializableDiskWithNoCopy_GivesOneNotice)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        FakeDiskFileIo            io;
        MountDiagnosis            diagnosis;



        UseFileIo (store, io);
        io.SeedDurable (kSeamImagePath, MakeDsk (0x5A));

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamImagePath));
        CorruptOneAddressField (*store.GetImage (kSlot, kDrive), 3);
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);

        AssertFailed (store.Mount (kSlot, kDrive, kSeamImagePath, diagnosis));

        Assert::AreEqual (1, s_flushNotifyCount, L"the first recovery copy is reported");
        Assert::IsFalse (diagnosis.ShouldReport(), L"and the decline is not reported again");
    }


    TEST_METHOD (Mount_SameUnchangedFile_KeepsTheImageWithNoMediaChange)
    {
        DiskImageStore  store;
        FakeDiskFileIo  io;
        DiskImage     * image   = nullptr;
        int             changes = 0;



        UseFileIo (store, io);
        io.SeedDurable (kSeamWozPath, MakeWoz());

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamWozPath));
        image = store.GetImage (kSlot, kDrive);
        store.SetMediaChangeListener ([&changes] { changes++; });

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamWozPath));

        Assert::IsTrue (store.GetImage (kSlot, kDrive) == image, L"the image that is the file stays");
        Assert::AreEqual (0, changes, L"and no media change is reported");
    }


    TEST_METHOD (MountDiagnosis_ShouldReport_IsFalseOnlyForAReportedSameFileDecline)
    {
        MountDiagnosis  diagnosis;



        diagnosis.failure     = MountFailure::UnsavedWrites;
        diagnosis.isSameFile  = true;
        diagnosis.wasReported = true;
        Assert::IsFalse (diagnosis.ShouldReport(), L"a same-file decline already reported");

        diagnosis.wasReported = false;
        Assert::IsTrue (diagnosis.ShouldReport(), L"a same-file decline not reported yet");

        diagnosis.isSameFile  = false;
        diagnosis.wasReported = true;
        Assert::IsTrue (diagnosis.ShouldReport(), L"an insert of another file always");

        diagnosis.failure = MountFailure::WrongSizeForFormat;
        Assert::IsTrue (diagnosis.ShouldReport(), L"and any other decline");
    }


    TEST_METHOD (FormatMountFailureMessage_UnsavedWrites_OtherFile)
    {
        wstring  reason = ChangePrompt::DescribeError (HRESULT_FROM_WIN32 (ERROR_DISK_FULL));



        Assert::AreEqual (wstring (L"Casso did not insert this disk image:\n\nC:\\disks\\New.dsk\n\n"
                                   L"The disk already in drive 1 has changes that could not be saved to its file:\n\n"
                                   L"C:\\disks\\Kept.dsk\n\nError: ") + reason +
                          L"\n\nThat disk is still in the drive with its changes. Eject it to save them to another file.",
                          DiskStoreText::FormatMountFailureMessage ("C:\\disks\\New.dsk",
                              UnsavedWritesOver ("C:\\disks\\Kept.dsk", HRESULT_FROM_WIN32 (ERROR_DISK_FULL), false)));

        Assert::AreEqual (wstring (L"Casso did not insert this disk image:\n\nC:\\disks\\New.dsk\n\n"
                                   L"The disk already in drive 1 has changes that have not been saved to its file yet:\n\n"
                                   L"C:\\disks\\Kept.dsk\n\n"
                                   L"That disk is still in the drive with its changes. Eject it to save them to another file."),
                          DiskStoreText::FormatMountFailureMessage ("C:\\disks\\New.dsk",
                              UnsavedWritesOver ("C:\\disks\\Kept.dsk", S_OK, false)),
                          L"with no save attempted, no Error paragraph");
    }


    TEST_METHOD (FormatMountFailureMessage_UnsavedWrites_SameFile)
    {
        wstring  reason = ChangePrompt::DescribeError (HRESULT_FROM_WIN32 (ERROR_DISK_FULL));



        Assert::AreEqual (wstring (L"Casso could not save changes to the disk image:\n\nC:\\disks\\Kept.dsk\n\nError: ") + reason +
                          L"\n\nThe file on disk is unchanged, and the disk was not read from it again. The drive still has your changes.",
                          DiskStoreText::FormatMountFailureMessage ("C:\\disks\\Kept.dsk",
                              UnsavedWritesOver ("C:\\disks\\Kept.dsk", HRESULT_FROM_WIN32 (ERROR_DISK_FULL), true)));
    }


    TEST_METHOD (FormatMountFailureMessage_KeptDiskIsStated)
    {
        MountDiagnosis  diagnosis = DiagnosisOf (MountFailure::WrongSizeForFormat, DiskFormat::Dsk, 4096);
        wstring         message;



        diagnosis.keptPath = "C:\\disks\\Kept.dsk";
        message            = DiskStoreText::FormatMountFailureMessage ("C:\\disks\\Short.dsk", diagnosis);

        Assert::IsTrue (message.ends_with (L"\n\nThe disk already in the drive was not changed."),
                        L"a decline over an occupied drive states that the disk there was not changed");
    }


    TEST_METHOD (ClassifyLoadFailure_ShortSectorImage_NamesTheLength)
    {
        vector<Byte>    truncated (4096, 0);
        MountDiagnosis  diagnosis = DiskImageStore::ClassifyLoadFailure (DiskFormat::Dsk,
                                                                         truncated);



        Assert::IsTrue (diagnosis.failure == MountFailure::WrongSizeForFormat,
            L"a sector image of the wrong length is diagnosed by its length");
        Assert::AreEqual ((size_t) 4096, diagnosis.fileByteSize,
            L"the size the user's file actually was must survive to the message");
    }


    TEST_METHOD (ClassifyLoadFailure_NibbleOfTheWrongLength_NamesItsOwnSizes)
    {
        vector<Byte>    truncated (100000, 0xFF);
        MountDiagnosis  diagnosis = DiskImageStore::ClassifyLoadFailure (DiskFormat::Nib,
                                                                         truncated);
        std::string     clause    = diagnosis.Describe();

        Assert::IsTrue (diagnosis.failure == MountFailure::WrongSizeForNibble,
            L"a nibble image has its own size rule and its own verdict");
        Assert::AreEqual ((size_t) 100000, diagnosis.fileByteSize);

        //  BOTH accepted totals, because either can carry either name. A
        //  clause naming one of them tells half the users the wrong thing.
        Assert::IsTrue (clause.find ("232,960") != std::string::npos,
            L"the clause must give the standard total");
        Assert::IsTrue (clause.find ("223,440") != std::string::npos,
            L"and the smaller one, which circulates under the same name");
        Assert::IsTrue (clause.find ("100,000") != std::string::npos,
            L"and the size the user's file actually was");
    }


    TEST_METHOD (ClassifyLoadFailure_RightSizedButNoNibbles_IsItsOwnVerdict)
    {
        vector<Byte>    zeros (NibbleImageCodec::kNibImageSize, 0x00);
        MountDiagnosis  wrongSize = DiskImageStore::ClassifyLoadFailure (DiskFormat::Nib,
                                                                         vector<Byte> (999, 0xFF));
        MountDiagnosis  noNibbles = DiskImageStore::ClassifyLoadFailure (DiskFormat::Nib, zeros);

        //  The two nibble refusals must not read alike: one says the download
        //  stopped early, the other says the file was renamed from something
        //  else, and a user can act on each.
        Assert::IsTrue (noNibbles.failure == MountFailure::NotANibbleStream,
            L"right size, no assemblable nibble anywhere");
        Assert::AreNotEqual (wrongSize.Describe(), noNibbles.Describe(),
            L"the two nibble refusals must differ");
    }


    TEST_METHOD (ClassifyLoadFailure_NibbleGarbageOfTheRightLength_IsAccepted)
    {
        //  DELIBERATELY NOT REFUSED, and asserted so nobody later "fixes" it.
        //  The format has no signature, header or checksum, and about half of
        //  random bytes carry the high bit, so a renamed archive of the right
        //  length is indistinguishable from a real image by inspection. It
        //  mounts as a disk that will not boot, which is the honest outcome; a
        //  stricter rule would refuse odd but genuine images to catch files
        //  that fail harmlessly anyway.
        vector<Byte>    garbage (NibbleImageCodec::kNibImageSize, 0);
        size_t          i = 0;

        for (i = 0; i < garbage.size(); i++)
        {
            garbage[i] = static_cast<Byte> ((i * 37) & 0xFF);
        }

        MountDiagnosis  diagnosis = DiskImageStore::ClassifyLoadFailure (DiskFormat::Nib, garbage);

        Assert::IsTrue (diagnosis.failure == MountFailure::Unrecognized,
            L"content this loader cannot rule out is not given a nibble verdict");
    }


    TEST_METHOD (Mount_MalformedNibbleImage_NeverAsserts)
    {
        //  A file the user named is edge input. E_INVALIDARG marks a coding
        //  error in this tree and always asserts, so no mount path may reach
        //  one however wrong the bytes are.
        DiskImageStore  store;
        MountDiagnosis  diagnosis;
        HRESULT         hr = S_OK;

        hr = store.MountFromBytes (kSlot, kDrive, "short.nib", DiskFormat::Nib,
                                   vector<Byte> (17, 0xFF), diagnosis);
        AssertFailed (hr);

        hr = store.MountFromBytes (kSlot, kDrive, "empty.nib", DiskFormat::Nib,
                                   vector<Byte>(), diagnosis);
        AssertFailed (hr);

        hr = store.MountFromBytes (kSlot, kDrive, "zeros.nib", DiskFormat::Nib,
                                   vector<Byte> (NibbleImageCodec::kNibImageSize, 0), diagnosis);
        AssertFailed (hr);

        //  AND THE VERDICT REACHES THE USER. Asserting only the HRESULT let a
        //  gap through: the loader checked geometry alone, so a right-sized
        //  file of zeros MOUNTED, and NotANibbleStream was a verdict nothing
        //  could ever produce. A diagnosis with no path to it is support that
        //  looks present and is not.
        Assert::IsTrue (diagnosis.failure == MountFailure::NotANibbleStream,
            L"the mount path must produce the verdict, not just fail");
    }


    TEST_METHOD (Mount_NibbleImage_AttributesWriteProtectionToTheFileNotTheImage)
    {
        //  A nibble image carries no write-protect flag of its own, unlike a
        //  WOZ. Only the host file's state and the user's setting can protect
        //  it, and the interface must not imply the image asked for it.
        DiskImageStore    store;
        DiskImage         built;
        vector<Byte>      sectors (NibblizationLayer::kImageByteSize, 0x11);
        vector<Byte>      file;
        DiskImage       * img = nullptr;
        WriteProtectInfo  info;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, built));
        AssertSucceeded (NibbleImageCodec::Serialize (built, vector<Byte>(), file));
        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "plain.nib", DiskFormat::Nib, file));

        img = store.GetImage (kSlot, kDrive);
        Assert::IsNotNull (img);

        info = img->GetWriteProtectInfo();
        Assert::IsFalse (info.imageFlag, L"the format has no flag to carry");
        Assert::IsFalse (info.Any(),     L"so an unprotected nibble image is unprotected");

        img->SetUserWriteProtected (true);
        info = img->GetWriteProtectInfo();

        Assert::IsTrue  (info.userSetting, L"the user's setting is what protects it");
        Assert::IsFalse (info.imageFlag,   L"and it must not be reported as the image's own");
    }


    TEST_METHOD (ClassifyLoadFailure_EmptyFile_OutranksTheLengthTest)
    {
        vector<Byte>    nothing;
        MountDiagnosis  asDsk = DiskImageStore::ClassifyLoadFailure (DiskFormat::Dsk, nothing);
        MountDiagnosis  asWoz = DiskImageStore::ClassifyLoadFailure (DiskFormat::Woz, nothing);



        // A zero-byte file is the wrong size for every container, and saying
        // so by arithmetic against 143,360 buries the one useful fact.
        Assert::IsTrue (asDsk.failure == MountFailure::EmptyFile,
            L"an empty sector image is empty before it is the wrong size");
        Assert::IsTrue (asWoz.failure == MountFailure::EmptyFile,
            L"and an empty .woz is empty before it is a missing header");
    }


    TEST_METHOD (ClassifyLoadFailure_WozWithoutAHeader_IsNotAWozAtAll)
    {
        vector<Byte>    renamed (600, 0x41);
        MountDiagnosis  diagnosis = DiskImageStore::ClassifyLoadFailure (DiskFormat::Woz, renamed);



        Assert::IsTrue (diagnosis.failure == MountFailure::NotAWozFile,
            L"bytes with no WOZ header are not a damaged WOZ, they are not one");
    }


    TEST_METHOD (ClassifyLoadFailure_WozWithAHeader_IsADamagedWoz)
    {
        vector<Byte>    woz     = MakeWoz();
        size_t          keep    = WozLoader::kHeaderSize + 16;
        vector<Byte>    chopped (woz.begin(), woz.begin() + (ptrdiff_t) keep);
        MountDiagnosis  diagnosis;



        // The header survives the truncation, so what is left IS a WOZ -- an
        // incomplete one. Telling the user it is not a WOZ would send them
        // looking for a file they already have.
        diagnosis = DiskImageStore::ClassifyLoadFailure (DiskFormat::Woz, chopped);

        Assert::IsTrue (diagnosis.failure == MountFailure::MalformedWoz,
            L"a real WOZ header over broken chunks is a damaged image");
    }


    TEST_METHOD (MountFromBytes_ShortDsk_RefusesWithALengthReasonAndNoAssert)
    {
        DiskImageStore  store;
        MountDiagnosis  diagnosis;
        vector<Byte>    truncated (4096, 0);
        HRESULT         hr = S_OK;



        // NO ExpectedEhmAssert HERE, AND THAT IS THE TEST. A malformed file
        // dropped on a drive used to trip the nibblizer's E_INVALIDARG guard
        // and raise an assertion dialog in Debug, before any of the reporting
        // below could run. A user's bad file is not a coding error.
        hr = store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Truncated.dsk",
                                   DiskFormat::Dsk, truncated, diagnosis);

        AssertFailed (hr);
        Assert::IsFalse (store.IsMounted (kSlot, kDrive),
            L"a refused image must leave the bay empty rather than half-mounted");
        Assert::IsTrue (diagnosis.failure == MountFailure::WrongSizeForFormat,
            L"and the refusal must carry the reason out with it");
        Assert::AreEqual ((size_t) 4096, diagnosis.fileByteSize);
    }


    TEST_METHOD (MountFromBytes_Succeeding_LeavesNoStaleReason)
    {
        DiskImageStore  store;
        MountDiagnosis  diagnosis = DiagnosisOf (MountFailure::MalformedWoz);
        HRESULT         hr        = S_OK;



        // A caller reusing one diagnosis across mounts must not read the
        // previous refusal off a mount that worked.
        hr = store.MountFromBytes (kSlot, kDrive, "C:\\disks\\Good.dsk",
                                   DiskFormat::Dsk, MakeDsk (0), diagnosis);

        AssertSucceeded (hr);
        Assert::IsFalse (diagnosis.HasFailure(),
            L"a mount that worked must report no reason at all");
    }


    TEST_METHOD (Mount_UnknownExtension_IsDiagnosedBeforeAnythingIsRead)
    {
        DiskImageStore  store;
        MountDiagnosis  diagnosis;
        int             reads     = 0;
        HRESULT         hr        = S_OK;



        store.SetImageReader ([&reads] (const string &, vector<Byte> &) -> HRESULT
                              {
                                  reads++;
                                  return E_FAIL;
                              });

        hr = store.Mount (kSlot, kDrive, "C:\\disks\\Notes.txt", diagnosis);

        AssertFailed (hr);
        Assert::IsTrue (diagnosis.failure == MountFailure::UnknownExtension,
            L"a name no loader claims is settled by the name");
        Assert::AreEqual (0, reads,
            L"and settled without opening the file, which nothing would read");
    }


    TEST_METHOD (Mount_UnreadableFile_IsDiagnosedAsAReadFailure)
    {
        DiskImageStore  store;
        MountDiagnosis  diagnosis;
        HRESULT         hr = S_OK;



        // The extension is fine and the bytes never arrive: a moved file, or
        // one another program is holding. That is not the same problem as a
        // file whose contents the loader refused, and must not read like it.
        store.SetImageReader ([] (const string &, vector<Byte> &) -> HRESULT
                              {
                                  return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
                              });

        hr = store.Mount (kSlot, kDrive, "C:\\disks\\Gone.dsk", diagnosis);

        AssertFailed (hr);
        Assert::IsTrue (diagnosis.failure == MountFailure::FileUnreadable,
            L"bytes that never arrived are a read failure, not a bad image");
    }


    TEST_METHOD (FormatMountFailureMessage_UnknownExtension_ListsWhatIsRead)
    {
        wstring  message = DiskStoreText::FormatMountFailureMessage (
                               "C:\\disks\\Notes.txt",
                               DiagnosisOf (MountFailure::UnknownExtension));



        Assert::IsTrue (message.find (L"C:\\disks\\Notes.txt") != wstring::npos,
            L"the message must identify the file the user picked");

        //  Every container the tool advertises, read off the same table the
        //  tool reads -- a name added there and left out here is exactly the
        //  drift this used to have, when the sentence named four of five.
        {
            size_t  count = 0;
            const DiskCommandRunner::ContainerName *  names =
                DiskCommandRunner::GetAdvertisedContainers (count);

            Assert::IsTrue (count >= 5, L"the table is not shorter than what shipped");

            for (size_t i = 0; i < count; i++)
            {
                wstring  dotted = L"." + wstring (names[i].name, names[i].name + strlen (names[i].name));

                Assert::IsTrue (message.find (dotted) != wstring::npos,
                    (L"the refusal must name " + dotted).c_str());
            }
        }
    }


    TEST_METHOD (FormatMountFailureMessage_KnownExtension_BlamesTheContents)
    {
        wstring  message = DiskStoreText::FormatMountFailureMessage (
                               "C:\\disks\\Broken.dsk",
                               DiagnosisOf (MountFailure::WrongSizeForFormat,
                                            DiskFormat::Dsk, 4096));



        Assert::IsTrue (message.find (L"C:\\disks\\Broken.dsk") != wstring::npos,
            L"the message must identify the file the user picked");

        // A .dsk IS read, so reciting the supported formats back would be
        // both useless and misleading -- the extension was never the problem.
        Assert::IsTrue (message.find (L".woz") == wstring::npos,
            L"a recognized extension must not be met with the format list");
    }


    TEST_METHOD (FormatMountFailureMessage_WrongSize_GivesBothNumbers)
    {
        wstring  message = DiskStoreText::FormatMountFailureMessage (
                               "C:\\disks\\Truncated.dsk",
                               DiagnosisOf (MountFailure::WrongSizeForFormat,
                                            DiskFormat::Dsk, 4096));



        // "Not a valid disk image" is barely better than saying nothing. The
        // two numbers together are what tell somebody their download stopped
        // early, which is the whole reason the size is carried this far.
        Assert::IsTrue (message.find (L"4,096 bytes") != wstring::npos,
            L"the message must report how big the file actually is");
        Assert::IsTrue (message.find (L"143,360 bytes") != wstring::npos,
            L"and how big it should have been");
        Assert::IsTrue (message.find (L".dsk") != wstring::npos,
            L"identified as the container the file's own name promised");
    }


    TEST_METHOD (FormatMountFailureMessage_WrongSize_NamesTheFormatItWasGiven)
    {
        wstring  message = DiskStoreText::FormatMountFailureMessage (
                               "C:\\disks\\Half.po",
                               DiagnosisOf (MountFailure::WrongSizeForFormat,
                                            DiskFormat::Po, 71680));



        Assert::IsTrue (message.find (L".po") != wstring::npos,
            L"a .po must not be told what a .dsk should weigh");
        Assert::IsTrue (message.find (L"71,680 bytes") != wstring::npos);
    }


    TEST_METHOD (FormatMountFailureMessage_Unreadable_PointsAtTheFileNotTheContents)
    {
        wstring  message = DiskStoreText::FormatMountFailureMessage (
                               "C:\\disks\\Gone.dsk",
                               DiagnosisOf (MountFailure::FileUnreadable, DiskFormat::Dsk));



        Assert::IsTrue (message.find (L"cannot be read") != wstring::npos,
            L"a file that never opened must be reported as a file, not as a bad image");
        Assert::IsTrue (message.find (L"143,360") == wstring::npos,
            L"and must not be lectured about a size nobody measured");
    }


    TEST_METHOD (FormatMountFailureMessage_EmptyFile_SaysSoPlainly)
    {
        wstring  message = DiskStoreText::FormatMountFailureMessage (
                               "C:\\disks\\Zero.dsk",
                               DiagnosisOf (MountFailure::EmptyFile, DiskFormat::Dsk));



        Assert::IsTrue (message.find (L"empty") != wstring::npos,
            L"a zero-byte file is empty, which is more use than an arithmetic complaint");
    }


    TEST_METHOD (FormatMountFailureMessage_NotAWoz_SuggestsARename)
    {
        wstring  message = DiskStoreText::FormatMountFailureMessage (
                               "C:\\disks\\Renamed.woz",
                               DiagnosisOf (MountFailure::NotAWozFile, DiskFormat::Woz));



        Assert::IsTrue (message.find (L"WOZ file header") != wstring::npos,
            L"the missing header is the fact, and reporting it is what explains the refusal");
        Assert::IsTrue (message.find (L"renamed") != wstring::npos,
            L"a .woz that is not a WOZ is almost always a renamed file, so report that");
    }


    TEST_METHOD (FormatMountFailureMessage_MalformedWoz_SaysTheFileIsDamaged)
    {
        wstring  notAWoz = DiskStoreText::FormatMountFailureMessage (
                               "C:\\disks\\Renamed.woz",
                               DiagnosisOf (MountFailure::NotAWozFile, DiskFormat::Woz));
        wstring  damaged = DiskStoreText::FormatMountFailureMessage (
                               "C:\\disks\\Damaged.woz",
                               DiagnosisOf (MountFailure::MalformedWoz, DiskFormat::Woz));



        Assert::IsTrue (damaged.find (L"damaged or incomplete") != wstring::npos,
            L"a real WOZ header over broken chunks is a damaged file, and reads as one");

        // The two send the user in different directions: one to find the real
        // image, the other to fetch this one again. Sharing wording would
        // throw that away.
        Assert::AreNotEqual (notAWoz, damaged,
            L"a renamed file and a damaged one must not read alike");
    }


    TEST_METHOD (FormatMountFailureMessage_EmptyPath_StillReadsAsASentence)
    {
        wstring  message = DiskStoreText::FormatMountFailureMessage (
                               string(), DiagnosisOf (MountFailure::Unrecognized));



        Assert::IsTrue (message.find (L"(unknown path)") != wstring::npos,
            L"a missing path must be marked as missing, not left as a blank gap");
    }


    TEST_METHOD (FormatMountFailureMessage_EveryReason_SaysSomethingOfItsOwn)
    {
        //  MountFailure is TOTAL over the wordings, so the sweep runs the
        //  enum rather than a table of the ones somebody remembered. A table
        //  sweep visits only rows that exist and structurally cannot find a
        //  missing one.
        const MountFailure  reasons[] =
        {
            MountFailure::None,
            MountFailure::UnknownExtension,
            MountFailure::FileUnreadable,
            MountFailure::EmptyFile,
            MountFailure::WrongSizeForFormat,
            MountFailure::NotAWozFile,
            MountFailure::MalformedWoz,
            MountFailure::Unrecognized,
            MountFailure::WrongSizeForNibble,
            MountFailure::NotANibbleStream,
            MountFailure::AlreadyMounted,
            MountFailure::UnsavedWrites,
            MountFailure::BehindLive,
        };

        vector<wstring>  seen;
        size_t           i    = 0;
        size_t           j    = 0;



        Assert::AreEqual ((size_t) 13, sizeof (reasons) / sizeof (reasons[0]),
            L"an enumerator added without a wording must fail here, not ship silent");

        for (i = 0; i < sizeof (reasons) / sizeof (reasons[0]); i++)
        {
            wstring  message = DiskStoreText::FormatMountFailureMessage (
                                   "C:\\disks\\Image.dsk",
                                   DiagnosisOf (reasons[i], DiskFormat::Dsk, 4096));

            Assert::IsTrue (message.find (L"C:\\disks\\Image.dsk") != wstring::npos,
                L"every reason names the file");

            for (j = 0; j < seen.size(); j++)
            {
                Assert::AreNotEqual (seen[j], message,
                    L"two reasons sharing one sentence is the failure being fixed");
            }

            seen.push_back (message);
        }
    }


    TEST_METHOD (FormatByteCount_GroupsDigitsAndAgreesWithItself)
    {
        //  Grouped by hand rather than through a locale, so two users
        //  comparing the same refusal see the same number.
        Assert::AreEqual (string ("0 bytes"),       MountDiagnosis::FormatByteCount (0));
        Assert::AreEqual (string ("1 byte"),        MountDiagnosis::FormatByteCount (1));
        Assert::AreEqual (string ("999 bytes"),     MountDiagnosis::FormatByteCount (999));
        Assert::AreEqual (string ("1,000 bytes"),   MountDiagnosis::FormatByteCount (1000));
        Assert::AreEqual (string ("143,360 bytes"), MountDiagnosis::FormatByteCount (143360));
        Assert::AreEqual (string ("1,048,576 bytes"),
                          MountDiagnosis::FormatByteCount (1048576));
    }


    TEST_METHOD (MakeRecoveryPath_NeverCollidesWithAnEarlierRescue)
    {
        // Overwriting a previous recovery would repeat the mistake being fixed:
        // one rescue must never cost another.
        string  first  = DiskImageStore::MakeRecoveryPath ("C:\\disks\\Work.dsk", 0);
        string  second = DiskImageStore::MakeRecoveryPath ("C:\\disks\\Work.dsk", 1);



        Assert::AreEqual (string ("C:\\disks\\Work.recovered.woz"), first);
        Assert::AreNotEqual (first, second, L"a second rescue must take a different name");
    }
};
