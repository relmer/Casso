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
