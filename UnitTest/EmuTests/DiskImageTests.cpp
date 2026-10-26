#include "Pch.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageTests
//
//  Phase 9 acceptance for the bit-stream IDiskImage container. WOZ /
//  .DSK / .DO / .PO loader coverage moves to Phase 10.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskImageTests)
{
public:

    TEST_METHOD (DefaultTrackCountIs35)
    {
        DiskImage   img;

        Assert::AreEqual (DiskImage::kDefaultTrackCount, img.GetTrackCount(),
            L"Default DiskImage exposes 35 tracks");
    }

    TEST_METHOD (ResizeTrackSetsBitCount)
    {
        DiskImage   img;

        img.ResizeTrack (0, 1024);

        Assert::AreEqual (size_t (1024), img.GetTrackBitCount (0),
            L"ResizeTrack must persist the requested bit count");
    }

    TEST_METHOD (ReadBitWriteBitRoundTrip)
    {
        DiskImage   img;

        img.ResizeTrack (0, 16);

        img.WriteBit (0, 0,  1);
        img.WriteBit (0, 7,  1);
        img.WriteBit (0, 15, 1);

        Assert::AreEqual (uint8_t (1), img.ReadBit (0, 0));
        Assert::AreEqual (uint8_t (1), img.ReadBit (0, 7));
        Assert::AreEqual (uint8_t (1), img.ReadBit (0, 15));
        Assert::AreEqual (uint8_t (0), img.ReadBit (0, 1));
        Assert::AreEqual (uint8_t (0), img.ReadBit (0, 8));
    }

    TEST_METHOD (DirtyFlagSetByWrite)
    {
        DiskImage   img;

        img.ResizeTrack (0, 8);
        Assert::IsFalse (img.IsDirty(), L"Fresh image must be clean");

        img.WriteBit (0, 0, 1);
        Assert::IsTrue  (img.IsDirty(), L"WriteBit must mark image dirty");
        Assert::IsTrue  (img.IsTrackDirty (0),
            L"WriteBit must mark track dirty");
    }

    TEST_METHOD (WriteProtectBlocksWriteBit)
    {
        DiskImage   img;

        img.ResizeTrack (0, 8);
        img.SetWriteProtected (true);
        img.WriteBit (0, 0, 1);

        Assert::AreEqual (uint8_t (0), img.ReadBit (0, 0),
            L"Write-protected disk must reject WriteBit");
        Assert::IsFalse (img.IsDirty(),
            L"Write-protected disk must remain clean");
    }

    TEST_METHOD (WriteProtectSourcesAreIndependent)
    {
        DiskImage   img;

        Assert::IsFalse (img.IsWriteProtected(), L"Fresh image is writable");

        img.SetImageWriteProtected (true);
        Assert::IsTrue  (img.IsWriteProtected());
        Assert::IsTrue  (img.GetWriteProtectInfo().imageFlag);
        Assert::IsFalse (img.GetWriteProtectInfo().userSetting);

        img.SetImageWriteProtected (false);
        img.SetUserWriteProtected  (true);
        Assert::IsTrue  (img.IsWriteProtected(),
            L"User setting alone protects the disk");
        Assert::IsTrue  (img.GetWriteProtectInfo().userSetting);
        Assert::IsFalse (img.GetWriteProtectInfo().imageFlag);

        img.SetUserWriteProtected (false);
        img.SetFileWriteProtect   (true, false);
        Assert::IsTrue  (img.IsWriteProtected(),
            L"Read-only backing file alone protects the disk");
        Assert::IsTrue  (img.GetWriteProtectInfo().readOnlyFile);
        Assert::IsFalse (img.GetWriteProtectInfo().noPermission);

        img.SetFileWriteProtect (false, true);
        Assert::IsTrue  (img.IsWriteProtected(),
            L"No-permission backing file alone protects the disk");
        Assert::IsTrue  (img.GetWriteProtectInfo().noPermission);

        img.SetFileWriteProtect (false, false);
        Assert::IsFalse (img.IsWriteProtected(),
            L"Clearing every source makes the disk writable again");
    }

    TEST_METHOD (WriteProtectInfoReportsAllActiveSources)
    {
        DiskImage         img;
        WriteProtectInfo  info;

        img.SetImageWriteProtected (true);
        img.SetUserWriteProtected  (true);
        info = img.GetWriteProtectInfo();

        Assert::IsTrue (info.imageFlag);
        Assert::IsTrue (info.userSetting);
        Assert::IsTrue (info.Any());
    }

    TEST_METHOD (UserWriteProtectBlocksWriteBit)
    {
        DiskImage   img;

        img.ResizeTrack (0, 8);
        img.SetUserWriteProtected (true);
        img.WriteBit (0, 0, 1);

        Assert::AreEqual (uint8_t (0), img.ReadBit (0, 0),
            L"A user-write-protected disk must reject WriteBit");
        Assert::IsFalse (img.IsDirty());
    }

    TEST_METHOD (DefaultSourceFormatIsDsk)
    {
        DiskImage   img;

        Assert::IsTrue (img.GetSourceFormat() == DiskFormat::Dsk,
            L"Default DiskImage reports DSK");
    }

    TEST_METHOD (SetSourceFormatPersists)
    {
        DiskImage   img;

        img.SetSourceFormat (DiskFormat::Woz);
        Assert::IsTrue (img.GetSourceFormat() == DiskFormat::Woz);
    }

    TEST_METHOD (BitIndexWrapsAtTrackEnd)
    {
        DiskImage   img;

        img.ResizeTrack (0, 16);
        img.WriteBit (0, 0, 1);

        // Reading at bitIndex=16 must wrap to bitIndex 0.
        Assert::AreEqual (uint8_t (1), img.ReadBit (0, 16),
            L"Bit index past track end must wrap modulo trackBitCount");
    }

    TEST_METHOD (LoadMissingFileFails)
    {
        DiskImage   img;

        HRESULT   hr = img.Load ("does_not_exist_anywhere.dsk");

        AssertFailed (hr);
        Assert::IsFalse (img.IsLoaded());
    }

    TEST_METHOD (SerializeReturnsBytesAfterPhase10NibblizationLayer)
    {
        DiskImage         img;
        vector<Byte>      bytes;

        HRESULT   hr = img.Serialize (bytes);

        AssertSucceeded (hr,
            L"Phase 10: Serialize routes DSK through NibblizationLayer::Denibblize");
        Assert::AreEqual (size_t (143360), bytes.size(),
            L"Phase 10: a default 35-track DSK serializes to 143360 bytes");
    }


    //  A WOZ of `count` 512-bit tracks, mapped at 4N to 4N+3, or at 4N-1, 4N
    //  and 4N+1 as Applesauce maps them, loaded into `img`.
    static void LoadWoz (DiskImage & img, int count, bool isApplesauce)
    {
        vector<WozSyntheticTrack>  tracks (count);
        vector<Byte>               woz;
        int                        n      = 0;



        for (n = 0; n < count; n++)
        {
            tracks[n].data.assign (64, 0xFF);
            tracks[n].bitCount      = 512;
            tracks[n].quarterTracks = isApplesauce ? vector<int> { max (4 * n - 1, 0), 4 * n, 4 * n + 1 }
                                                   : vector<int> { 4 * n, 4 * n + 1, 4 * n + 2, 4 * n + 3 };
        }

        AssertSucceeded (WozLoader::BuildSyntheticV21 (tracks, woz));

        img.LoadFromBytes (DiskFormat::Woz, woz, "created.woz");
        Assert::IsTrue (img.IsLoaded(), L"the WOZ loads");
    }


    TEST_METHOD (TryCreateTrack_OnAReservedBlankTrack_SizesItWithoutMarkingDirty)
    {
        DiskImage  img;
        uint64_t   generation = 0;



        LoadWoz (img, 35, false);
        generation = img.GetLayoutGeneration();

        Assert::IsTrue   (img.TryCreateTrack (4 * 36, 51200), L"track 36 is created");
        Assert::AreEqual ((size_t) 51200, img.GetTrackBitCount (36), L"with 51,200 bits");
        Assert::IsFalse  (img.IsDirty(), L"and nothing to save until the guest writes");
        Assert::AreNotEqual (generation, img.GetLayoutGeneration(), L"and the layout changes");
    }


    TEST_METHOD (TryCreateTrack_DeclinesEveryOtherCase)
    {
        DiskImage                  protectedWoz;
        DiskImage                  unloaded;
        DiskImage                  dsk;
        DiskImage                  flux;
        DiskImage                  woz;
        DiskImage                  applesauce;
        vector<WozSyntheticTrack>  fluxTracks (1);
        vector<Byte>               fluxBytes;



        LoadWoz (protectedWoz, 35, false);
        protectedWoz.SetUserWriteProtected (true);
        Assert::IsFalse (protectedWoz.TryCreateTrack (4 * 36, 51200), L"a write-protected disk");

        Assert::IsFalse (unloaded.TryCreateTrack (0, 51200), L"a disk that is not loaded");

        dsk.LoadFromBytes (DiskFormat::Dsk, vector<Byte> (143360, 0), "sectors.dsk");
        Assert::IsFalse (dsk.TryCreateTrack (4 * 37, 51200), L"track 37 of a sector image");

        fluxTracks[0].isFlux        = true;
        fluxTracks[0].quarterTracks = { 0 };
        AssertSucceeded (WozLoader::BuildSyntheticV21 (fluxTracks, fluxBytes));
        flux.LoadFromBytes (DiskFormat::Woz, fluxBytes, "flux.woz");
        Assert::IsFalse (flux.TryCreateTrack (0, 51200), L"a flux slot");

        LoadWoz (woz, 35, false);
        Assert::IsFalse (woz.TryCreateTrack (0, 51200), L"a slot that has data");

        LoadWoz (applesauce, 35, true);
        Assert::IsFalse (applesauce.TryCreateTrack (2, 51200), L"a position with no mapping");
    }
};
