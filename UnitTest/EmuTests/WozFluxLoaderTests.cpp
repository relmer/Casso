#include "Pch.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "FluxTestImages.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WozFluxLoaderTests
//
//  Reading the WOZ 2.1 FLUX chunk: which quarter tracks become flux tracks,
//  FLUX winning over TMAP, the INFO fields being ignored on read, and the one
//  FLUX chunk that cannot be used at all.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WozFluxLoaderTests)
{
public:

    static constexpr size_t  kBitCount = 51200;

    static WozSyntheticTrack MakeBitTrack (vector<int> quarterTracks)
    {
        WozSyntheticTrack  track;

        track.data.assign ((kBitCount + 7) / 8, 0xFF);
        track.bitCount      = kBitCount;
        track.quarterTracks = quarterTracks;
        return track;
    }


    static WozSyntheticTrack MakeFluxTrack (vector<int> quarterTracks)
    {
        WozSyntheticTrack  track;
        vector<Byte>       bits ((kBitCount + 7) / 8, 0xFF);

        track.data          = FluxTestImages::BitsToNominalFlux (bits, kBitCount);
        track.isFlux        = true;
        track.quarterTracks = quarterTracks;
        return track;
    }


    static size_t FindChunk (const vector<Byte> & image, const char * id)
    {
        size_t  at = 0;

        for (at = WozLoader::kHeaderSize; at + 4 <= image.size(); at++)
        {
            if (memcmp (image.data() + at, id, 4) == 0)
            {
                return at;
            }
        }

        return 0;
    }


    TEST_METHOD (FluxQuarterTracksBecomeFluxSlots)
    {
        vector<Byte>  image;
        DiskImage     disk;
        HRESULT       hr = S_OK;

        hr = WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 0, 1 }), MakeFluxTrack ({ 6 }) }, image);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = WozLoader::Load (image, disk);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::AreEqual (0, disk.ResolveQuarterTrack (0));
        Assert::AreEqual (1, disk.ResolveQuarterTrack (6));
        Assert::IsTrue   (disk.GetTrackKind (0) == TrackKind::Bits);
        Assert::IsTrue   (disk.GetTrackKind (1) == TrackKind::Flux);
        Assert::AreEqual (static_cast<size_t> (0), disk.GetTrackBitCount (1));
        Assert::IsTrue   (disk.GetFluxTrack (1).GetBytes() == MakeFluxTrack ({}).data);
        Assert::IsTrue   (disk.HasFluxTracks());
    }


    TEST_METHOD (FluxWinsOverTmapForTheSameQuarterTrack)
    {
        vector<Byte>  image;
        DiskImage     disk;
        HRESULT       hr = S_OK;

        hr = WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 4 }), MakeFluxTrack ({ 4 }) }, image);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = WozLoader::Load (image, disk);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::AreEqual (1, disk.ResolveQuarterTrack (4));
        Assert::IsTrue   (disk.GetTrackKind (1) == TrackKind::Flux);
    }


    TEST_METHOD (FluxIsReadWhateverInfoSays)
    {
        vector<Byte>  image;
        DiskImage     disk;
        HRESULT       hr   = S_OK;
        size_t        info = 0;

        hr = WozLoader::BuildSyntheticV21 ({ MakeFluxTrack ({ 0 }) }, image);
        Assert::IsTrue (SUCCEEDED (hr));

        info = FindChunk (image, "INFO") + 8;
        image[info + WozLoader::kInfoOffsetVersion]        = 2;
        image[info + WozLoader::kInfoOffsetFluxBlock]      = 0;
        image[info + WozLoader::kInfoOffsetFluxBlock + 1]  = 0;
        image[info + WozLoader::kInfoOffsetLargestFlux]    = 0;
        image[info + WozLoader::kInfoOffsetLargestFlux + 1] = 0;

        hr = WozLoader::Load (image, disk);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::AreEqual (0, disk.ResolveQuarterTrack (0));
        Assert::IsTrue   (disk.GetTrackKind (0) == TrackKind::Flux);
    }


    TEST_METHOD (ShortFluxChunkRefusesTheMount)
    {
        vector<Byte>  image;
        DiskImage     disk;
        HRESULT       hr   = S_OK;
        size_t        flux = 0;

        hr = WozLoader::BuildSyntheticV21 ({ MakeFluxTrack ({ 0 }) }, image);
        Assert::IsTrue (SUCCEEDED (hr));

        flux = FindChunk (image, "FLUX");
        Assert::IsTrue (flux > 0);
        image[flux + 4] = 100;

        hr = WozLoader::Load (image, disk);
        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (WozLoader::ClassifyLoadFailure (image) == MountFailure::MalformedWoz);
    }


    TEST_METHOD (NeighboringQuarterTracksShareTheFluxTrack)
    {
        vector<Byte>  image;
        DiskImage     disk;
        HRESULT       hr = S_OK;

        hr = WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 0 }), MakeFluxTrack ({ 5, 6, 7 }) }, image);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = WozLoader::Load (image, disk);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::AreEqual ( 1, disk.ResolveQuarterTrack (5));
        Assert::AreEqual ( 1, disk.ResolveQuarterTrack (6));
        Assert::AreEqual ( 1, disk.ResolveQuarterTrack (7));
        Assert::AreEqual (-1, disk.ResolveQuarterTrack (8));
    }


    TEST_METHOD (BitOnlyImageHasNoFluxSlots)
    {
        vector<Byte>  image;
        DiskImage     disk;
        HRESULT       hr = S_OK;
        int           s  = 0;

        hr = WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 0, 1 }), MakeBitTrack ({ 4 }) }, image);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (static_cast<size_t> (0), FindChunk (image, "FLUX"));

        hr = WozLoader::Load (image, disk);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::IsFalse  (disk.HasFluxTracks());
        Assert::AreEqual (kBitCount, disk.GetTrackBitCount (1));

        for (s = 0; s < disk.GetTrackCount(); s++)
        {
            Assert::IsTrue (disk.GetTrackKind (s) == TrackKind::Bits);
        }
    }


    TEST_METHOD (ReloadingABitImageClearsEarlierFluxSlots)
    {
        vector<Byte>  fluxImage;
        vector<Byte>  bitImage;
        DiskImage     disk;
        HRESULT       hr = S_OK;

        hr = WozLoader::BuildSyntheticV21 ({ MakeFluxTrack ({ 0 }) }, fluxImage);
        Assert::IsTrue (SUCCEEDED (hr));
        hr = WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 0 }) }, bitImage);
        Assert::IsTrue (SUCCEEDED (hr));

        disk.LoadFromBytes (DiskFormat::Woz, fluxImage, "");
        Assert::IsTrue (disk.HasFluxTracks());

        disk.LoadFromBytes (DiskFormat::Woz, bitImage, "");
        Assert::IsFalse (disk.HasFluxTracks());
    }


    static uint16_t Read16 (const vector<Byte> & image, size_t at)
    {
        return static_cast<uint16_t> (image[at] | (image[at + 1] << 8));
    }


    static void LoadImage (const vector<Byte> & image, DiskImage & disk)
    {
        HRESULT  hr = WozLoader::Load (image, disk);

        Assert::IsTrue (SUCCEEDED (hr));
    }


    static void RoundTrip (const DiskImage & disk, vector<Byte> & saved, DiskImage & reloaded)
    {
        HRESULT  hr = WozLoader::Serialize (disk, saved);

        Assert::IsTrue (SUCCEEDED (hr));
        LoadImage (saved, reloaded);
    }


    TEST_METHOD (UnwrittenFluxImageRoundTripsExactly)
    {
        vector<Byte>  image;
        vector<Byte>  saved;
        DiskImage     disk;
        DiskImage     reloaded;
        size_t        srcFlux  = 0;
        size_t        outFlux  = 0;
        int           qt       = 0;
        HRESULT       hr       = S_OK;

        hr = WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 0, 1 }), MakeFluxTrack ({ 5, 6, 7 }), MakeFluxTrack ({ 12 }) }, image);
        Assert::IsTrue (SUCCEEDED (hr));

        LoadImage (image, disk);
        RoundTrip (disk, saved, reloaded);

        for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
        {
            Assert::AreEqual (disk.ResolveQuarterTrack (qt), reloaded.ResolveQuarterTrack (qt));
        }

        Assert::IsTrue (reloaded.GetTrackKind (1) == TrackKind::Flux);
        Assert::IsTrue (reloaded.GetTrackKind (2) == TrackKind::Flux);
        Assert::IsTrue (reloaded.GetFluxTrack (1).GetBytes() == disk.GetFluxTrack (1).GetBytes());
        Assert::IsTrue (reloaded.GetFluxTrack (2).GetBytes() == disk.GetFluxTrack (2).GetBytes());

        srcFlux = FindChunk (image, "FLUX");
        outFlux = FindChunk (saved, "FLUX");
        Assert::IsTrue (outFlux > 0);
        Assert::IsTrue (memcmp (image.data() + srcFlux, saved.data() + outFlux, 8 + 160) == 0,
                        L"the FLUX chunk must come back byte for byte");
    }


    TEST_METHOD (SavedFluxChunkSitsWhereInfoSaysOnABlockBoundary)
    {
        vector<Byte>  image;
        vector<Byte>  saved;
        DiskImage     disk;
        DiskImage     reloaded;
        size_t        info    = 0;
        size_t        outFlux = 0;
        HRESULT       hr      = S_OK;

        hr = WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 0 }), MakeFluxTrack ({ 4 }) }, image);
        Assert::IsTrue (SUCCEEDED (hr));

        LoadImage (image, disk);
        RoundTrip (disk, saved, reloaded);

        info    = FindChunk (saved, "INFO") + 8;
        outFlux = FindChunk (saved, "FLUX");

        Assert::AreEqual (static_cast<size_t> (0), outFlux % WozLoader::kV2BlockSize);
        Assert::AreEqual (outFlux / WozLoader::kV2BlockSize,
                          static_cast<size_t> (Read16 (saved, info + WozLoader::kInfoOffsetFluxBlock)));
        Assert::IsTrue   (saved[info + WozLoader::kInfoOffsetVersion] >= 3);
        Assert::IsTrue   (Read16 (saved, info + WozLoader::kInfoOffsetLargestFlux) > 0);
    }


    TEST_METHOD (WrittenFluxTrackStaysFluxAndReadsBackAsWritten)
    {
        vector<Byte>     image;
        vector<Byte>     saved;
        DiskImage        disk;
        DiskImage        reloaded;
        vector<uint8_t>  bits (64, 1);
        HRESULT          hr = S_OK;

        hr = WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 0 }), MakeFluxTrack ({ 4 }) }, image);
        Assert::IsTrue (SUCCEEDED (hr));

        LoadImage (image, disk);
        disk.SpliceFluxWrite (1, 5000, bits);
        Assert::IsTrue (disk.IsTrackDirty (1));

        RoundTrip (disk, saved, reloaded);

        Assert::AreEqual (1, reloaded.ResolveQuarterTrack (4));
        Assert::IsTrue   (reloaded.GetTrackKind (1) == TrackKind::Flux);
        Assert::IsTrue   (reloaded.GetFluxTrack (1).GetBytes() == disk.GetFluxTrack (1).GetBytes());
    }


    TEST_METHOD (GrownFluxTrackUpdatesTrksAndInfo)
    {
        vector<Byte>       image;
        vector<Byte>       saved;
        DiskImage          disk;
        DiskImage          reloaded;
        WozSyntheticTrack  sparse;
        vector<uint8_t>    bits (4000, 1);
        size_t             info    = 0;
        size_t             trks    = 0;
        size_t             blocks  = 0;
        HRESULT            hr      = S_OK;

        // A track that is mostly one long gap fits in a single block.
        sparse.isFlux        = true;
        sparse.quarterTracks = { 0 };
        sparse.data.assign (300, 255);
        sparse.data.push_back (10);

        hr = WozLoader::BuildSyntheticV21 ({ sparse }, image);
        Assert::IsTrue (SUCCEEDED (hr));

        LoadImage (image, disk);
        disk.SpliceFluxWrite (0, 1000, bits);

        RoundTrip (disk, saved, reloaded);

        blocks = (disk.GetFluxTrack (0).GetBytes().size() + 511) / 512;
        info   = FindChunk (saved, "INFO") + 8;
        trks   = FindChunk (saved, "TRKS") + 8;

        Assert::IsTrue   (blocks > 1, L"the write must have grown the track past one block");
        Assert::AreEqual (blocks, static_cast<size_t> (Read16 (saved, trks + 2)));
        Assert::AreEqual (blocks, static_cast<size_t> (Read16 (saved, info + WozLoader::kInfoOffsetLargestFlux)));
        Assert::IsTrue   (reloaded.GetFluxTrack (0).GetBytes() == disk.GetFluxTrack (0).GetBytes());
    }


    TEST_METHOD (BitOnlyImageSavesWithoutFlux)
    {
        vector<Byte>  image;
        vector<Byte>  saved;
        DiskImage     disk;
        DiskImage     reloaded;
        size_t        info = 0;
        HRESULT       hr   = S_OK;

        hr = WozLoader::BuildSyntheticV2 (WozLoader::kDiskType525, false,
                                          vector<Byte> ((kBitCount + 7) / 8, 0xFF), kBitCount, image);
        Assert::IsTrue (SUCCEEDED (hr));

        LoadImage (image, disk);
        RoundTrip (disk, saved, reloaded);

        info = FindChunk (saved, "INFO") + 8;

        Assert::AreEqual (static_cast<size_t> (0), FindChunk (saved, "FLUX"));
        Assert::AreEqual (static_cast<Byte> (2), saved[info + WozLoader::kInfoOffsetVersion]);
        Assert::AreEqual (static_cast<uint16_t> (0), Read16 (saved, info + WozLoader::kInfoOffsetFluxBlock));
        Assert::AreEqual (static_cast<uint16_t> (0), Read16 (saved, info + WozLoader::kInfoOffsetLargestFlux));
        Assert::IsFalse  (reloaded.HasFluxTracks());
    }


    TEST_METHOD (DescribeReadsMetaAfterTheFluxChunk)
    {
        vector<Byte>            image;
        WozLoader::Description  woz;
        HRESULT                 hr   = S_OK;
        const char              meta[] = "title\tBandits\n";
        size_t                  len  = sizeof (meta) - 1;

        hr = WozLoader::BuildSyntheticV21 ({ MakeFluxTrack ({ 0 }) }, image);
        Assert::IsTrue (SUCCEEDED (hr));

        image.insert (image.end(), { 'M', 'E', 'T', 'A', static_cast<Byte> (len), 0, 0, 0 });
        image.insert (image.end(), meta, meta + len);

        WozLoader::Describe (image, woz);

        Assert::AreEqual (static_cast<size_t> (1), woz.meta.size());
        Assert::AreEqual (std::string ("Bandits"), woz.meta[0].value);
    }
};
