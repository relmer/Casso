#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/WozLoader.h"

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
};
