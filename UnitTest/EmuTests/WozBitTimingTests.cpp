#include "Pch.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/Disk2NibbleEngine.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WozBitTimingTests
//
//  INFO's optimal bit timing: the value an image gives is reported and kept
//  on save, and every bit track plays at the controller's own cell whatever
//  it is. The sequencer's hold on a finished byte is pinned to Sather here,
//  because that hold is why a faster timing cannot be played.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WozBitTimingTests)
{
public:

    static constexpr size_t  kTrackBits     = 40000;
    static constexpr size_t  kInfoFileStart = WozLoader::kHeaderSize + 8;

    //  Disk bytes picked for how they end and how they start. Each starts with
    //  a 1; D5 follows it with a 1, AA and AC with a 0.
    static constexpr uint8_t  kEndsInOne      = 0xD5;   // 1101 0101
    static constexpr uint8_t  kEndsInOneZero  = 0xAA;   // 1010 1010
    static constexpr uint8_t  kEndsInTwoZeros = 0xAC;   // 1010 1100

    //  A byte, and the byte after it. Sather's Table 9.5 sorts a finished
    //  byte's hold by those two: how the byte ends, and the next one's second
    //  bit.
    struct BytePair
    {
        uint8_t          value = 0;
        uint8_t          next  = 0;
        const wchar_t  * label = nullptr;
    };

    //  A finished byte, and for how many CPU cycles in a row the latch held it.
    struct HeldByte
    {
        uint8_t  value  = 0;
        int      cycles = 0;
    };


    static const vector<BytePair> & GetTablePairs()
    {
        static const vector<BytePair>  pairs =
        {
            { kEndsInOne,      kEndsInOne,     L"ends 1, next byte 11"   },
            { kEndsInOneZero,  kEndsInOne,     L"ends 10, next byte 11"  },
            { kEndsInTwoZeros, kEndsInOne,     L"ends 00, next byte 11"  },
            { kEndsInOne,      kEndsInOneZero, L"ends 1, next byte 10"   },
            { kEndsInOneZero,  kEndsInOneZero, L"ends 10, next byte 10"  },
            { kEndsInTwoZeros, kEndsInOneZero, L"ends 00, next byte 10"  },
        };

        return pairs;
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


    static vector<Byte> Pack (vector<uint8_t> & bits)
    {
        vector<Byte>  packed ((kTrackBits + 7) / 8, 0);
        size_t        i = 0;

        bits.resize (kTrackBits, 1);

        for (i = 0; i < kTrackBits; i++)
        {
            packed[i >> 3] = static_cast<Byte> (packed[i >> 3] | (bits[i] << (7 - (i & 7))));
        }

        return packed;
    }


    //  Ten-bit sync, then a run of bytes that holds every pair in Table 9.5.
    static vector<Byte> MakeTablePairTrack()
    {
        static constexpr int  kSyncBytes = 40;

        const vector<uint8_t>  run = { kEndsInOne,     kEndsInOne,      kEndsInOneZero, kEndsInOne,      kEndsInTwoZeros,
                                       kEndsInOne,     kEndsInOneZero,  kEndsInOneZero, kEndsInTwoZeros, kEndsInOneZero };
        vector<uint8_t>        bits;
        int                    i = 0;

        for (i = 0; i < kSyncBytes; i++)
        {
            AppendByte (bits, 0xFF, 10);
        }

        while (bits.size() + run.size() * 8 <= kTrackBits)
        {
            for (uint8_t value : run)
            {
                AppendByte (bits, value, 8);
            }
        }

        while (bits.size() + 10 <= kTrackBits)
        {
            AppendByte (bits, 0xFF, 10);
        }

        return Pack (bits);
    }


    //  A one-track WOZ 2.1 image whose INFO gives the timing. The synthetic
    //  builder leaves the header CRC zero, which means "not computed", so the
    //  byte can be patched after the fact.
    static void BuildImage (Byte timing, Byte diskType, vector<Byte> & woz)
    {
        vector<WozSyntheticTrack>  tracks (1);
        HRESULT                    hr = S_OK;

        tracks[0].data          = MakeTablePairTrack();
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


    //  Every byte the sequencer finishes over two revolutions of the pair
    //  track, sampled once a cycle, as a polling loop samples the latch.
    static void ReadHeldBytes (Byte timing, vector<HeldByte> & held)
    {
        DiskImage          disk;
        Disk2NibbleEngine  eng;
        uint8_t            nib   = 0;
        uint32_t           cycle = 0;

        LoadImage (timing, disk);

        eng.SetDiskImage    (&disk);
        eng.SetCurrentTrack (0);
        eng.SetMotorOn      (true);

        for (cycle = 0; cycle < 2 * kTrackBits * Disk2NibbleEngine::kCyclesPerBit; cycle++)
        {
            eng.Tick (1);

            if (eng.ConsumeFreshNibble (nib))
            {
                held.push_back ({ nib, 0 });
            }

            if (!held.empty() && eng.PeekReadLatch() == held.back().value)
            {
                held.back().cycles++;
            }
        }
    }


    //  The shortest and longest any `pair.value` was held with `pair.next`
    //  after it. The first byte read is skipped: the sequencer may have come
    //  in part way through it.
    static void GetHoldRange (const vector<HeldByte> & held, const BytePair & pair, int & shortest, int & longest)
    {
        size_t  i     = 0;
        int     found = 0;

        shortest = std::numeric_limits<int>::max();
        longest  = 0;

        for (i = 1; i + 1 < held.size(); i++)
        {
            if (held[i].value != pair.value || held[i + 1].value != pair.next)
            {
                continue;
            }

            shortest = std::min (shortest, held[i].cycles);
            longest  = std::max (longest, held[i].cycles);
            found++;
        }

        Assert::IsTrue (found > 0, pair.label);
    }


    //  Sather, "Understanding the Apple IIe", Table 9.5: the 16-sector
    //  sequencer holds a finished byte for 16 or 17 clocks, whichever way it
    //  ends and whatever the next byte starts with. A 6502 polling the latch
    //  every seven cycles needs 14 (p. 9-33). At two clocks a cycle, that is
    //  eight or nine cycles for every pair.
    //
    //  A track plays at the controller's own cell whatever timing its image
    //  gives, so a disk imaged at 28 or 34 is held the same way. Played at 28
    //  as given, a byte ending in two zeros ahead of one starting 11 would be
    //  held 12 clocks, six cycles, and the boot ROM's poll would step over it.
    TEST_METHOD (EveryByteIsHeldForSathersValidPeriodWhateverTheImageTiming)
    {
        static constexpr int   kShortestHold = 8;
        static constexpr int   kLongestHold  = 9;
        static constexpr Byte  kFastTiming   = 28;
        static constexpr Byte  kSlowTiming   = 34;

        vector<HeldByte>  held;
        int               shortest = 0;
        int               longest  = 0;

        for (Byte timing : { WozLoader::kBitTimingStandard, kFastTiming, kSlowTiming })
        {
            held.clear();
            ReadHeldBytes (timing, held);

            for (const BytePair & pair : GetTablePairs())
            {
                GetHoldRange (held, pair, shortest, longest);

                Assert::IsTrue (shortest >= kShortestHold, pair.label);
                Assert::IsTrue (longest  <= kLongestHold,  pair.label);
            }
        }
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
