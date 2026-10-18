#include "Pch.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/FluxBitView.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "FluxTestImages.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FluxSectorAccessTests
//
//  The sector-level tools on a flux disk: a flux track decodes to the sectors
//  it was made from, at nominal speed and off it; a sector write replaces only
//  that sector's data field and leaves the rest of the flux alone; a sector
//  that is not there fails the write rather than regenerating the track; and
//  the whole path the disk command takes round-trips through a saved file.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FluxSectorAccessTests)
{
public:

    static constexpr int     kTracks      = NibblizationLayer::kTrackCount;
    static constexpr size_t  kSectorBytes = NibblizationLayer::kSectorByteSize;

    static vector<Byte> MakeSectors()
    {
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        size_t        i = 0;

        for (i = 0; i < sectors.size(); i++)
        {
            sectors[i] = static_cast<Byte> ((i * 7 + i / 256) & 0xFF);
        }

        return sectors;
    }


    //  A WOZ 2.1 file whose every track is the flux version of the standard
    //  nibblized track, with cells cellScale times nominal, alternating with
    //  its inverse every stretchBits bits when alternate is set.
    static vector<Byte> MakeFluxWoz (const vector<Byte> & sectors, double cellScale, bool alternate, size_t keepBits = SIZE_MAX)
    {
        DiskImage                  bitImage;
        vector<WozSyntheticTrack>  tracks;
        vector<Byte>               image;
        int                        t  = 0;
        HRESULT                    hr = S_OK;

        hr = NibblizationLayer::NibblizeDsk (sectors, bitImage);
        Assert::IsTrue (SUCCEEDED (hr));

        for (t = 0; t < kTracks; t++)
        {
            const vector<Byte>  &  bits  = bitImage.GetTrackBits (t);
            size_t                 count = min (bitImage.GetTrackBitCount (t), keepBits);
            WozSyntheticTrack      track;

            track.isFlux        = true;
            track.quarterTracks = { t * 4 };
            track.data          = alternate
                                  ? FluxTestImages::BitsToFlux (bits, count,
                                        FluxTestImages::MakeAlternating (count, 3000,
                                            FluxTestImages::kNominalCellTicks * cellScale,
                                            FluxTestImages::kNominalCellTicks / cellScale))
                                  : FluxTestImages::BitsToFlux (bits, count,
                                        { { 0, FluxTestImages::kNominalCellTicks * cellScale } });
            tracks.push_back (track);
        }

        hr = WozLoader::BuildSyntheticV21 (tracks, image);
        Assert::IsTrue (SUCCEEDED (hr));

        return image;
    }


    static void LoadWoz (const vector<Byte> & image, DiskImage & disk)
    {
        HRESULT  hr = WozLoader::Load (image, disk);

        Assert::IsTrue (SUCCEEDED (hr));
    }


    static vector<Byte> Decode (const DiskImage & disk)
    {
        vector<Byte>      out;
        DenibblizeReport  report;
        HRESULT           hr = NibblizationLayer::Denibblize (disk, DiskFormat::Dsk, out, report);

        Assert::IsTrue (SUCCEEDED (hr), L"every flux track must decode completely");
        return out;
    }


    TEST_METHOD (NominalFluxDecodesToTheSectorsItWasMadeFrom)
    {
        vector<Byte>  sectors = MakeSectors();
        DiskImage     disk;

        LoadWoz (MakeFluxWoz (sectors, 1.0, false), disk);

        Assert::IsTrue (disk.GetTrackKind (0) == TrackKind::Flux);
        Assert::IsTrue (Decode (disk) == sectors);
    }


    TEST_METHOD (OffSpeedFluxDecodesToTheSameSectors)
    {
        vector<Byte>  sectors = MakeSectors();
        DiskImage     fast;
        DiskImage     mixed;

        LoadWoz (MakeFluxWoz (sectors, 0.97, false), fast);
        LoadWoz (MakeFluxWoz (sectors, 1.03, true),  mixed);

        Assert::IsTrue (Decode (fast)  == sectors, L"a disk written 3% fast must decode");
        Assert::IsTrue (Decode (mixed) == sectors, L"a disk alternating 3% fast and slow must decode");
    }


    TEST_METHOD (LongGapsDecodeAsZerosEveryTime)
    {
        FluxTrack    track;
        FluxBitView  first;
        FluxBitView  second;
        vector<Byte> bytes (10, 32);
        size_t       i = 0;

        bytes.insert (bytes.end(), { 255, 255, 255, 255, 76 });
        bytes.push_back (32);
        track.Assign (bytes);

        first.Build  (track);
        second.Build (track);

        Assert::IsTrue (first.GetBits() == second.GetBits(), L"a sector decode must be deterministic");

        // 10 cells, then a gap of 35 cells (1,096 ticks) whose last holds the transition, then 1.
        Assert::AreEqual (static_cast<size_t> (10 + 35 + 1), first.GetBitCount());

        for (i = 10; i < 10 + 34; i++)
        {
            Assert::AreEqual (0, (first.GetBits()[i >> 3] >> (7 - (i & 7))) & 1);
        }
    }


    TEST_METHOD (MixedBitAndFluxImageDecodes)
    {
        vector<Byte>  sectors = MakeSectors();
        DiskImage     bitImage;
        vector<Byte>  image;
        DiskImage     disk;
        HRESULT       hr      = S_OK;
        int           t       = 0;

        vector<WozSyntheticTrack>  tracks;

        hr = NibblizationLayer::NibblizeDsk (sectors, bitImage);
        Assert::IsTrue (SUCCEEDED (hr));

        for (t = 0; t < kTracks; t++)
        {
            WozSyntheticTrack  track;

            track.isFlux        = (t % 2) == 1;
            track.quarterTracks = { t * 4 };
            track.bitCount      = bitImage.GetTrackBitCount (t);
            track.data          = track.isFlux
                                  ? FluxTestImages::BitsToNominalFlux (bitImage.GetTrackBits (t), track.bitCount)
                                  : bitImage.GetTrackBits (t);
            tracks.push_back (track);
        }

        hr = WozLoader::BuildSyntheticV21 (tracks, image);
        Assert::IsTrue (SUCCEEDED (hr));

        LoadWoz (image, disk);
        Assert::IsTrue (Decode (disk) == sectors);
    }


    TEST_METHOD (SectorWriteReplacesOnlyThatDataField)
    {
        vector<Byte>          sectors = MakeSectors();
        vector<Byte>          edited  = sectors;
        DiskImage             disk;
        vector<uint64_t>      before;
        vector<uint64_t>      after;
        vector<int>           changed = { 5 };
        uint64_t              total   = 0;
        size_t                kept    = 0;
        size_t                i       = 0;
        int                   t       = 0;
        HRESULT               hr      = S_OK;
        vector<vector<Byte>>  otherTracks;

        LoadWoz (MakeFluxWoz (sectors, 1.03, true), disk);

        for (t = 0; t < kTracks; t++)
        {
            otherTracks.push_back (disk.GetFluxTrack (t).GetBytes());
        }

        total = disk.GetFluxTrack (5).GetTotalTicks();
        disk.GetFluxTrack (5).GetTransitionTicks (before);

        // One sector on track 5.
        memset (&edited[(5 * 16 + 9) * kSectorBytes], 0xA5, kSectorBytes);

        hr = NibblizationLayer::RenibblizeTracks (edited, DiskFormat::Dsk, changed, disk);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::IsTrue   (disk.GetTrackKind (5) == TrackKind::Flux, L"a sector write must keep the track flux");
        Assert::AreEqual (total, disk.GetFluxTrack (5).GetTotalTicks());
        Assert::IsTrue   (Decode (disk) == edited);

        for (t = 0; t < kTracks; t++)
        {
            if (t != 5)
            {
                Assert::IsTrue (disk.GetFluxTrack (t).GetBytes() == otherTracks[t]);
            }
        }

        // Nearly every transition on the track is where it was: one data
        // field is about 2,800 cells of a 51,000-cell track.
        disk.GetFluxTrack (5).GetTransitionTicks (after);

        for (i = 0; i < before.size(); i++)
        {
            kept += binary_search (after.begin(), after.end(), before[i]) ? 1 : 0;
        }

        Assert::IsTrue (kept > before.size() * 9 / 10,
                        L"a sector write must leave the rest of the track's flux where it was");
    }


    TEST_METHOD (WriteToAMissingSectorFailsWithoutTouchingTheTrack)
    {
        vector<Byte>  sectors = MakeSectors();
        vector<Byte>  edited  = sectors;
        DiskImage     disk;
        vector<Byte>  before;
        vector<int>   changed = { 3 };
        HRESULT       hr      = S_OK;

        // Only the first part of each track survives, so the later sectors'
        // fields are not there to write into.
        LoadWoz (MakeFluxWoz (sectors, 1.0, false, 20000), disk);
        before = disk.GetFluxTrack (3).GetBytes();

        memset (&edited[(3 * 16 + NibblizationLayer::GetDosFileIndexForPhysicalSector (14)) * kSectorBytes], 0x5A, kSectorBytes);

        hr = NibblizationLayer::RenibblizeTracks (edited, DiskFormat::Dsk, changed, disk);

        Assert::AreEqual (static_cast<long> (HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND)), static_cast<long> (hr));
        Assert::IsTrue (disk.GetFluxTrack (3).GetBytes() == before);
    }


    TEST_METHOD (DiskCommandSaveRoundTripsAFluxImage)
    {
        vector<Byte>        sectors = MakeSectors();
        vector<Byte>        edited;
        vector<Byte>        file    = MakeFluxWoz (sectors, 1.0, false);
        vector<Byte>        saved;
        vector<Byte>        loaded;
        SectorDecodeReport  report;
        std::string         reason;
        DiskImage           reloaded;
        HRESULT             hr      = S_OK;

        hr = VolumeImage::Load (file, "flux.woz", edited, report);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (edited == sectors);

        memset (&edited[(17 * 16 + 4) * kSectorBytes], 0x3C, kSectorBytes);

        hr = VolumeImage::Save (file, "flux.woz", edited, saved, reason);
        Assert::IsTrue (SUCCEEDED (hr), L"a sector write to a flux image must be accepted");

        LoadWoz (saved, reloaded);
        Assert::IsTrue (reloaded.GetTrackKind (17) == TrackKind::Flux);

        hr = VolumeImage::Load (saved, "flux.woz", loaded, report);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (loaded == edited);
    }
};
