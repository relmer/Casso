#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/MountDiagnosis.h"
#include "Devices/Disk/NibbleImageCodec.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/WozLoader.h"
#include "Devices/Disk2Controller.h"

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




TEST_CLASS (DiskImageStoreTests)
{
public:

    //  Breaks one address field's checksum so the track can no longer be
    //  denibblized without loss, which is what makes Serialize refuse.
    static void CorruptOneAddressField (DiskImage & img, int track)
    {
        vector<Byte> &  bits   = img.GetTrackBitsForWrite (track);
        size_t          i      = 0;
        bool            done   = false;



        for (i = 0; i + 10 < bits.size() && !done; i++)
        {
            if (bits[i] == 0xD5 && bits[i + 1] == 0xAA && bits[i + 2] == 0x96)
            {
                // Write a checksum that cannot be right for any header. The
                // encoded odd/even pair carries only the value's own bits, so
                // replacing both is unconditional where flipping one is not.
                bits[i + 9]  = 0xAA;
                bits[i + 10] = 0xAA;
                done         = true;
            }
        }

        Assert::IsTrue (done, L"the track must carry an address field to corrupt");
    }


    static constexpr int   kSlot  = 6;
    static constexpr int   kDrive = 0;

    vector<Byte> MakeDsk (Byte fill)
    {
        return vector<Byte> (NibblizationLayer::kImageByteSize, fill);
    }

    // A full-size sector image with a distinct per-byte pattern so a
    // round-trip actually exercises every sector (not a uniform fill).
    vector<Byte> MakeSectorImage (uint32_t seed)
    {
        vector<Byte>  raw (NibblizationLayer::kImageByteSize);

        for (size_t i = 0; i < raw.size(); i++)
        {
            raw[i] = static_cast<Byte> ((i * 31u + seed) & 0xFF);
        }

        return raw;
    }

    // A minimal single-track WOZ v2 image with a recognizable bit pattern
    // (mostly 0xFF sync + a D5 AA 96 address prolog marker).
    vector<Byte> MakeWoz (bool writeProtected = false)
    {
        vector<Byte>   bits (6400, 0xFF);
        vector<Byte>   woz;

        bits[20] = 0xD5;
        bits[21] = 0xAA;
        bits[22] = 0x96;

        WozLoader::BuildSyntheticV2 (1, writeProtected, bits, 51200, woz);
        return woz;
    }

    // Captures EhmNotifyUser output for the flush-error tests. FlushEntry
    // surfaces losses through the shared EHM notifier (CHRN/CBRN), so these
    // route the global notifier to file-local counters for the span of one
    // test. The notifier is a raw function pointer, hence a free function +
    // static state rather than a capturing lambda.
    static inline int      s_flushNotifyCount = 0;
    static inline wstring  s_flushNotifyLast;

    static void CaptureFlushNotify (const wchar_t * message)
    {
        s_flushNotifyCount++;
        s_flushNotifyLast = (message != nullptr) ? message : L"";
    }

    // RAII: routes EhmNotifyUser to the capture (resetting the counters)
    // for the test's lifetime, then clears the global hook so the next test
    // starts clean even if an assertion throws out of the body.
    struct ScopedFlushNotifyCapture
    {
        ScopedFlushNotifyCapture()
        {
            s_flushNotifyCount = 0;
            s_flushNotifyLast.clear();
            SetNotifyFunction (CaptureFlushNotify);
        }

        ~ScopedFlushNotifyCapture()
        {
            SetNotifyFunction (nullptr);
        }
    };

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


    ////////////////////////////////////////////////////////////////////////
    //
    //  Flush-error reporting. A dirty image that fails to persist used to
    //  vanish (every flush caller drops the HRESULT). The store now surfaces
    //  the loss through the EHM notifier; a clean or successful flush must
    //  stay silent.
    //
    ////////////////////////////////////////////////////////////////////////

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


    ////////////////////////////////////////////////////////////////////////
    //
    //  Cross-format flush round-trip through the real store path
    //  (MountFromBytes -> mark dirty -> Flush -> Serialize -> sink). Prior
    //  coverage was DSK-only and size-checked; these assert *content* for
    //  every sector format and a guest-write survives for WOZ.
    //
    ////////////////////////////////////////////////////////////////////////

    // mount raw -> dirty (no bit corruption) -> flush -> captured == raw.
    void VerifyFaithfulSectorFlush (DiskFormat fmt, const char * path)
    {
        DiskImageStore   store;
        vector<Byte>     raw = MakeSectorImage (0xABCD1234u);
        vector<Byte>     captured;

        store.SetFlushSink ([&] (const string &, const vector<Byte> & b) { captured = b; return S_OK; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, path, fmt, raw));

        // Mark dirty without touching the bit stream so the flush must
        // reproduce the original image exactly.
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);

        AssertSucceeded (store.Flush (kSlot, kDrive));
        Assert::IsTrue   (raw == captured,
            L"flushed bytes must faithfully round-trip the sector image");
    }

    TEST_METHOD (Flush_FaithfulRoundTrip_Dsk)  { VerifyFaithfulSectorFlush (DiskFormat::Dsk, "rt.dsk"); }
    TEST_METHOD (Flush_FaithfulRoundTrip_Do)   { VerifyFaithfulSectorFlush (DiskFormat::Do,  "rt.do");  }
    TEST_METHOD (Flush_FaithfulRoundTrip_Po)   { VerifyFaithfulSectorFlush (DiskFormat::Po,  "rt.po");  }

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


    ////////////////////////////////////////////////////////////////////////
    //
    //  Flush-error notification reaches through the bulk flush paths that
    //  also drop the HRESULT (FlushAll / PowerCycle), once per failed dirty
    //  mount.
    //
    ////////////////////////////////////////////////////////////////////////

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


    ////////////////////////////////////////////////////////////////////////
    //
    //  Integration: motor spin-down (Disk2Controller, CPU thread) wired to a
    //  real DiskImageStore persists a dirty WOZ through the flush sink -- the
    //  end-to-end path behind motor-idle auto-flush + WOZ write-back.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (MotorOffFlush_persistsDirtyWozThroughStore)
    {
        DiskImageStore   store;
        vector<Byte>     woz        = MakeWoz();
        vector<Byte>     captured;
        bool             flushed    = false;
        const size_t     flippedBit = 200;
        DiskImageStore   store2;

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
        AssertSucceeded (store2.MountFromBytes (6, 0, "m.woz", DiskFormat::Woz, captured));
        Assert::AreEqual (Byte (0), store2.GetImage (6, 0)->ReadBit (0, flippedBit),
            L"the write persisted at motor-off must survive reload");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Atomic persist. FlushEntry used to open the mounted image's own path
    //  with ofstream, which truncates it before the first byte is written --
    //  so a write that then failed (full volume, disconnected share) left no
    //  file at all, and nothing checked the stream afterwards, so the image
    //  was marked clean either way. These cover the replacement: the target
    //  is only ever swapped for a fully written temp file.
    //
    //  Not covered here: a write that fails PART WAY through. Forcing that
    //  needs a filesystem seam this class does not have, so the guarantee it
    //  rests on -- the target is untouched until the temp file is complete --
    //  is checked structurally below rather than by simulating a short write.
    //
    ////////////////////////////////////////////////////////////////////////

    // A scratch path in the system temp directory, removed by the caller.
    static fs::path ScratchPath (const char * name)
    {
        return fs::temp_directory_path() / (string ("casso_atomic_") + name);
    }


    static string ReadBackAll (const fs::path & path)
    {
        ifstream           file (path, ios::binary);
        std::stringstream  buffer;

        buffer << file.rdbuf();

        return buffer.str();
    }


    TEST_METHOD (WriteFileAtomically_ReplacesTargetAndLeavesNoTempBehind)
    {
        fs::path         target = ScratchPath ("replace.bin");
        vector<Byte>     bytes  (2048, 0x5A);
        std::error_code  ec;

        {
            ofstream  seed (target, ios::binary);
            seed << "stale content";
        }

        AssertSucceeded (DiskImageStore::WriteFileAtomically (target.string(), bytes));

        Assert::AreEqual (size_t (2048), ReadBackAll (target).size(),
            L"the target must hold the new bytes, not the stale ones");
        Assert::IsFalse (fs::exists (DiskImageStore::GetCommitTemporaryPath (target.string(), 0)),
            L"a successful write must not leave its temp file behind");

        fs::remove (target, ec);
    }


    TEST_METHOD (WriteFileAtomically_UnwritableTarget_LeavesOriginalIntact)
    {
        // The target directory does not exist, so the temp file cannot be
        // created. The point is what does NOT happen: no partial file, no
        // truncated target, and a reported failure rather than a silent one.
        fs::path      missingDir = ScratchPath ("no_such_dir");
        fs::path      target     = missingDir / "image.dsk";
        vector<Byte>  bytes (512, 0x11);
        HRESULT       hr         = S_OK;

        Assert::IsFalse (fs::exists (missingDir), L"precondition: the directory must not exist");

        hr = DiskImageStore::WriteFileAtomically (target.string(), bytes);

        Assert::IsTrue (FAILED (hr),
            L"an impossible write must report failure, not succeed silently");

        //  AND IT REPORTS WHICH FAILURE. This came back as E_FAIL, so the
        //  save-failure notice -- built to print the code and the system's own
        //  words for it -- said "Unspecified error" for a folder that refused
        //  the file. The second assertion is the one that would have caught it.
        Assert::AreEqual ((int) HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND), (int) hr,
            L"the system's own code for a folder that is not there");
        Assert::IsTrue (hr != E_FAIL, L"never the code that says nothing");
        Assert::IsFalse (fs::exists (target), L"no partial target may be left behind");
        Assert::IsFalse (fs::exists (DiskImageStore::GetCommitTemporaryPath (target.string(), 0)),
            L"no temp file may be left behind on failure");
    }


    TEST_METHOD (Flush_ToRealFile_WritesThroughAtomicPath)
    {
        // The production branch -- no flush sink -- end to end on a real
        // file. Every other test in this class installs a sink, so this path
        // had no coverage at all.
        DiskImageStore   store;
        fs::path         target = ScratchPath ("flush.dsk");
        vector<Byte>     seedBytes = MakeDsk (0x24);
        std::error_code  ec;

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, target.string(),
                                               DiskFormat::Dsk, seedBytes));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0,
            static_cast<uint8_t> (store.GetImage (kSlot, kDrive)->ReadBit (0, 0) ^ 1));

        AssertSucceeded (store.Flush (kSlot, kDrive));

        Assert::AreEqual (static_cast<size_t> (NibblizationLayer::kImageByteSize), ReadBackAll (target).size(),
            L"a real-file flush must write a full-size image");
        Assert::IsFalse (fs::exists (DiskImageStore::GetCommitTemporaryPath (target.string(), 0)),
            L"the flush must not leave its temp file beside the image");

        fs::remove (target, ec);
    }


    // A WOZ carrying a valid CRC, then damaged -- the stored checksum now
    // describes what the file used to be.
    vector<Byte> MakeCrcDamagedWoz()
    {
        DiskImage     src;
        vector<Byte>  bits (6400, 0xFF);
        vector<Byte>  woz;
        vector<Byte>  out;

        bits[20] = 0xD5;
        bits[21] = 0xAA;
        bits[22] = 0x96;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (1, false, bits, 51200, woz));
        AssertSucceeded (WozLoader::Load (woz, src));
        AssertSucceeded (WozLoader::Serialize (src, out));

        out[out.size() - 1] = static_cast<Byte> (out[out.size() - 1] ^ 0xFF);

        return out;
    }


    TEST_METHOD (Mount_CrcMismatchedImage_IsWriteProtected)
    {
        // A damaged image used to be rewritten, with a warning first. That
        // traded a detectable problem for an undetectable one: the rewrite
        // stamps a freshly computed checksum over the same damage, so nothing
        // afterwards can tell the file is wrong. It is now held read-only for
        // the session instead, and never rewritten at all.
        DiskImageStore  store;
        vector<Byte>    damaged = MakeCrcDamagedWoz();
        DiskImage *     img     = nullptr;

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "suspect.woz",
                                               DiskFormat::Woz, damaged));

        img = store.GetImage (kSlot, kDrive);
        Assert::IsNotNull (img);
        Assert::IsTrue (img->HasSourceCrcMismatch(),
            L"precondition: the mount must have seen the bad checksum");

        Assert::IsTrue (img->IsWriteProtected(),
            L"a damaged image must be write-protected");
        Assert::IsTrue (img->GetWriteProtectInfo().checksumMismatch,
            L"and it must give the reason, so the UI can explain a state the user did not choose");

        // Not the image flag: that lives in the WOZ's INFO chunk, so setting
        // it would mean writing the very file being protected from writes.
        Assert::IsFalse (img->GetWriteProtectInfo().imageFlag,
            L"the damaged state must not be recorded as the in-file flag");
        Assert::IsFalse (img->GetWriteProtectInfo().userSetting,
            L"nor confused with the user's own toggle");
    }


    TEST_METHOD (Flush_CrcMismatchedImage_NeverRewritesTheFile)
    {
        // The gate that makes the protection real. A guest cannot dirty a
        // write-protected image, so this reaches past the emulation to set the
        // bit directly -- the point is that the flush path itself refuses.
        DiskImageStore  store;
        vector<Byte>    damaged   = MakeCrcDamagedWoz();
        DiskImage *     img       = nullptr;
        int             sinkCalls = 0;

        store.SetFlushSink ([&sinkCalls] (const string &, const vector<Byte> &)
        {
            sinkCalls++;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "suspect.woz",
                                               DiskFormat::Woz, damaged));

        img = store.GetImage (kSlot, kDrive);
        Assert::IsNotNull (img);

        img->WriteBit (0, 0, static_cast<uint8_t> (img->ReadBit (0, 0) ^ 1));
        AssertSucceeded (store.Flush (kSlot, kDrive));

        Assert::AreEqual (0, sinkCalls,
            L"a damaged image must never be written back -- the rewrite is what "
            L"would make its damage undetectable");
        Assert::IsTrue (img->HasSourceCrcMismatch(),
            L"and the file is still damaged, so the flag must stay set");
    }


    TEST_METHOD (SetImageWriteProtect_OnADamagedImage_IsRefused)
    {
        // The one remaining route that would have rewritten a damaged file.
        // Patching the write-protect flag recomputes the header checksum, and
        // that checksum failing to match IS the damage report -- so the write
        // that is otherwise harmless is precisely the one that would destroy
        // the evidence.
        DiskImageStore  store;
        vector<Byte>    file      = MakeCrcDamagedWoz();
        vector<Byte>    original;
        int             sinkCalls = 0;
        HRESULT         hr        = S_OK;

        original = file;

        store.SetImageReader ([&file] (const string &, vector<Byte> & out)
        {
            out = file;
            return S_OK;
        });

        store.SetFlushSink ([&sinkCalls, &file] (const string &, const vector<Byte> & bytes)
        {
            sinkCalls++;
            file = bytes;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "suspect.woz",
                                               DiskFormat::Woz, file));

        hr = store.SetImageWriteProtect (kSlot, kDrive, true);

        Assert::IsTrue (FAILED (hr), L"the toggle must refuse a damaged image");
        Assert::AreEqual (0, sinkCalls, L"and must not write anything on the way to refusing");
        Assert::IsTrue (file == original, L"so the file is byte-for-byte untouched");
    }

    ////////////////////////////////////////////////////////////////////////
    //
    //  Salvage at the store level: the gate, the counts, and the one
    //  guarantee the whole feature rests on -- the damaged original is
    //  never written to.
    //
    ////////////////////////////////////////////////////////////////////////

    // A WOZ built from a real sector image, so its tracks are ordinary
    // 16-sector data, with the header CRC then broken so the image mounts as
    // damaged. That combination -- damaged AND standard -- is what salvage
    // is for.
    vector<Byte> MakeDamagedStandardWoz()
    {
        DiskImage     src;
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        vector<Byte>  woz;
        uint32_t      seed = 0xC0FFEEu;
        size_t        i    = 0;

        for (i = 0; i < sectors.size(); i++)
        {
            seed       = seed * 1664525u + 1013904223u;
            sectors[i] = static_cast<Byte> ((seed >> 16) & 0xFF);
        }

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, src));
        src.SetSourceFormat (DiskFormat::Woz);
        AssertSucceeded (WozLoader::Serialize (src, woz));

        // Break the stored checksum without touching the data, so the image
        // is damaged in exactly the way that write-protects it.
        woz[8] = static_cast<Byte> (woz[8] ^ 0xFF);

        return woz;
    }


    TEST_METHOD (AssessSalvage_DamagedStandardDisk_IsOffered)
    {
        DiskImageStore     store;
        SalvageAssessment  assessment;

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, MakeDamagedStandardWoz()));

        AssertSucceeded (store.AssessSalvage (kSlot, kDrive, assessment));

        Assert::IsTrue (assessment.isOffered,
            L"a damaged disk with ordinary sectors is exactly what salvage is for");
        Assert::AreEqual (560, assessment.totalSectors,
            L"the total counts the tracks this disk has, not a flat 35 x 16 assumption");
        Assert::AreEqual (560, assessment.report.sectorsVerified,
            L"the damage here is the file checksum, not the sectors -- all still verify");
        Assert::IsTrue (assessment.suggestedPath.find ("broken.salvaged.woz") != string::npos,
            L"the suggested name describes what the file is");
    }


    TEST_METHOD (AssessSalvage_UndamagedDisk_IsNotOffered)
    {
        // Nothing to escape from: an undamaged disk is not write-protected, so
        // a lossy copy could only lose data.
        DiskImageStore     store;
        SalvageAssessment  assessment;
        vector<Byte>       healthy = MakeDamagedStandardWoz();

        healthy[8] = static_cast<Byte> (healthy[8] ^ 0xFF);   // put the checksum back

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "fine.woz",
                                               DiskFormat::Woz, healthy));
        Assert::IsFalse (store.GetImage (kSlot, kDrive)->HasSourceCrcMismatch(),
            L"precondition: this image is not damaged");

        AssertSucceeded (store.AssessSalvage (kSlot, kDrive, assessment));

        Assert::IsFalse (assessment.isOffered,
            L"salvage must not be offered for a disk the user can already write to");
    }



    TEST_METHOD (AssessSalvage_UndamagedDisk_DoesNotDecodeTheDisk)
    {
        // Pins the cheap path, because the expensive one is not merely slow --
        // this runs from the Disk menu's enable query, so it runs every time
        // that menu is drawn. Decoding unconditionally cost 11 ms for an
        // ordinary disk and 154 ms for a copy-protected one, per drive.
        //
        // Damage is free to test, salvage is only ever offered for a damaged
        // disk, so an undamaged one must return before decoding anything. An
        // untouched report is how that is observable from here.
        DiskImageStore     store;
        SalvageAssessment  assessment;
        vector<Byte>       healthy = MakeDamagedStandardWoz();

        healthy[8] = static_cast<Byte> (healthy[8] ^ 0xFF);   // put the checksum back

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "fine.woz",
                                               DiskFormat::Woz, healthy));
        AssertSucceeded (store.AssessSalvage (kSlot, kDrive, assessment));

        Assert::AreEqual (0, assessment.report.tracksPresent,
            L"an undamaged disk must not be decoded at all");
        Assert::AreEqual (0, assessment.totalSectors,
            L"and no counts are produced, because none were needed");
    }


    TEST_METHOD (AssessSalvage_NonStandardDisk_IsNotOffered)
    {
        // A copy-protected disk has no standard sectors to recover, and
        // rebuilding it from sectors would destroy the tracks it depends on.
        // It never reaches the dialog, which is why the dialog never has to
        // explain copy protection.
        DiskImageStore     store;
        SalvageAssessment  assessment;
        vector<Byte>       woz;
        vector<Byte>       bits ((51200 + 7) / 8, 0xFF);

        // A track of pure sync bytes: a legal bit stream with no address
        // fields at all, which is what protection looks like from here.
        AssertSucceeded (WozLoader::BuildSyntheticV2 (1, false, bits, 51200, woz));
        woz[8] = static_cast<Byte> (woz[8] ^ 0xFF);   // damaged too, so only structure decides

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "protected.woz",
                                               DiskFormat::Woz, woz));

        AssertSucceeded (store.AssessSalvage (kSlot, kDrive, assessment));

        Assert::IsTrue (assessment.report.tracksUnformatted > 0,
            L"precondition: this disk has no standard structure");
        Assert::IsFalse (assessment.isOffered,
            L"salvage must refuse a disk whose tracks it cannot rebuild");
    }


    TEST_METHOD (AssessSalvage_WritesNothing)
    {
        // It exists to inform a decision, so it must not make one.
        DiskImageStore     store;
        SalvageAssessment  assessment;
        int                sinkCalls = 0;

        store.SetFlushSink ([&sinkCalls] (const string &, const vector<Byte> &)
        {
            sinkCalls++;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, MakeDamagedStandardWoz()));
        AssertSucceeded (store.AssessSalvage (kSlot, kDrive, assessment));

        Assert::AreEqual (0, sinkCalls, L"assessing must write nothing at all");
    }


    TEST_METHOD (SalvageToFile_WritesTheCopyAndLeavesTheOriginalAlone)
    {
        // The guarantee the whole feature rests on. The damaged original has
        // to survive, still damaged and still detectably so, or salvage has
        // destroyed the evidence it was built to preserve.
        DiskImageStore    store;
        DenibblizeReport  report;
        vector<Byte>      original = MakeDamagedStandardWoz();
        string            writtenTo;
        vector<Byte>      written;
        DiskImage         reloaded;

        store.SetFlushSink ([&writtenTo, &written] (const string & path,
                                                    const vector<Byte> & bytes)
        {
            writtenTo = path;
            written   = bytes;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, original));

        AssertSucceeded (store.SalvageToFile (kSlot, kDrive, "broken.salvaged.woz", report));

        Assert::AreEqual (string ("broken.salvaged.woz"), writtenTo,
            L"the copy goes to its own file");
        Assert::IsTrue (written.size() > 0, L"and it actually holds something");

        // The salvaged copy is a valid, undamaged WOZ.
        AssertSucceeded (WozLoader::Load (written, reloaded));
        Assert::IsFalse (reloaded.HasSourceCrcMismatch(),
            L"the salvaged copy carries a correct checksum of its own");

        Assert::AreEqual (string ("broken.woz"), store.GetSourcePath (kSlot, kDrive),
            L"and the mount still points at the untouched original");
    }


    TEST_METHOD (SalvageToFile_RefusesToOverwriteTheSource)
    {
        // Salvage exists so the damaged original survives. Writing over it
        // would defeat the entire point, so the path is checked rather than
        // trusted.
        DiskImageStore    store;
        DenibblizeReport  report;
        int               sinkCalls = 0;
        HRESULT           hr        = S_OK;

        store.SetFlushSink ([&sinkCalls] (const string &, const vector<Byte> &)
        {
            sinkCalls++;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, MakeDamagedStandardWoz()));

        UnitTestHelpers::ExpectedEhmAssert  expected;

        hr = store.SalvageToFile (kSlot, kDrive, "broken.woz", report);

        Assert::IsTrue (FAILED (hr), L"salvaging onto the source must be refused");
        Assert::AreEqual (0, sinkCalls, L"and nothing may be written on the way to refusing");
    }


    TEST_METHOD (SalvageToFile_KeepsTheMetadataButClaimsTheFile)
    {
        // The salvaged copy is still the same disk -- title, publisher and
        // provenance travel -- but Casso wrote this particular file, and
        // leaving someone else's name in creator would put a preservation
        // tool's signature on a lossy reconstruction.
        DiskImageStore    store;
        DenibblizeReport  report;
        vector<Byte>      original = MakeDamagedStandardWoz();
        vector<Byte>      written;
        string            meta     = "title\tSalvage Test\npublisher\tCasso\n";
        Byte              header[8] = { 'M', 'E', 'T', 'A', 0, 0, 0, 0 };

        header[4] = static_cast<Byte> (meta.size() & 0xFF);
        original.insert (original.end(), header, header + sizeof (header));
        original.insert (original.end(), meta.begin(), meta.end());

        store.SetFlushSink ([&written] (const string &, const vector<Byte> & bytes)
        {
            written = bytes;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, original));
        AssertSucceeded (store.SalvageToFile (kSlot, kDrive, "broken.salvaged.woz", report));

        {
            string  blob (reinterpret_cast<const char *> (written.data()), written.size());

            Assert::IsTrue (blob.find ("title\tSalvage Test") != string::npos,
                L"the salvaged copy is still the same disk, so META travels");
            Assert::IsTrue (blob.find ("Casso ") != string::npos,
                L"but Casso wrote this file and records that in creator");
        }
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


    ////////////////////////////////////////////////////////////////////////
    //
    //  Mount diagnosis
    //
    //  A mount that did not happen has to say WHICH way it did not happen.
    //  Every reason below is one the load path can genuinely tell apart, and
    //  each of them sends the user somewhere different: refetch the file,
    //  find the real image, look for the program holding it open. The
    //  message used to collapse all but one of them into a single sentence
    //  that named none.
    //
    ////////////////////////////////////////////////////////////////////////

    //  A diagnosis of one reason, for the message tests, so each of them
    //  says which reason it is wording rather than assembling a struct.
    static MountDiagnosis  DiagnosisOf (MountFailure failure,
                                        DiskFormat   format = DiskFormat::Dsk,
                                        size_t       fileByteSize = 0)
    {
        MountDiagnosis  diagnosis;



        diagnosis.failure      = failure;
        diagnosis.format       = format;
        diagnosis.fileByteSize = fileByteSize;

        return diagnosis;
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


    ////////////////////////////////////////////////////////////////////////
    //
    //  FormatMountFailureMessage
    //
    //  The wording each reason produces. Asserted on the sentence a person
    //  reads, not on a paraphrase of it -- the message IS the feature here,
    //  and a message that named nothing specific is what this replaced.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FormatMountFailureMessage_UnknownExtension_ListsWhatIsRead)
    {
        wstring  message = DiskImageStore::FormatMountFailureMessage (
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
        wstring  message = DiskImageStore::FormatMountFailureMessage (
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
        wstring  message = DiskImageStore::FormatMountFailureMessage (
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
        wstring  message = DiskImageStore::FormatMountFailureMessage (
                               "C:\\disks\\Half.po",
                               DiagnosisOf (MountFailure::WrongSizeForFormat,
                                            DiskFormat::Po, 71680));



        Assert::IsTrue (message.find (L".po") != wstring::npos,
            L"a .po must not be told what a .dsk should weigh");
        Assert::IsTrue (message.find (L"71,680 bytes") != wstring::npos);
    }


    TEST_METHOD (FormatMountFailureMessage_Unreadable_PointsAtTheFileNotTheContents)
    {
        wstring  message = DiskImageStore::FormatMountFailureMessage (
                               "C:\\disks\\Gone.dsk",
                               DiagnosisOf (MountFailure::FileUnreadable, DiskFormat::Dsk));



        Assert::IsTrue (message.find (L"cannot be read") != wstring::npos,
            L"a file that never opened must be reported as a file, not as a bad image");
        Assert::IsTrue (message.find (L"143,360") == wstring::npos,
            L"and must not be lectured about a size nobody measured");
    }


    TEST_METHOD (FormatMountFailureMessage_EmptyFile_SaysSoPlainly)
    {
        wstring  message = DiskImageStore::FormatMountFailureMessage (
                               "C:\\disks\\Zero.dsk",
                               DiagnosisOf (MountFailure::EmptyFile, DiskFormat::Dsk));



        Assert::IsTrue (message.find (L"empty") != wstring::npos,
            L"a zero-byte file is empty, which is more use than an arithmetic complaint");
    }


    TEST_METHOD (FormatMountFailureMessage_NotAWoz_SuggestsARename)
    {
        wstring  message = DiskImageStore::FormatMountFailureMessage (
                               "C:\\disks\\Renamed.woz",
                               DiagnosisOf (MountFailure::NotAWozFile, DiskFormat::Woz));



        Assert::IsTrue (message.find (L"WOZ file header") != wstring::npos,
            L"the missing header is the fact, and reporting it is what explains the refusal");
        Assert::IsTrue (message.find (L"renamed") != wstring::npos,
            L"a .woz that is not a WOZ is almost always a renamed file, so report that");
    }


    TEST_METHOD (FormatMountFailureMessage_MalformedWoz_SaysTheFileIsDamaged)
    {
        wstring  notAWoz = DiskImageStore::FormatMountFailureMessage (
                               "C:\\disks\\Renamed.woz",
                               DiagnosisOf (MountFailure::NotAWozFile, DiskFormat::Woz));
        wstring  damaged = DiskImageStore::FormatMountFailureMessage (
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
        wstring  message = DiskImageStore::FormatMountFailureMessage (
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
        };

        vector<wstring>  seen;
        size_t           i    = 0;
        size_t           j    = 0;



        Assert::AreEqual ((size_t) 8, sizeof (reasons) / sizeof (reasons[0]),
            L"an enumerator added without a wording must fail here, not ship silent");

        for (i = 0; i < sizeof (reasons) / sizeof (reasons[0]); i++)
        {
            wstring  message = DiskImageStore::FormatMountFailureMessage (
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

