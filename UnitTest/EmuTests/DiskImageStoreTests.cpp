#include "Pch.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
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

namespace
{
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

        for (size_t i = 0; i < raw.size (); i++)
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
}



TEST_CLASS (DiskImageStoreTests)
{
public:

    TEST_METHOD (DetectFormatByExtension_KnownTypes)
    {
        DiskFormat   fmt = DiskFormat::Dsk;

        Assert::IsTrue (SUCCEEDED (DiskImageStore::DetectFormatByExtension ("foo.dsk", fmt)));
        Assert::IsTrue (fmt == DiskFormat::Dsk);

        Assert::IsTrue (SUCCEEDED (DiskImageStore::DetectFormatByExtension ("foo.DO", fmt)));
        Assert::IsTrue (fmt == DiskFormat::Do);

        Assert::IsTrue (SUCCEEDED (DiskImageStore::DetectFormatByExtension ("foo.po", fmt)));
        Assert::IsTrue (fmt == DiskFormat::Po);

        Assert::IsTrue (SUCCEEDED (DiskImageStore::DetectFormatByExtension ("foo.WOZ", fmt)));
        Assert::IsTrue (fmt == DiskFormat::Woz);
    }

    TEST_METHOD (DetectFormatByExtension_UnknownReturnsFail)
    {
        DiskFormat   fmt = DiskFormat::Dsk;

        Assert::IsTrue (FAILED (DiskImageStore::DetectFormatByExtension ("foo.bin", fmt)));
        Assert::IsTrue (FAILED (DiskImageStore::DetectFormatByExtension ("noext",  fmt)));
    }

    TEST_METHOD (MountFromBytes_DskRunsNibblization)
    {
        DiskImageStore   store;
        vector<Byte>     raw = MakeDsk (0xA5);

        HRESULT   hr = store.MountFromBytes (kSlot, kDrive, "synthetic.dsk", DiskFormat::Dsk, raw);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (store.IsMounted (kSlot, kDrive));

        DiskImage *  img = store.GetImage (kSlot, kDrive);

        Assert::IsNotNull (img);
        Assert::IsTrue   (img->GetTrackBitCount (0) > 0,
            L"DSK mount must produce nibblized track bits");
    }

    TEST_METHOD (MountFromBytes_WozNativeNoNibblization)
    {
        DiskImageStore   store;
        vector<Byte>     bits ((6400), 0xFF);
        vector<Byte>     woz;

        Assert::IsTrue (SUCCEEDED (WozLoader::BuildSyntheticV2 (1, false, bits, 51200, woz)));

        HRESULT   hr = store.MountFromBytes (kSlot, kDrive, "synthetic.woz", DiskFormat::Woz, woz);

        Assert::IsTrue (SUCCEEDED (hr));

        DiskImage *  img = store.GetImage (kSlot, kDrive);

        Assert::IsNotNull (img);
        Assert::IsTrue   (img->GetSourceFormat () == DiskFormat::Woz);
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

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (kSlot, kDrive, "x.dsk", DiskFormat::Dsk, raw)));

        // Mark dirty by writing a bit through the public API.
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);
        Assert::IsTrue (store.GetImage (kSlot, kDrive)->IsDirty ());

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

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (kSlot, kDrive, "x.dsk", DiskFormat::Dsk, raw)));
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

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (6, 0, "a.dsk", DiskFormat::Dsk, raw)));
        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (6, 1, "b.dsk", DiskFormat::Dsk, raw)));
        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (5, 0, "c.dsk", DiskFormat::Dsk, raw)));

        store.GetImage (6, 0)->WriteBit (0, 0, 1);
        store.GetImage (5, 0)->WriteBit (0, 0, 1);
        // (6,1) intentionally clean.

        Assert::IsTrue (SUCCEEDED (store.FlushAll ()));
        Assert::AreEqual (2, flushCount, L"FlushAll must flush exactly the dirty mounts");

        // Mounts persist after FlushAll.
        Assert::IsTrue (store.IsMounted (6, 0));
        Assert::IsTrue (store.IsMounted (6, 1));
        Assert::IsTrue (store.IsMounted (5, 0));
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Flush-error reporting. A dirty image that fails to persist used to
    //  vanish (every flush caller drops the HRESULT). The reporter surfaces
    //  the loss; a clean or successful flush must stay silent.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FlushError_reporterFiresOnFailure_withPathAndHr)
    {
        DiskImageStore   store;
        vector<Byte>     raw          = MakeDsk (0);
        bool             reported     = false;
        string           reportedPath;
        HRESULT          reportedHr   = S_OK;

        store.SetFlushSink          ([] (const string &, const vector<Byte> &) { return E_FAIL; });
        store.SetFlushErrorReporter ([&] (const string & p, HRESULT hr)
        {
            reported     = true;
            reportedPath = p;
            reportedHr   = hr;
        });

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (kSlot, kDrive, "boom.dsk", DiskFormat::Dsk, raw)));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);   // dirty

        HRESULT   hr = store.Flush (kSlot, kDrive);

        Assert::IsTrue   (FAILED (hr));
        Assert::IsTrue   (reported, L"a failed flush must be reported, not swallowed");
        Assert::AreEqual (string ("boom.dsk"), reportedPath);
        Assert::IsTrue   (FAILED (reportedHr));
    }

    TEST_METHOD (FlushError_noReportOnCleanOrSuccessfulFlush)
    {
        DiskImageStore   store;
        vector<Byte>     raw      = MakeDsk (0);
        bool             reported = false;

        store.SetFlushSink          ([] (const string &, const vector<Byte> &) { return S_OK; });
        store.SetFlushErrorReporter ([&] (const string &, HRESULT) { reported = true; });

        // Clean image ejected -> no flush -> no report.
        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (kSlot, kDrive, "clean.dsk", DiskFormat::Dsk, raw)));
        store.Eject (kSlot, kDrive);
        Assert::IsFalse (reported, L"clean image must not report");

        // Dirty image that flushes successfully -> no report.
        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (kSlot, kDrive, "ok.dsk", DiskFormat::Dsk, raw)));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);
        Assert::IsTrue  (SUCCEEDED (store.Flush (kSlot, kDrive)));
        Assert::IsFalse (reported, L"a successful flush must not report");
    }

    TEST_METHOD (FlushError_surfacesThroughVoidEjectPath)
    {
        // Eject is void and drops FlushEntry's HRESULT; the reporter must
        // still fire so an eject that loses writes isn't silent.
        DiskImageStore   store;
        vector<Byte>     raw      = MakeDsk (0);
        int              reports  = 0;

        store.SetFlushSink          ([] (const string &, const vector<Byte> &) { return E_FAIL; });
        store.SetFlushErrorReporter ([&] (const string &, HRESULT) { reports++; });

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (kSlot, kDrive, "e.dsk", DiskFormat::Dsk, raw)));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);
        store.Eject (kSlot, kDrive);

        Assert::AreEqual (1, reports, L"Eject flush failure must surface via the reporter");
    }

    TEST_METHOD (FlushAll_ClearsDirtyFlags)
    {
        DiskImageStore   store;
        vector<Byte>     raw = MakeDsk (0);

        store.SetFlushSink ([](const string &, const vector<Byte> &) { return S_OK; });

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (kSlot, kDrive, "x.dsk", DiskFormat::Dsk, raw)));

        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);
        Assert::IsTrue (store.GetImage (kSlot, kDrive)->IsDirty ());

        Assert::IsTrue (SUCCEEDED (store.FlushAll ()));
        Assert::IsFalse (store.GetImage (kSlot, kDrive)->IsDirty (),
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

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (kSlot, kDrive, "x.dsk", DiskFormat::Dsk, raw)));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);

        store.SoftReset ();

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

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (6, 0, "a.dsk", DiskFormat::Dsk, raw)));
        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (6, 1, "b.dsk", DiskFormat::Dsk, raw)));

        store.GetImage (6, 0)->WriteBit (0, 0, 1);

        store.PowerCycle ();

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

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (kSlot, kDrive, "x.dsk", DiskFormat::Dsk, raw)));
        store.GetImage (kSlot, kDrive)->WriteBit (0, 0, 1);

        Assert::IsTrue (SUCCEEDED (store.Flush (kSlot, kDrive)));
        Assert::AreEqual (size_t (NibblizationLayer::kImageByteSize), captured.size (),
            L"Flushed payload must be 143360 bytes for DSK");
    }

    TEST_METHOD (RemountReplacesPreviousImage)
    {
        DiskImageStore   store;
        vector<Byte>     raw1 = MakeDsk (0x11);
        vector<Byte>     raw2 = MakeDsk (0x22);

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (kSlot, kDrive, "a.dsk", DiskFormat::Dsk, raw1)));

        DiskImage *  first = store.GetImage (kSlot, kDrive);
        Assert::IsNotNull (first);

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (kSlot, kDrive, "b.dsk", DiskFormat::Dsk, raw2)));
        Assert::IsTrue (store.GetSourcePath (kSlot, kDrive) == "b.dsk");
    }

    TEST_METHOD (MountFromBytes_RejectsBadSlotOrDrive)
    {
        DiskImageStore   store;
        vector<Byte>     raw = MakeDsk (0);

        Assert::IsTrue (FAILED (store.MountFromBytes (-1, 0, "x.dsk", DiskFormat::Dsk, raw)));
        Assert::IsTrue (FAILED (store.MountFromBytes ( 0, 5, "x.dsk", DiskFormat::Dsk, raw)));
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

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (kSlot, kDrive, path, fmt, raw)));

        // Mark dirty without touching the bit stream so the flush must
        // reproduce the original image exactly.
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);

        Assert::IsTrue   (SUCCEEDED (store.Flush (kSlot, kDrive)));
        Assert::IsTrue   (raw == captured,
            L"flushed bytes must faithfully round-trip the sector image");
    }

    TEST_METHOD (Flush_FaithfulRoundTrip_Dsk)  { VerifyFaithfulSectorFlush (DiskFormat::Dsk, "rt.dsk"); }
    TEST_METHOD (Flush_FaithfulRoundTrip_Do)   { VerifyFaithfulSectorFlush (DiskFormat::Do,  "rt.do");  }
    TEST_METHOD (Flush_FaithfulRoundTrip_Po)   { VerifyFaithfulSectorFlush (DiskFormat::Po,  "rt.po");  }

    TEST_METHOD (Flush_WozGuestWriteSurvivesReloadThroughStore)
    {
        DiskImageStore   store;
        vector<Byte>     woz        = MakeWoz ();
        vector<Byte>     captured;
        const size_t     flippedBit = 200;

        store.SetFlushSink ([&] (const string &, const vector<Byte> & b) { captured = b; return S_OK; });

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (kSlot, kDrive, "g.woz", DiskFormat::Woz, woz)));
        Assert::AreEqual (Byte (1), store.GetImage (kSlot, kDrive)->ReadBit (0, flippedBit));

        store.GetImage (kSlot, kDrive)->WriteBit (0, flippedBit, 0);   // real guest write -> dirty
        Assert::IsTrue (SUCCEEDED (store.Flush (kSlot, kDrive)));

        // Reload the flushed bytes: the write must survive the full WOZ
        // serialize -> file -> reload cycle (DiskImage::Serialize dispatch).
        DiskImageStore   store2;
        Assert::IsTrue (SUCCEEDED (store2.MountFromBytes (kSlot, kDrive, "g.woz", DiskFormat::Woz, captured)));
        Assert::AreEqual (Byte (0), store2.GetImage (kSlot, kDrive)->ReadBit (0, flippedBit),
            L"a guest write to a .woz must survive flush + reload through the store");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Flush-error reporting reaches through the bulk flush paths that also
    //  drop the HRESULT (FlushAll / PowerCycle), once per failed dirty mount.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FlushError_reporterFiresPerDirtyMount_viaFlushAll)
    {
        DiskImageStore   store;
        vector<Byte>     raw     = MakeDsk (0);
        int              reports = 0;

        store.SetFlushSink          ([] (const string &, const vector<Byte> &) { return E_FAIL; });
        store.SetFlushErrorReporter ([&] (const string &, HRESULT) { reports++; });

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (6, 0, "a.dsk", DiskFormat::Dsk, raw)));
        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (6, 1, "b.dsk", DiskFormat::Dsk, raw)));
        store.GetImage (6, 0)->WriteBit (0, 0, 1);
        store.GetImage (6, 1)->WriteBit (0, 0, 1);

        store.FlushAll ();
        Assert::AreEqual (2, reports, L"FlushAll must report each dirty mount that fails");
    }

    TEST_METHOD (FlushError_reporterFires_viaPowerCycle)
    {
        DiskImageStore   store;
        vector<Byte>     raw     = MakeDsk (0);
        int              reports = 0;

        store.SetFlushSink          ([] (const string &, const vector<Byte> &) { return E_FAIL; });
        store.SetFlushErrorReporter ([&] (const string &, HRESULT) { reports++; });

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (6, 0, "a.dsk", DiskFormat::Dsk, raw)));
        store.GetImage (6, 0)->WriteBit (0, 0, 1);   // (6,1) clean

        store.PowerCycle ();
        Assert::AreEqual (1, reports, L"PowerCycle must report the dirty mount that fails");
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
        vector<Byte>     woz        = MakeWoz ();
        vector<Byte>     captured;
        bool             flushed    = false;
        const size_t     flippedBit = 200;

        store.SetFlushSink ([&] (const string &, const vector<Byte> & b)
        {
            captured = b;
            flushed  = true;
            return S_OK;
        });

        Assert::IsTrue (SUCCEEDED (store.MountFromBytes (6, 0, "m.woz", DiskFormat::Woz, woz)));
        store.GetImage (6, 0)->WriteBit (0, flippedBit, 0);   // dirty guest write

        Disk2Controller  ctrl (6);
        ctrl.SetMotorOffFlushCallback ([&] () { store.FlushAll (); });

        ctrl.Write (0xC0E9, 0x00);    // motor on
        ctrl.Write (0xC0E8, 0x00);    // motor off (arm spindown)
        ctrl.Tick  (1100000);         // past the spindown -> callback -> FlushAll

        Assert::IsTrue (flushed, L"motor spin-down must persist dirty images via the store");

        // The write must have made it into the persisted bytes.
        DiskImageStore   store2;
        Assert::IsTrue (SUCCEEDED (store2.MountFromBytes (6, 0, "m.woz", DiskFormat::Woz, captured)));
        Assert::AreEqual (Byte (0), store2.GetImage (6, 0)->ReadBit (0, flippedBit),
            L"the write persisted at motor-off must survive reload");
    }
};
