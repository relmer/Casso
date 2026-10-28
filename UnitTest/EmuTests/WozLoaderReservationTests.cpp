#include "Pch.h"
#include "WozLoaderTestSupport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WozLoaderReservationTests
//
//  Empty track records and the bit storage reserved at load for the tracks a
//  guest can create, and how both serialize.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WozLoaderReservationTests), public WozLoaderTestSupport
{
public:

    //  An unused TRK record, all zeros, is an empty track, not damage, and so
    //  is the zero-count record BuildSyntheticV2 writes for a 0-bit track, at
    //  start block 3: the image is undamaged and writable.
    TEST_METHOD (Load_EmptyBitRecord_IsAnEmptyWritableSlot)
    {
        for (bool isAllZero : { true, false })
        {
            DiskImage     img;
            vector<Byte>  woz;
            size_t        recordsOffset = 0;



            AssertSucceeded (WozLoader::BuildSyntheticV2 (1, false, vector<Byte>(), 0, woz));

            Assert::IsTrue   (FindChunk (woz, "TRKS", recordsOffset) != 0, L"the image has a TRKS chunk");
            Assert::AreEqual ((Byte) 3, woz[recordsOffset],     L"the record as written starts at block 3");
            Assert::AreEqual ((Byte) 1, woz[recordsOffset + 2], L"and is one block long");

            if (isAllZero)
            {
                ZeroTrackRecord (woz, 0);
            }

            AssertSucceeded (WozLoader::Load (woz, img), L"the image loads");

            Assert::IsFalse  (img.IsDamaged(), L"the image is not damaged");
            Assert::IsFalse  (img.IsWriteProtected(), L"and is writable");
            Assert::IsTrue   (img.GetTrackKind (0) == TrackKind::Bits, L"the record is a bit slot");
            Assert::AreEqual ((size_t) 0, img.GetTrackBitCount (0), L"with no bits");
        }
    }


    //  An all-zero record a FLUX entry maps is an empty flux track, not
    //  damage.
    TEST_METHOD (Load_AllZeroFluxRecord_IsAnEmptyWritableFluxTrack)
    {
        DiskImage                  img;
        vector<WozSyntheticTrack>  tracks (1);
        vector<Byte>               woz;



        tracks[0].isFlux        = true;
        tracks[0].quarterTracks = { 0 };

        AssertSucceeded (WozLoader::BuildSyntheticV21 (tracks, woz));
        ZeroTrackRecord (woz, 0);

        AssertSucceeded (WozLoader::Load (woz, img), L"the image loads");

        Assert::IsFalse (img.IsDamaged(), L"the image is not damaged");
        Assert::IsFalse (img.IsWriteProtected(), L"and is writable");
        Assert::IsTrue  (img.GetMappedSlot (0) >= 0, L"quarter track 0 is mapped");
        Assert::IsTrue  (img.GetTrackKind (img.GetMappedSlot (0)) == TrackKind::Flux, L"to a flux track");
    }


    //  A WOZ from Applesauce has 35 tracks: tracks 35 to 39 are reserved at
    //  TRK index N, at quarter tracks 4N to 4N+3, with no data.
    TEST_METHOD (Load_BlankWholeTracks_ReservedAtTheirOwnIndex)
    {
        DiskImage     img;
        vector<Byte>  woz;
        int           n   = 0;
        int           k   = 0;



        AssertSucceeded (WozLoader::BuildSyntheticV21 (MakeTracks (35, true), woz));
        AssertSucceeded (WozLoader::Load (woz, img));

        Assert::AreEqual (40, img.GetTrackCount(), L"a slot for each of tracks 35 to 39");

        for (n = 35; n < 40; n++)
        {
            for (k = 0; k < 4; k++)
            {
                Assert::AreEqual (n, img.GetMappedSlot (4 * n + k), L"quarter tracks 4N to 4N+3 map to slot N");
            }

            Assert::AreEqual (-1, img.ResolveQuarterTrack (4 * n), L"which has no data");
        }

        for (n = 1; n < 35; n++)
        {
            Assert::AreEqual (-1, img.GetMappedSlot (4 * n + 2), L"a mapped track's 4N+2 is as loaded");
        }
    }


    TEST_METHOD (Load_BlankTrackWhoseIndexIsTaken_TakesTheLowestFreeIndex)
    {
        DiskImage     img;
        vector<Byte>  woz;
        int           k   = 0;



        AssertSucceeded (WozLoader::BuildSyntheticV21 (MakeTracksWithIndexTaken(), woz));
        AssertSucceeded (WozLoader::Load (woz, img));

        for (k = 0; k < 4; k++)
        {
            Assert::AreEqual (5, img.GetMappedSlot (80 + k), L"track 20 takes index 5");
        }

        Assert::AreEqual (20, img.GetMappedSlot (20), L"track 5 is on record 20");
        Assert::AreEqual (21, img.GetMappedSlot (84), L"and track 21 takes its own index");
        Assert::AreEqual (40, img.GetTrackCount());
    }


    //  An empty bit record, all zeros or as BuildSyntheticV2 writes it for 0
    //  bits, is unmapped and then reserved like a track with no record.
    TEST_METHOD (Load_EmptyBitRecord_IsReservedLikeAnUnmappedTrack)
    {
        for (bool isAllZero : { true, false })
        {
            DiskImage     img;
            vector<Byte>  woz;
            int           k   = 0;



            AssertSucceeded (WozLoader::BuildSyntheticV2 (1, false, vector<Byte>(), 0, woz));

            if (isAllZero)
            {
                ZeroTrackRecord (woz, 0);
            }

            AssertSucceeded (WozLoader::Load (woz, img));

            for (k = 0; k < 4; k++)
            {
                Assert::AreEqual (0, img.GetMappedSlot (k), L"quarter tracks 0 to 3 map to the reserved slot 0");
            }

            Assert::AreEqual (-1, img.ResolveQuarterTrack (0), L"which has no data");
            Assert::AreEqual (40, img.GetTrackCount(), L"and every whole track has a slot");
        }
    }


    //  A damaged image is write-protected, so no track can be created on it,
    //  and its map is exactly as loaded.
    TEST_METHOD (Load_DamagedImage_KeepsItsMapAndReservesNothing)
    {
        DiskImage                  img;
        vector<WozSyntheticTrack>  tracks        = MakeTracks (2, false);
        vector<Byte>               woz;
        size_t                     recordsOffset = 0;
        int                        qt            = 0;



        tracks[1].data.clear();
        tracks[1].bitCount = 0;

        AssertSucceeded (WozLoader::BuildSyntheticV21 (tracks, woz));

        ZeroTrackRecord (woz, 1);

        //  Record 0's blocks lie past the end of the file.
        Assert::IsTrue (FindChunk (woz, "TRKS", recordsOffset) != 0);
        woz[recordsOffset]     = 0xFF;
        woz[recordsOffset + 1] = 0x7F;

        AssertSucceeded (WozLoader::Load (woz, img));

        Assert::IsTrue   (img.IsDamaged(), L"the image is damaged");
        Assert::IsTrue   (img.IsWriteProtected(), L"and write-protected");
        Assert::AreEqual (DiskImage::kDefaultTrackCount, img.GetTrackCount(), L"with no slot added");

        for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
        {
            Assert::AreEqual (qt < 8 ? qt / 4 : -1, img.GetMappedSlot (qt), L"and every map entry as loaded");
        }
    }


    TEST_METHOD (Load_ReusedImage_EmptiesReservedSlots)
    {
        DiskImage     img;
        vector<Byte>  full;
        vector<Byte>  shorter;
        int           n       = 0;



        AssertSucceeded (WozLoader::BuildSyntheticV21 (MakeTracks (40, false), full));
        AssertSucceeded (WozLoader::BuildSyntheticV21 (MakeTracks (35, false), shorter));

        AssertSucceeded (WozLoader::Load (full, img));
        AssertSucceeded (WozLoader::Load (shorter, img));

        for (n = 35; n < 40; n++)
        {
            Assert::AreEqual (n, img.GetMappedSlot (4 * n), L"tracks 35 to 39 are reserved");
            Assert::AreEqual ((size_t) 0, img.GetTrackBitCount (n), L"with the bits of the earlier load gone");
        }
    }


    //  The reservation writes nothing: an image saved and loaded again saves
    //  the same TMAP and TRKS.
    TEST_METHOD (Serialize_UnwrittenImage_IsUnchangedByTheReservation)
    {
        DiskImage     first;
        DiskImage     second;
        vector<Byte>  woz;
        vector<Byte>  saved;
        vector<Byte>  savedAgain;
        vector<Byte>  tmap;
        int           qt         = 0;



        AssertSucceeded (WozLoader::BuildSyntheticV21 (MakeTracks (35, false), woz));
        AssertSucceeded (WozLoader::Load (woz, first));
        AssertSucceeded (WozLoader::Serialize (first, saved));
        AssertSucceeded (WozLoader::Load (saved, second));
        AssertSucceeded (WozLoader::Serialize (second, savedAgain));

        tmap = GetChunkBytes (saved, "TMAP");

        Assert::IsTrue (tmap == GetChunkBytes (savedAgain, "TMAP"), L"the TMAP is the same");
        Assert::IsTrue (GetChunkBytes (saved, "TRKS") == GetChunkBytes (savedAgain, "TRKS"), L"and so is the TRKS");

        for (qt = 140; qt < DiskImage::kQuarterTrackCount; qt++)
        {
            Assert::AreEqual ((Byte) 0xFF, tmap[qt], L"with no entry for a reserved track");
        }
    }


    //  A track the guest created is saved, and the loaded file has the same
    //  slots and map: at the track's index, at the lowest free index and over
    //  an empty bit record.
    TEST_METHOD (Serialize_ReservationSurvivesARoundTrip)
    {
        struct Case
        {
            vector<Byte>  woz;
            int           track = 0;
            int           slot  = 0;
        };

        vector<Case>  cases (3);
        int           qt    = 0;



        AssertSucceeded (WozLoader::BuildSyntheticV21 (MakeTracks (35, false), cases[0].woz));
        cases[0].track = 36;
        cases[0].slot  = 36;

        AssertSucceeded (WozLoader::BuildSyntheticV21 (MakeTracksWithIndexTaken(), cases[1].woz));
        cases[1].track = 20;
        cases[1].slot  = 5;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (1, false, vector<Byte>(), 0, cases[2].woz));
        ZeroTrackRecord (cases[2].woz, 0);
        cases[2].track = 0;
        cases[2].slot  = 0;

        for (const Case & c : cases)
        {
            DiskImage     img;
            DiskImage     reloaded;
            vector<Byte>  saved;



            img.LoadFromBytes (DiskFormat::Woz, c.woz, "created.woz");

            Assert::IsTrue (img.TryCreateTrack (4 * c.track, 51200), L"the track is created");
            img.WriteBit (c.slot, 0, 1);

            AssertSucceeded (WozLoader::Serialize (img, saved));
            reloaded.LoadFromBytes (DiskFormat::Woz, saved, "created.woz");

            Assert::AreEqual (img.GetTrackCount(), reloaded.GetTrackCount(), L"the slot count is the same");

            for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
            {
                Assert::AreEqual (img.GetMappedSlot (qt), reloaded.GetMappedSlot (qt), L"and so is every map entry");
            }

            Assert::AreEqual (c.slot, reloaded.ResolveQuarterTrack (4 * c.track), L"the created track has data");
            Assert::AreEqual ((uint8_t) 1, reloaded.ReadBit (c.slot, 0), L"with the guest's write");
        }
    }
};
