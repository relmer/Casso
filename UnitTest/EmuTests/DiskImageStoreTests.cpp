#include "Pch.h"
#include "DiskStoreTestSupport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStoreTests
//
//  Phase 10 / FR-023 / FR-025 / audit §7. Auto-flush invariants:
//  Eject flushes dirty, FlushAll on machine switch, SoftReset preserves
//  mounts (and flushes dirty), PowerCycle unmounts all (and flushes
//  dirty). All tests use MountFromBytes + a flush sink so writes go into
//  an in-memory capture buffer rather than the host filesystem (test
//  isolation contract).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskImageStoreTests), public DiskStoreTestSupport
{
public:

    TEST_METHOD (GetSourceFormatByExtension_KnownTypes)
    {
        DiskFormat   fmt = DiskFormat::Dsk;

        AssertSucceeded (DiskImageStore::GetSourceFormatByExtension ("foo.dsk", fmt));
        Assert::IsTrue (fmt == DiskFormat::Dsk);

        AssertSucceeded (DiskImageStore::GetSourceFormatByExtension ("foo.DO", fmt));
        Assert::IsTrue (fmt == DiskFormat::Do);

        AssertSucceeded (DiskImageStore::GetSourceFormatByExtension ("foo.po", fmt));
        Assert::IsTrue (fmt == DiskFormat::Po);

        AssertSucceeded (DiskImageStore::GetSourceFormatByExtension ("foo.WOZ", fmt));
        Assert::IsTrue (fmt == DiskFormat::Woz);
    }


    TEST_METHOD (GetSourceFormatByExtension_UnknownReturnsFail)
    {
        DiskFormat   fmt = DiskFormat::Dsk;

        AssertFailed (DiskImageStore::GetSourceFormatByExtension ("foo.bin", fmt));
        AssertFailed (DiskImageStore::GetSourceFormatByExtension ("noext",  fmt));
    }


    TEST_METHOD (IsMountableImageExtension_MatchesTheRoutedTypes)
    {
        Assert::IsTrue  (DiskImageStore::IsMountableImageExtension (string ("foo.dsk")));
        Assert::IsTrue  (DiskImageStore::IsMountableImageExtension (string ("foo.DO")));
        Assert::IsTrue  (DiskImageStore::IsMountableImageExtension (string ("foo.po")));
        Assert::IsTrue  (DiskImageStore::IsMountableImageExtension (string ("foo.WOZ")));

        Assert::IsFalse (DiskImageStore::IsMountableImageExtension (string ("foo.bin")));
        Assert::IsFalse (DiskImageStore::IsMountableImageExtension (string ("noext")));
    }


    TEST_METHOD (EveryDiskFormat_HasAnExtensionThatRoutesBackToIt)
    {
        //  THE SWEEP IS OVER THE ENUM, NOT OVER A LIST OF FORMATS. A list
        //  visits only the rows its author remembered, so it cannot find the
        //  arm they forgot -- which is the failure this guards. There is a
        //  live example in the tree: BlankDiskBuilder::ValidateSpec has no
        //  DiskFormat::Do arm and asserts on a container the tool advertises,
        //  and no table-driven test noticed because no table lists it.
        //
        //  Both directions matter. A format with no extension arm answers the
        //  "disk" fallback; one with no routing arm fails to come back.
        int          count  = (int) DiskFormat::Count;
        int          i      = 0;
        DiskFormat   fmt    = DiskFormat::Dsk;
        DiskFormat   routed = DiskFormat::Dsk;
        const char * ext    = nullptr;
        std::string  path;
        HRESULT      hr     = S_OK;

        Assert::IsTrue (count > 0, L"the enum must not be empty");

        for (i = 0; i < count; i++)
        {
            fmt = (DiskFormat) i;
            ext = MountDiagnosis::GetPrimaryExtension (fmt);

            Assert::AreNotEqual ("disk", ext,
                L"every format needs its own extension, not the fallback");

            path = std::string ("image") + ext;
            hr   = DiskImageStore::GetSourceFormatByExtension (path, routed);

            AssertSucceeded (hr, L"a format's own primary extension must route");
            Assert::AreEqual ((int) fmt, (int) routed,
                L"and must route back to the format it came from");
        }
    }


    TEST_METHOD (IsMountableImageExtension_AcceptsNibbleImages)
    {
        // INVERTED, NOT DELETED. Mount now routes both nibble extensions, and
        // the assertion that it did not belongs turned around rather than
        // removed -- the filter and the router disagreeing over these names is
        // the exact defect this test was written for.
        Assert::IsTrue (DiskImageStore::IsMountableImageExtension (string ("foo.nib")));
        Assert::IsTrue (DiskImageStore::IsMountableImageExtension (string ("foo.nb2")));
        Assert::IsTrue (DiskImageStore::IsMountableImageExtension (wstring (L"foo.NIB")));
        Assert::IsTrue (DiskImageStore::IsMountableImageExtension (wstring (L"foo.Nb2")));
    }


    TEST_METHOD (IsMountableImageExtension_WideAgreesWithNarrow)
    {
        Assert::IsTrue  (DiskImageStore::IsMountableImageExtension (wstring (L"C:\\Disks\\BOOT.DSK")));
        Assert::IsTrue  (DiskImageStore::IsMountableImageExtension (wstring (L"C:\\Disks\\demo.woz")));
        Assert::IsFalse (DiskImageStore::IsMountableImageExtension (wstring (L"C:\\Disks\\notes.txt")));
        Assert::IsFalse (DiskImageStore::IsMountableImageExtension (wstring (L"")));
    }


    TEST_METHOD (MountFromBytes_DskRunsNibblization)
    {
        DiskImageStore    store;
        vector<Byte>      raw   = MakeDsk (0xA5);
        DiskImage       * img   = nullptr;

        HRESULT   hr = store.MountFromBytes (kSlot, kDrive, "synthetic.dsk", DiskFormat::Dsk, raw);

        AssertSucceeded (hr);
        Assert::IsTrue (store.IsMounted (kSlot, kDrive));

        img = store.GetImage (kSlot, kDrive);

        Assert::IsNotNull (img);
        Assert::IsTrue   (img->GetTrackBitCount (0) > 0,
            L"DSK mount must produce nibblized track bits");
    }


    TEST_METHOD (MountedSourcePaths_ReportsEveryMountedBay)
    {
        DiskImageStore    store;
        vector<Byte>      raw   = MakeDsk (0x11);

        AssertSucceeded (store.MountFromBytes (6, 0, "a.dsk", DiskFormat::Dsk, raw));
        AssertSucceeded (store.MountFromBytes (6, 1, "b.dsk", DiskFormat::Dsk, raw));

        auto  mounted = store.GetMountedSourcePaths();

        Assert::AreEqual ((size_t) 2, mounted.size());
        Assert::AreEqual (std::string ("a.dsk"), mounted[0].path);
        Assert::AreEqual (0, mounted[0].drive);
        Assert::AreEqual (std::string ("b.dsk"), mounted[1].path);
        Assert::AreEqual (1, mounted[1].drive);

        store.Eject (6, 0);
        mounted = store.GetMountedSourcePaths();

        Assert::AreEqual ((size_t) 1, mounted.size());
        Assert::AreEqual (std::string ("b.dsk"), mounted[0].path);
    }


    TEST_METHOD (MountedSourcePaths_SkipsEmptyVirtualPath)
    {
        DiskImageStore    store;
        vector<Byte>      raw   = MakeDsk (0x22);

        AssertSucceeded (store.MountFromBytes (6, 0, "", DiskFormat::Dsk, raw));

        Assert::AreEqual ((size_t) 0, store.GetMountedSourcePaths().size());
    }


    TEST_METHOD (MountFromBytes_WozNativeNoNibblization)
    {
        DiskImageStore    store;
        vector<Byte>      woz;
        DiskImage       * img   = nullptr;
        vector<Byte>     bits ((6400), 0xFF);

        AssertSucceeded (WozLoader::BuildSyntheticV2 (1, false, bits, 51200, woz));

        HRESULT   hr = store.MountFromBytes (kSlot, kDrive, "synthetic.woz", DiskFormat::Woz, woz);

        AssertSucceeded (hr);

        img = store.GetImage (kSlot, kDrive);

        Assert::IsNotNull (img);
        Assert::IsTrue   (img->GetSourceFormat() == DiskFormat::Woz);
        Assert::AreEqual (size_t (51200), img->GetTrackBitCount (0));
    }


    TEST_METHOD (Eject_AutoFlushesDirty)
    {
        DiskImageStore        store;
        vector<Byte>          raw          = MakeDsk (0);
        vector<Byte>          captured;
        string                capturedPath;
        bool                  invoked      = false;

        store.SetFlushSink ([&](const string & path, const vector<Byte> & bytes)
        {
            capturedPath = path;
            captured     = bytes;
            invoked      = true;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "x.dsk", DiskFormat::Dsk, raw));

        // Mark dirty by writing a bit through the public API.
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);
        Assert::IsTrue (store.GetImage (kSlot, kDrive)->IsDirty());

        store.Eject (kSlot, kDrive);

        Assert::IsTrue  (invoked, L"FR-025: Eject must auto-flush dirty image");
        Assert::AreEqual (string ("x.dsk"), capturedPath);
        Assert::IsFalse (store.IsMounted (kSlot, kDrive));
    }


    TEST_METHOD (Eject_NoFlushIfClean)
    {
        DiskImageStore   store;
        vector<Byte>     raw     = MakeDsk (0);
        bool             invoked = false;

        store.SetFlushSink ([&](const string &, const vector<Byte> &)
        {
            invoked = true;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "x.dsk", DiskFormat::Dsk, raw));
        store.Eject (kSlot, kDrive);

        Assert::IsFalse (invoked, L"FR-025: clean image must NOT be flushed");
    }


    TEST_METHOD (FlushAll_FlushesEveryDirtyMount)
    {
        DiskImageStore   store;
        vector<Byte>     raw        = MakeDsk (0);
        int              flushCount = 0;

        store.SetFlushSink ([&](const string &, const vector<Byte> &)
        {
            flushCount++;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (6, 0, "a.dsk", DiskFormat::Dsk, raw));
        AssertSucceeded (store.MountFromBytes (6, 1, "b.dsk", DiskFormat::Dsk, raw));
        AssertSucceeded (store.MountFromBytes (5, 0, "c.dsk", DiskFormat::Dsk, raw));

        store.GetImage (6, 0)->WriteBit (0, 0, 1);
        store.GetImage (5, 0)->WriteBit (0, 0, 1);
        // (6,1) intentionally clean.

        AssertSucceeded (store.FlushAll());
        Assert::AreEqual (2, flushCount, L"FlushAll must flush exactly the dirty mounts");

        // Mounts persist after FlushAll.
        Assert::IsTrue (store.IsMounted (6, 0));
        Assert::IsTrue (store.IsMounted (6, 1));
        Assert::IsTrue (store.IsMounted (5, 0));
    }


    TEST_METHOD (FlushError_notifiesOnFailure_namingThePath)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        vector<Byte>              raw     = MakeDsk (0);
        HRESULT                   hr      = S_OK;

        //  The sink fails the way a folder that refuses the file does, so the
        //  notice has a real code to carry.
        store.SetFlushSink ([] (const string &, const vector<Byte> &)
        {
            return HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED);
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "boom.dsk", DiskFormat::Dsk, raw));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);   // dirty

        hr = store.Flush (kSlot, kDrive);

        AssertFailed (hr);
        Assert::AreEqual (1, s_flushNotifyCount, L"a failed flush must be surfaced, not swallowed");
        Assert::IsTrue   (s_flushNotifyLast.find (L"boom.dsk") != wstring::npos,
            L"the notification must identify the image that failed to save");

        //  AND SAY WHY. It named no cause and then advised switching to .woz,
        //  which cannot help a permission failure. The code and the system's
        //  words for it are what the reader can act on.
        Assert::IsTrue   (s_flushNotifyLast.find (L"0x80070005") != wstring::npos,
            L"the notice prints the failure code");
        Assert::IsTrue   (s_flushNotifyLast.find (L"Access is denied") != wstring::npos,
            L"and the system's own words for it");
        Assert::IsTrue   (s_flushNotifyLast.find (L".woz") == wstring::npos,
            L"and no longer prescribes a format change for a permission problem");
    }


    TEST_METHOD (FlushError_nibbleImage_reportsWritesItCouldNotPersist)
    {
        //  The same loss report as the sector formats, asserted for this
        //  container. The mechanism is shared and format-agnostic, which is
        //  precisely why it is worth pinning per container: nothing about a
        //  passing .dsk test says the nibble flush reaches the notifier.
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        DiskImage                 built;
        vector<Byte>              sectors (NibblizationLayer::kImageByteSize, 0x3C);
        vector<Byte>              file;
        HRESULT                   hr = S_OK;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, built));
        AssertSucceeded (NibbleImageCodec::Serialize (built, vector<Byte>(), file));

        store.SetFlushSink ([] (const string &, const vector<Byte> &) { return E_FAIL; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "boom.nib", DiskFormat::Nib, file));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);   // dirty

        hr = store.Flush (kSlot, kDrive);

        AssertFailed (hr);
        Assert::AreEqual (1, s_flushNotifyCount, L"a failed nibble flush must be surfaced");
        Assert::IsTrue   (s_flushNotifyLast.find (L"boom.nib") != wstring::npos,
            L"the notification must identify the image that failed to save");
    }


    TEST_METHOD (FlushError_noReportOnCleanOrSuccessfulFlush)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        vector<Byte>              raw = MakeDsk (0);

        store.SetFlushSink ([] (const string &, const vector<Byte> &) { return S_OK; });

        // Clean image ejected -> no flush -> no notification.
        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "clean.dsk", DiskFormat::Dsk, raw));
        store.Eject (kSlot, kDrive);
        Assert::AreEqual (0, s_flushNotifyCount, L"clean image must not notify");

        // Dirty image that flushes successfully -> no notification.
        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "ok.dsk", DiskFormat::Dsk, raw));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);
        AssertSucceeded (store.Flush (kSlot, kDrive));
        Assert::AreEqual (0, s_flushNotifyCount, L"a successful flush must not notify");
    }


    TEST_METHOD (FlushError_surfacesThroughVoidEjectPath)
    {
        // Eject is void and drops FlushEntry's HRESULT; the notification
        // must still fire so an eject that loses writes isn't silent.
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        vector<Byte>              raw = MakeDsk (0);

        store.SetFlushSink ([] (const string &, const vector<Byte> &) { return E_FAIL; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "e.dsk", DiskFormat::Dsk, raw));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);
        store.Eject (kSlot, kDrive);

        Assert::AreEqual (1, s_flushNotifyCount, L"Eject flush failure must surface via the notifier");
        Assert::IsTrue   (store.IsMounted (kSlot, kDrive), L"and the disk stays in the drive");
    }


    TEST_METHOD (FlushAll_ClearsDirtyFlags)
    {
        DiskImageStore   store;
        vector<Byte>     raw = MakeDsk (0);

        store.SetFlushSink ([](const string &, const vector<Byte> &) { return S_OK; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "x.dsk", DiskFormat::Dsk, raw));

        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);
        Assert::IsTrue (store.GetImage (kSlot, kDrive)->IsDirty());

        AssertSucceeded (store.FlushAll());
        Assert::IsFalse (store.GetImage (kSlot, kDrive)->IsDirty(),
            L"FlushAll must clear dirty bits after successful sink write");
    }


    TEST_METHOD (SoftReset_PreservesMountsAndFlushesDirty)
    {
        DiskImageStore   store;
        vector<Byte>     raw     = MakeDsk (0);
        bool             invoked = false;

        store.SetFlushSink ([&](const string &, const vector<Byte> &)
        {
            invoked = true;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "x.dsk", DiskFormat::Dsk, raw));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);

        store.SoftReset();

        Assert::IsTrue (invoked,            L"FR-034: SoftReset must flush dirty images");
        Assert::IsTrue (store.IsMounted (kSlot, kDrive),
            L"FR-034: SoftReset must preserve mounts");
    }


    TEST_METHOD (PowerCycle_UnmountsEverythingAndFlushesDirty)
    {
        DiskImageStore   store;
        vector<Byte>     raw     = MakeDsk (0);
        int              flushed = 0;

        store.SetFlushSink ([&](const string &, const vector<Byte> &)
        {
            flushed++;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (6, 0, "a.dsk", DiskFormat::Dsk, raw));
        AssertSucceeded (store.MountFromBytes (6, 1, "b.dsk", DiskFormat::Dsk, raw));

        store.GetImage (6, 0)->WriteBit (0, 0, 1);

        store.PowerCycle();

        Assert::AreEqual (1, flushed, L"FR-035: PowerCycle must flush only dirty mounts");
        Assert::IsFalse (store.IsMounted (6, 0));
        Assert::IsFalse (store.IsMounted (6, 1));
    }


    TEST_METHOD (DirtyDskFlushesNibblizedSerializedBytes)
    {
        // Capture the bytes flushed by the store and confirm they match
        // NibblizationLayer::Denibblize on the DiskImage at flush time.
        DiskImageStore   store;
        vector<Byte>     raw      = MakeDsk (0x33);
        vector<Byte>     captured;

        store.SetFlushSink ([&](const string &, const vector<Byte> & bytes)
        {
            captured = bytes;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "x.dsk", DiskFormat::Dsk, raw));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);

        AssertSucceeded (store.Flush (kSlot, kDrive));
        Assert::AreEqual (size_t (NibblizationLayer::kImageByteSize), captured.size(),
            L"Flushed payload must be 143360 bytes for DSK");
    }


    TEST_METHOD (RemountReplacesPreviousImage)
    {
        DiskImageStore    store;
        vector<Byte>      raw1  = MakeDsk (0x11);
        vector<Byte>      raw2  = MakeDsk (0x22);
        DiskImage       * first = nullptr;

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "a.dsk", DiskFormat::Dsk, raw1));

        first = store.GetImage (kSlot, kDrive);
        Assert::IsNotNull (first);

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "b.dsk", DiskFormat::Dsk, raw2));
        Assert::IsTrue (store.GetSourcePath (kSlot, kDrive) == "b.dsk");
    }


    TEST_METHOD (MountFromBytes_RejectsBadSlotOrDrive)
    {
        DiskImageStore   store;
        vector<Byte>     raw     = MakeDsk (0);
        HRESULT          hrSlot  = S_OK;
        HRESULT          hrDrive = S_OK;

        {
            // Out-of-range slot / drive are caller bugs, so both assert.
            UnitTestHelpers::ExpectedEhmAssert   expect;

            hrSlot  = store.MountFromBytes (-1, 0, "x.dsk", DiskFormat::Dsk, raw);
            hrDrive = store.MountFromBytes ( 0, 5, "x.dsk", DiskFormat::Dsk, raw);
        }

        AssertFailed (hrSlot);
        AssertFailed (hrDrive);
    }


    TEST_METHOD (Flush_FaithfulRoundTrip_Dsk)  { VerifyFaithfulSectorFlush (DiskFormat::Dsk, "rt.dsk"); }


    TEST_METHOD (Flush_FaithfulRoundTrip_Do)   { VerifyFaithfulSectorFlush (DiskFormat::Do,  "rt.do");  }


    TEST_METHOD (Flush_FaithfulRoundTrip_Po)   { VerifyFaithfulSectorFlush (DiskFormat::Po,  "rt.po");  }


    TEST_METHOD (Flush_NibbleSecondFlushKeepsTheTrackTheFirstSaved)
    {
        DiskImageStore  store;
        vector<Byte>    file = MakeLegalNib();
        vector<Byte>    captured;



        store.SetFlushSink ([&captured] (const string &, const vector<Byte> & bytes)
        {
            captured = bytes;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "two.nib", DiskFormat::Nib, file));

        SaveNibTwice (store, file, [&captured] () { return captured; });
    }


    TEST_METHOD (Flush_NibbleThroughFileIo_SecondFlushKeepsTheTrackTheFirstSaved)
    {
        DiskImageStore  store;
        FakeDiskFileIo  io;
        vector<Byte>    file = MakeLegalNib();
        string          path = "Q:/casso-no-such-root/two.nib";



        UseFileIo (store, io);
        io.SeedDurable (path, file);

        AssertSucceeded (store.Mount (kSlot, kDrive, path));

        SaveNibTwice (store, file, [&io, &path] () { return io.files[path]; });
    }


    TEST_METHOD (Flush_WozGuestWriteSurvivesReloadThroughStore)
    {
        DiskImageStore   store;
        vector<Byte>     woz        = MakeWoz();
        vector<Byte>     captured;
        const size_t     flippedBit = 200;
        DiskImageStore   store2;

        store.SetFlushSink ([&] (const string &, const vector<Byte> & b) { captured = b; return S_OK; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "g.woz", DiskFormat::Woz, woz));
        Assert::AreEqual (Byte (1), store.GetImage (kSlot, kDrive)->ReadBit (0, flippedBit));

        store.GetImage (kSlot, kDrive)->WriteBit (0, flippedBit, 0);   // real guest write -> dirty
        AssertSucceeded (store.Flush (kSlot, kDrive));

        // Reload the flushed bytes: the write must survive the full WOZ
        // serialize -> file -> reload cycle (DiskImage::Serialize dispatch).
        AssertSucceeded (store2.MountFromBytes (kSlot, kDrive, "g.woz", DiskFormat::Woz, captured));
        Assert::AreEqual (Byte (0), store2.GetImage (kSlot, kDrive)->ReadBit (0, flippedBit),
            L"a guest write to a .woz must survive flush + reload through the store");
    }


    TEST_METHOD (FlushError_notifiesPerDirtyMount_viaFlushAll)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        vector<Byte>              raw = MakeDsk (0);

        store.SetFlushSink ([] (const string &, const vector<Byte> &) { return E_FAIL; });

        AssertSucceeded (store.MountFromBytes (6, 0, "a.dsk", DiskFormat::Dsk, raw));
        AssertSucceeded (store.MountFromBytes (6, 1, "b.dsk", DiskFormat::Dsk, raw));
        store.GetImage (6, 0)->WriteBit (0, 0, 1);
        store.GetImage (6, 1)->WriteBit (0, 0, 1);

        store.FlushAll();
        Assert::AreEqual (2, s_flushNotifyCount, L"FlushAll must notify for each dirty mount that fails");
    }


    TEST_METHOD (FlushError_notifies_viaPowerCycle)
    {
        ScopedFlushNotifyCapture  capture;
        DiskImageStore            store;
        vector<Byte>              raw = MakeDsk (0);

        store.SetFlushSink ([] (const string &, const vector<Byte> &) { return E_FAIL; });

        AssertSucceeded (store.MountFromBytes (6, 0, "a.dsk", DiskFormat::Dsk, raw));
        store.GetImage (6, 0)->WriteBit (0, 0, 1);   // (6,1) clean

        store.PowerCycle();
        Assert::AreEqual (1, s_flushNotifyCount, L"PowerCycle must notify for the dirty mount that fails");
    }


    TEST_METHOD (MotorOffFlush_persistsDirtyWozThroughStore)
    {
        DiskImageStore   store;
        vector<Byte>     woz        = MakeWoz();
        vector<Byte>     captured;
        bool             flushed    = false;
        const size_t     flippedBit = 200;
        auto             store2     = std::make_unique<DiskImageStore>();

        store.SetFlushSink ([&] (const string &, const vector<Byte> & b)
        {
            captured = b;
            flushed  = true;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (6, 0, "m.woz", DiskFormat::Woz, woz));
        store.GetImage (6, 0)->WriteBit (0, flippedBit, 0);   // dirty guest write

        Disk2Controller  ctrl (6);
        ctrl.SetMotorOffFlushCallback ([&] () { store.FlushAll(); });

        ctrl.Write (0xC0E9, 0x00);    // motor on
        ctrl.Write (0xC0E8, 0x00);    // motor off (arm spindown)
        ctrl.Tick  (1100000);         // past the spindown -> callback -> FlushAll

        Assert::IsTrue (flushed, L"motor spin-down must persist dirty images via the store");

        // The write must have made it into the persisted bytes.
        AssertSucceeded (store2->MountFromBytes (6, 0, "m.woz", DiskFormat::Woz, captured));
        Assert::AreEqual (Byte (0), store2->GetImage (6, 0)->ReadBit (0, flippedBit),
            L"the write persisted at motor-off must survive reload");
    }


    TEST_METHOD (Flush_ThroughFileIo_FlushesTheTemporaryBeforeTheReplace)
    {
        DiskImageStore  store;
        FakeDiskFileIo  io;
        vector<Byte>    saved;
        string          temp = DiskImageStore::GetCommitTemporaryPath (kSeamImagePath, 0);



        UseFileIo (store, io);
        io.SeedDurable (kSeamImagePath, MakeDsk (0x24));

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamImagePath));
        WriteOneBit (store);
        AssertSucceeded (store.Flush (kSlot, kDrive));

        saved = io.files[kSeamImagePath];
        io.SimulatePowerLoss();

        Assert::IsTrue (io.operations == vector<string> { "Write", "CopyMetadata", "Flush", "Replace" },
            L"the temporary is written, given the image's metadata and flushed before it replaces the image");
        Assert::AreEqual (size_t (1), io.flushedPaths.size());
        Assert::AreEqual (temp, io.flushedPaths[0], L"the flush is of the temporary");
        Assert::AreEqual (size_t (NibblizationLayer::kImageByteSize), saved.size(), L"the save is a whole image");
        Assert::IsTrue (io.files[kSeamImagePath] == saved, L"and it survives the power cut whole");
        Assert::AreEqual (size_t (1), io.files.size(), L"no temporary is left behind");
    }


    TEST_METHOD (Flush_ThroughFileIo_KeepsTheImageFilesAttributesAndPermissions)
    {
        DiskImageStore            store;
        FakeDiskFileIo            io;
        FakeDiskFileIo::Metadata  custom { FILE_ATTRIBUTE_HIDDEN, "D:P(A;;FA;;;OW)" };



        UseFileIo (store, io);
        io.SeedDurable (kSeamImagePath, MakeDsk (0x24));
        io.metadata[kSeamImagePath] = custom;

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamImagePath));
        WriteOneBit (store);
        AssertSucceeded (store.Flush (kSlot, kDrive));

        Assert::IsTrue (io.metadata[kSeamImagePath] == custom,
            L"the saved image has the attributes and permissions it had");
    }


    TEST_METHOD (SetImageWriteProtect_ThroughFileIo_IsDurableAndKeepsMetadata)
    {
        DiskImageStore            store;
        FakeDiskFileIo            io;
        FakeDiskFileIo::Metadata  custom { FILE_ATTRIBUTE_HIDDEN, "D:P(A;;FA;;;OW)" };
        vector<Byte>              patched;



        UseFileIo (store, io);
        io.SeedDurable (kSeamWozPath, MakeWoz());
        io.metadata[kSeamWozPath] = custom;

        AssertSucceeded (store.Mount (kSlot, kDrive, kSeamWozPath));
        AssertSucceeded (store.SetImageWriteProtect (kSlot, kDrive, true));

        patched = io.files[kSeamWozPath];
        io.SimulatePowerLoss();

        Assert::IsTrue (io.operations == vector<string> { "Write", "CopyMetadata", "Flush", "Replace" },
            L"the patched image is committed like a save");
        Assert::IsTrue (patched != MakeWoz(), L"the file has the write-protect flag");
        Assert::IsTrue (io.files[kSeamWozPath] == patched, L"and it survives the power cut whole");
        Assert::IsTrue (io.metadata[kSeamWozPath] == custom, L"with the attributes and permissions it had");
    }


    TEST_METHOD (SalvageToFile_ThroughFileIo_FlushesTheCopyBeforeItAppears)
    {
        DiskImageStore    store;
        FakeDiskFileIo    io;
        DenibblizeReport  report;
        vector<Byte>      copy;
        string            copyPath = "Q:/casso-no-such-root/broken.salvaged.woz";



        UseFileIo (store, io);

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "Q:/casso-no-such-root/broken.woz",
                                               DiskFormat::Woz, MakeDamagedStandardWoz()));
        AssertSucceeded (store.SalvageToFile (kSlot, kDrive, store.GetMediaId (kSlot, kDrive), copyPath, report));

        copy = io.files[copyPath];
        io.SimulatePowerLoss();

        Assert::IsTrue (io.operations == vector<string> { "Write", "Flush", "Replace" },
            L"the copy is flushed before the rename");
        Assert::IsFalse (copy.empty(), L"the copy has the salvaged image");
        Assert::IsTrue (io.files[copyPath] == copy, L"and it survives the power cut whole");
    }
};
