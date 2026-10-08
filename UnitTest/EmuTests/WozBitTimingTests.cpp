#include "Pch.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/Disk2NibbleEngine.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WozBitTimingTests
//
//  INFO's optimal bit timing: which values are used, that a bit-stream track
//  plays at the timing its image gives, that a write on such a disk still
//  reads back, and that a save keeps the source's value.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WozBitTimingTests)
{
public:

    static constexpr size_t  kTrackBits     = 40000;
    static constexpr size_t  kInfoFileStart = WozLoader::kHeaderSize + 8;

    static const vector<uint8_t> & GetMarker()
    {
        static const vector<uint8_t>  marker = { 0xD5, 0xAA, 0xAD, 0xE7, 0xF3, 0xFC, 0xEE, 0xDE, 0xAA, 0xEB };

        return marker;
    }


    static void AppendByte (vector<uint8_t> & bits, uint8_t value, int width)
    {
        int  i = 0;

        for (i = 0; i < 8; i++)
        {
            bits.push_back (static_cast<uint8_t> ((value >> (7 - i)) & 1));
        }

        for (i = 8; i < width; i++)
        {
            bits.push_back (0);
        }
    }


    //  Ten-bit sync with the marker once, a quarter of the way round.
    static vector<Byte> MakeMarkedTrack()
    {
        vector<uint8_t>  bits;
        vector<Byte>     packed;
        size_t           i = 0;

        while (bits.size() + 10 <= kTrackBits / 4)
        {
            AppendByte (bits, 0xFF, 10);
        }

        for (i = 0; i < GetMarker().size(); i++)
        {
            AppendByte (bits, GetMarker()[i], 8);
        }

        while (bits.size() + 10 <= kTrackBits)
        {
            AppendByte (bits, 0xFF, 10);
        }

        bits.resize (kTrackBits, 1);
        packed.assign ((kTrackBits + 7) / 8, 0);

        for (i = 0; i < kTrackBits; i++)
        {
            packed[i >> 3] = static_cast<Byte> (packed[i >> 3] | (bits[i] << (7 - (i & 7))));
        }

        return packed;
    }


    //  A one-track WOZ 2.1 image whose INFO gives the timing. The synthetic
    //  builder leaves the header CRC zero, which means "not computed", so the
    //  byte can be patched after the fact.
    static void BuildImage (Byte timing, Byte diskType, vector<Byte> & woz)
    {
        vector<WozSyntheticTrack>  tracks (1);
        HRESULT                    hr = S_OK;

        tracks[0].data          = MakeMarkedTrack();
        tracks[0].bitCount      = kTrackBits;
        tracks[0].quarterTracks = { 0 };

        hr = WozLoader::BuildSyntheticV21 (tracks, woz);
        Assert::IsTrue (SUCCEEDED (hr));

        woz[kInfoFileStart + WozLoader::kInfoOffsetBitTiming] = timing;
        woz[kInfoFileStart + WozLoader::kInfoOffsetDiskType]  = diskType;
    }


    static void LoadImage (Byte timing, DiskImage & disk)
    {
        vector<Byte>  woz;
        HRESULT       hr = S_OK;

        BuildImage (timing, WozLoader::kDiskType525, woz);

        hr = WozLoader::Load (woz, disk);
        Assert::IsTrue (SUCCEEDED (hr));
    }


    static WozMetadata MakeInfo (Byte version, Byte diskType, Byte timing)
    {
        WozMetadata  meta;

        meta.infoPayload.assign (WozLoader::kInfoChunkSize, 0);
        meta.infoPayload[WozLoader::kInfoOffsetVersion]   = version;
        meta.infoPayload[WozLoader::kInfoOffsetDiskType]  = diskType;
        meta.infoPayload[WozLoader::kInfoOffsetBitTiming] = timing;

        return meta;
    }


    TEST_METHOD (UsableTimingsAreTakenAsGiven)
    {
        static constexpr Byte  kVersion2 = 2;
        static constexpr Byte  kVersion3 = 3;

        Assert::AreEqual (Byte (28), WozLoader::GetPlaybackBitTiming (MakeInfo (kVersion2, WozLoader::kDiskType525, 28)));
        Assert::AreEqual (Byte (30), WozLoader::GetPlaybackBitTiming (MakeInfo (kVersion3, WozLoader::kDiskType525, 30)));
        Assert::AreEqual (WozLoader::kBitTimingMin, WozLoader::GetPlaybackBitTiming (MakeInfo (kVersion2, WozLoader::kDiskType525, WozLoader::kBitTimingMin)));
        Assert::AreEqual (WozLoader::kBitTimingMax, WozLoader::GetPlaybackBitTiming (MakeInfo (kVersion2, WozLoader::kDiskType525, WozLoader::kBitTimingMax)));
    }


    TEST_METHOD (UnusableTimingsPlayAtTheStandardRate)
    {
        static constexpr Byte  kVersion1 = 1;
        static constexpr Byte  kVersion2 = 2;

        WozMetadata  none;

        Assert::AreEqual (WozLoader::kBitTimingStandard, WozLoader::GetPlaybackBitTiming (none),
                          L"no source INFO: a disk Casso built, or not a WOZ at all");
        Assert::AreEqual (WozLoader::kBitTimingStandard, WozLoader::GetPlaybackBitTiming (MakeInfo (kVersion1, WozLoader::kDiskType525, 28)),
                          L"INFO version 1 has no timing field");
        Assert::AreEqual (WozLoader::kBitTimingStandard, WozLoader::GetPlaybackBitTiming (MakeInfo (kVersion2, WozLoader::kDiskType35, 16)),
                          L"a 3.5-inch disk's timing does not apply to a Disk II");
        Assert::AreEqual (WozLoader::kBitTimingStandard, WozLoader::GetPlaybackBitTiming (MakeInfo (kVersion2, WozLoader::kDiskType525, 0)));
        Assert::AreEqual (WozLoader::kBitTimingStandard, WozLoader::GetPlaybackBitTiming (MakeInfo (kVersion2, WozLoader::kDiskType525, WozLoader::kBitTimingMin - 1)));
        Assert::AreEqual (WozLoader::kBitTimingStandard, WozLoader::GetPlaybackBitTiming (MakeInfo (kVersion2, WozLoader::kDiskType525, WozLoader::kBitTimingMax + 1)));
        Assert::AreEqual (WozLoader::kBitTimingStandard, WozLoader::GetPlaybackBitTiming (MakeInfo (kVersion2, WozLoader::kDiskType525, 255)));
    }


    TEST_METHOD (DescribeReportsTheStoredTiming)
    {
        vector<Byte>            woz;
        WozLoader::Description  desc;

        BuildImage (29, WozLoader::kDiskType525, woz);
        WozLoader::Describe (woz, desc);

        Assert::IsTrue   (desc.hasBitTiming);
        Assert::AreEqual (Byte (29), desc.bitTiming);
    }


    //  The head covers cycles x 2 clocks x 176 units over 44 x timing units a
    //  cell, give or take the cell it started part way through.
    static void AssertHeadRate (Byte timing)
    {
        static constexpr uint32_t  kCycles            = 7000;
        static constexpr double    kLssClocksPerCycle = 2;

        DiskImage          disk;
        Disk2NibbleEngine  eng;
        double             expected = 0;

        LoadImage (timing, disk);

        eng.SetDiskImage    (&disk);
        eng.SetCurrentTrack (0);
        eng.SetMotorOn      (true);

        Assert::AreEqual (static_cast<uint64_t> (timing) * Disk2NibbleEngine::kFluxUnitsPerTimingStep, eng.GetCellUnits());

        eng.Tick (kCycles);

        expected = kCycles * kLssClocksPerCycle * Disk2NibbleEngine::kFluxUnitsPerLssClock
                 / static_cast<double> (eng.GetCellUnits());

        Assert::AreEqual (expected, static_cast<double> (eng.GetBitPosition()), 1.0);
    }


    TEST_METHOD (HeadMovesAtTheImagesTiming)
    {
        AssertHeadRate (28);
        AssertHeadRate (30);
        AssertHeadRate (32);
    }


    TEST_METHOD (OutOfRangeTimingPlaysAtTheStandardCell)
    {
        DiskImage          disk;
        Disk2NibbleEngine  eng;

        LoadImage (0, disk);
        eng.SetDiskImage (&disk);

        Assert::AreEqual (Disk2NibbleEngine::kFluxUnitsPerCell, eng.GetCellUnits());

        LoadImage (WozLoader::kBitTimingMax + 1, disk);
        eng.SetDiskImage (&disk);

        Assert::AreEqual (Disk2NibbleEngine::kFluxUnitsPerCell, eng.GetCellUnits());
    }


    //  Through the sequencer, end to end: the marker comes round once a
    //  revolution, and a revolution at a timing of T takes T/8 cycles a bit.
    static void AssertRevolutionCycles (Byte timing)
    {
        DiskImage          disk;
        Disk2NibbleEngine  eng;
        vector<uint8_t>    recent;
        vector<uint64_t>   seenAt;
        uint8_t            nib      = 0;
        uint64_t           cycle    = 0;
        uint64_t           limit    = 3 * kTrackBits * Disk2NibbleEngine::kCyclesPerBit;
        double             expected = static_cast<double> (kTrackBits) * timing / 8;

        LoadImage (timing, disk);

        eng.SetDiskImage    (&disk);
        eng.SetCurrentTrack (0);
        eng.SetMotorOn      (true);

        for (cycle = 0; cycle < limit && seenAt.size() < 2; cycle++)
        {
            eng.Tick (1);

            if (!eng.ConsumeFreshNibble (nib))
            {
                continue;
            }

            recent.push_back (nib);

            if (recent.size() > GetMarker().size())
            {
                recent.erase (recent.begin());
            }

            if (recent == GetMarker())
            {
                seenAt.push_back (cycle);
            }
        }

        Assert::AreEqual (size_t (2), seenAt.size(), L"the marker must read back intact, twice");
        Assert::AreEqual (expected, static_cast<double> (seenAt[1] - seenAt[0]), expected / 1000);
    }


    TEST_METHOD (TrackReadsBackAtTheMatchingRate)
    {
        AssertRevolutionCycles (28);
        AssertRevolutionCycles (30);
        AssertRevolutionCycles (32);
    }


    //  The controller writes at its own cell whatever the disk's timing, so a
    //  write on a fast disk has to read back through the same sequencer.
    TEST_METHOD (WriteOnAFastDiskReadsBack)
    {
        static constexpr int       kSyncBytes   = 6;
        static constexpr uint32_t  kLoadCycles  = Disk2NibbleEngine::kCyclesPerBit;
        static constexpr uint32_t  kSyncCycles  = Disk2NibbleEngine::kCyclesPerBit * 8;
        static constexpr uint32_t  kNibbleShift = Disk2NibbleEngine::kCyclesPerBit * 7;

        const vector<uint8_t>  written = { 0xD5, 0xAA, 0x96, 0xFF, 0xFE, 0xAA, 0xAB, 0xDE, 0xAA, 0xEB };
        DiskImage              disk;
        Disk2NibbleEngine      eng;
        vector<uint8_t>        nibbles;
        uint8_t                nib     = 0;
        size_t                 i       = 0;
        uint32_t               cycle   = 0;
        bool                   found   = false;

        LoadImage (28, disk);

        eng.SetDiskImage    (&disk);
        eng.SetCurrentTrack (0);
        eng.SetMotorOn      (true);
        eng.SetWriteMode    (true);

        for (i = 0; i < kSyncBytes; i++)
        {
            eng.SetShiftLoadMode (true);
            eng.WriteLatch       (0xFF);
            eng.Tick             (kLoadCycles);
            eng.SetShiftLoadMode (false);
            eng.Tick             (kSyncCycles);
        }

        for (i = 0; i < written.size(); i++)
        {
            eng.SetShiftLoadMode (true);
            eng.WriteLatch       (written[i]);
            eng.Tick             (kLoadCycles);
            eng.SetShiftLoadMode (false);
            eng.Tick             (kNibbleShift);
        }

        eng.SetWriteMode (false);
        Assert::IsTrue (disk.IsTrackDirty (0));

        // Two revolutions: the written stretch ends right where reading starts.
        for (cycle = 0; cycle < 2 * kTrackBits * Disk2NibbleEngine::kCyclesPerBit; cycle++)
        {
            eng.Tick (1);

            if (eng.ConsumeFreshNibble (nib))
            {
                nibbles.push_back (nib);
            }
        }

        found = search (nibbles.begin(), nibbles.end(), written.begin(), written.end()) != nibbles.end();
        Assert::IsTrue (found, L"nibbles written at the controller's cell must read back at the disk's timing");
    }


    TEST_METHOD (SaveKeepsTheSourceTiming)
    {
        DiskImage               disk;
        vector<Byte>            saved;
        WozLoader::Description  desc;
        HRESULT                 hr = S_OK;

        LoadImage (28, disk);
        disk.WriteBit (0, 0, 1);

        hr = WozLoader::Serialize (disk, saved);
        Assert::IsTrue (SUCCEEDED (hr));

        WozLoader::Describe (saved, desc);
        Assert::AreEqual (Byte (28), desc.bitTiming, L"an edited image keeps the timing it was imaged at");
    }


    TEST_METHOD (DiskCassoBuiltSavesTheStandardTiming)
    {
        DiskImage               disk;
        vector<Byte>            saved;
        WozLoader::Description  desc;
        HRESULT                 hr = S_OK;

        disk.ResizeTrack (0, kTrackBits);

        hr = WozLoader::Serialize (disk, saved);
        Assert::IsTrue (SUCCEEDED (hr));

        WozLoader::Describe (saved, desc);
        Assert::AreEqual (WozLoader::kBitTimingStandard, desc.bitTiming);
    }
};
