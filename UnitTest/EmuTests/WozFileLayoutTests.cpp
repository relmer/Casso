#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "FluxTestImages.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WozFileLayoutTests
//
//  The maps and record table a WOZ file holds, kept as read, and a save that
//  writes them back wherever the track model has not changed them: the
//  standard layout, a bit record that FLUX overrides on the same quarter
//  track, and a record no map refers to.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WozFileLayoutTests)
{
public:

    static constexpr size_t  kBitCount = 51200;
    static constexpr Byte    kNoTrack  = WozFileLayout::kNoTrack;



    static WozSyntheticTrack MakeBitTrack (vector<int> quarterTracks, Byte fill)
    {
        WozSyntheticTrack  track;



        track.data.assign ((kBitCount + 7) / 8, fill);
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



    static void LoadSaveLoad (const vector<WozSyntheticTrack> & tracks, DiskImage & first, DiskImage & second)
    {
        vector<Byte>  image;
        vector<Byte>  saved;
        HRESULT       hr    = S_OK;



        hr = WozLoader::BuildSyntheticV21 (tracks, image);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = WozLoader::Load (image, first);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = WozLoader::Serialize (first, saved);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = WozLoader::Load (saved, second);
        Assert::IsTrue (SUCCEEDED (hr));
    }



    TEST_METHOD (TheMapsAndRecordsAreKeptAsStored)
    {
        DiskImage              first;
        DiskImage              second;
        const WozFileLayout *  layout = nullptr;



        LoadSaveLoad ({ MakeBitTrack ({ 3, 4, 5 }, 0xFF), MakeFluxTrack ({ 8 }) }, first, second);
        layout = &first.GetWozMetadata().layout;

        Assert::IsTrue (layout->hasMaps && layout->hasFluxMap);
        Assert::AreEqual (0, static_cast<int> (layout->tmap[4]));
        Assert::AreEqual (static_cast<int> (kNoTrack), static_cast<int> (layout->tmap[8]));
        Assert::AreEqual (1, static_cast<int> (layout->flux[8]));
        Assert::AreEqual (160, static_cast<int> (layout->records.size()));
        Assert::AreEqual (static_cast<uint32_t> (kBitCount), layout->records[0].bitOrByteCount);
        Assert::IsTrue (layout->records[0].startBlock >= WozLoader::kV2FirstDataBlock);
    }



    TEST_METHOD (TheStandardLayoutSurvivesASave)
    {
        DiskImage  first;
        DiskImage  second;



        //  N-0.25, N and N+0.25 on track N's record.
        LoadSaveLoad ({ MakeBitTrack ({ 0, 1 }, 0xFF), MakeBitTrack ({ 3, 4, 5 }, 0xF7), MakeBitTrack ({ 7, 8, 9 }, 0xEF) }, first, second);

        Assert::IsTrue (first.GetWozMetadata().layout.tmap == second.GetWozMetadata().layout.tmap);
    }



    TEST_METHOD (ASaveKeepsTheBitRecordFluxOverrides)
    {
        DiskImage              first;
        DiskImage              second;
        const WozFileLayout *  layout = nullptr;



        LoadSaveLoad ({ MakeBitTrack ({ 4 }, 0xF7), MakeFluxTrack ({ 4 }) }, first, second);
        layout = &second.GetWozMetadata().layout;

        Assert::AreEqual (0, static_cast<int> (layout->tmap[4]), L"the bit record keeps its TMAP entry");
        Assert::AreEqual (1, static_cast<int> (layout->flux[4]));
        Assert::AreEqual (1, second.ResolveQuarterTrack (4), L"the drive still plays the flux track");
        Assert::IsTrue (second.GetTrackBits (0) == first.GetTrackBits (0), L"the bit record keeps its bits");
    }



    TEST_METHOD (ARecordNoMapRefersToKeepsItsBytes)
    {
        DiskImage              first;
        DiskImage              second;
        const WozFileLayout *  before = nullptr;
        const WozFileLayout *  after  = nullptr;



        LoadSaveLoad ({ MakeBitTrack ({ 0, 1 }, 0xFF), MakeBitTrack ({}, 0xA5) }, first, second);
        before = &first.GetWozMetadata().layout;
        after  = &second.GetWozMetadata().layout;

        Assert::AreEqual (1, static_cast<int> (before->unreferenced.size()));
        Assert::AreEqual (1, static_cast<int> (after->unreferenced.size()));
        Assert::AreEqual (1, after->unreferenced[0].index);
        Assert::AreEqual (static_cast<uint32_t> (kBitCount), after->records[1].bitOrByteCount);
        Assert::IsTrue (after->unreferenced[0].bytes == before->unreferenced[0].bytes);
    }



    TEST_METHOD (AnImageCassoBuiltHasNoStoredLayout)
    {
        DiskImage  disk;



        Assert::IsFalse (disk.GetWozMetadata().layout.hasMaps);
    }
};
