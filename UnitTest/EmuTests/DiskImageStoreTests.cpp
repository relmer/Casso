#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/Store/DiskFlusher.h"
#include "Devices/Disk/Store/DiskSalvager.h"
#include "Devices/Disk/Store/BayFileAccess.h"
#include "Devices/Disk/Store/DiskStoreMessages.h"
#include "Devices/Disk/Store/DiskSourceFormat.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/MountDiagnosis.h"
#include "Machines/Apple2/Common/Denibblizer.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Machines/Apple2/Common/Disk2Controller.h"

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
        return vector<Byte> (NibbleFormat::kImageByteSize, fill);
    }

    // A full-size sector image with a distinct per-byte pattern so a
    // round-trip actually exercises every sector (not a uniform fill).
    vector<Byte> MakeSectorImage (uint32_t seed)
    {
        vector<Byte>  raw (NibbleFormat::kImageByteSize);

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

        AssertSucceeded (store.GetFlusher().FlushAll());
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

        hr = store.GetFlusher().Flush (kSlot, kDrive);

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
        vector<Byte>              sectors (NibbleFormat::kImageByteSize, 0x3C);
        vector<Byte>              file;
        HRESULT                   hr = S_OK;

        AssertSucceeded (Nibblizer::NibblizeDsk (sectors, built));
        AssertSucceeded (NibbleImageCodec::Serialize (built, vector<Byte>(), file));

        store.SetFlushSink ([] (const string &, const vector<Byte> &) { return E_FAIL; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "boom.nib", DiskFormat::Nib, file));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);   // dirty

        hr = store.GetFlusher().Flush (kSlot, kDrive);

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
        AssertSucceeded (store.GetFlusher().Flush (kSlot, kDrive));
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

        AssertSucceeded (store.GetFlusher().FlushAll());
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
        // Denibblizer::Denibblize on the DiskImage at flush time.
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

        AssertSucceeded (store.GetFlusher().Flush (kSlot, kDrive));
        Assert::AreEqual (size_t (NibbleFormat::kImageByteSize), captured.size(),
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

        AssertSucceeded (store.GetFlusher().Flush (kSlot, kDrive));
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
        auto             store2     = std::make_unique<DiskImageStore>();   // a second store is too big for the stack beside the first

        store.SetFlushSink ([&] (const string &, const vector<Byte> & b) { captured = b; return S_OK; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "g.woz", DiskFormat::Woz, woz));
        Assert::AreEqual (Byte (1), store.GetImage (kSlot, kDrive)->ReadBit (0, flippedBit));

        store.GetImage (kSlot, kDrive)->WriteBit (0, flippedBit, 0);   // real guest write -> dirty
        AssertSucceeded (store.GetFlusher().Flush (kSlot, kDrive));

        // Reload the flushed bytes: the write must survive the full WOZ
        // serialize -> file -> reload cycle (DiskImage::Serialize dispatch).
        AssertSucceeded (store2->MountFromBytes (kSlot, kDrive, "g.woz", DiskFormat::Woz, captured));
        Assert::AreEqual (Byte (0), store2->GetImage (kSlot, kDrive)->ReadBit (0, flippedBit),
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

        store.GetFlusher().FlushAll();
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
        auto             store      = std::make_unique<DiskImageStore>();
        vector<Byte>     woz        = MakeWoz();
        vector<Byte>     captured;
        bool             flushed    = false;
        const size_t     flippedBit = 200;
        auto             store2     = std::make_unique<DiskImageStore>();

        store->SetFlushSink ([&] (const string &, const vector<Byte> & b)
        {
            captured = b;
            flushed  = true;
            return S_OK;
        });

        AssertSucceeded (store->MountFromBytes (6, 0, "m.woz", DiskFormat::Woz, woz));
        store->GetImage (6, 0)->WriteBit (0, flippedBit, 0);   // dirty guest write

        auto  ctrl = std::make_unique<Disk2Controller> (6);   // the controller and both stores are too big for the stack
        ctrl->GetSignals().SetMotorOffFlushCallback ([&] () { store->GetFlusher().FlushAll(); });

        ctrl->Write (0xC0E9, 0x00);    // motor on
        ctrl->Write (0xC0E8, 0x00);    // motor off (arm spindown)
        ctrl->Tick (1100000);         // past the spindown -> callback -> FlushAll

        Assert::IsTrue (flushed, L"motor spin-down must persist dirty images via the store");

        // The write must have made it into the persisted bytes.
        AssertSucceeded (store2->MountFromBytes (6, 0, "m.woz", DiskFormat::Woz, captured));
        Assert::AreEqual (Byte (0), store2->GetImage (6, 0)->ReadBit (0, flippedBit),
            L"the write persisted at motor-off must survive reload");
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
        AssertSucceeded (store.GetFlusher().Flush (kSlot, kDrive));

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

        hr = store.GetFlusher().SetImageWriteProtect (kSlot, kDrive, true);

        Assert::IsTrue (FAILED (hr), L"the toggle must refuse a damaged image");
        Assert::AreEqual (0, sinkCalls, L"and must not write anything on the way to refusing");
        Assert::IsTrue (file == original, L"so the file is byte-for-byte untouched");
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
        vector<Byte>      sectors (NibbleFormat::kImageByteSize, 0x11);
        vector<Byte>      file;
        DiskImage       * img = nullptr;
        WriteProtectInfo  info;

        AssertSucceeded (Nibblizer::NibblizeDsk (sectors, built));
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



};

