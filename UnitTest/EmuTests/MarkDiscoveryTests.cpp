#include "Pch.h"

#include "Devices/Disk/DiskFieldFormat.h"
#include "Devices/Disk/Inspector/FieldLocator.h"
#include "Devices/Disk/Inspector/LatchFramer.h"
#include "Devices/Disk/Stamp/MarkDiscovery.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MarkDiscoveryTests
//
//  Prologs found on framed tracks built nibble by nibble in memory.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MarkDiscoveryTests)
{
public:

    static constexpr int       kTrack        = 5;
    static constexpr Byte      kVolume       = 254;
    static constexpr int       kSyncNibbles  = 12;
    static constexpr int       kSectors      = 4;
    static constexpr int       kNoiseNibbles = 6400;
    static constexpr uint32_t  kSeed         = 12345;
    static constexpr uint32_t  kMultiplier   = 1103515245u;
    static constexpr int       kHighShift    = 16;
    static constexpr Byte      kNibbleBits   = 0x96;



    static void AppendNibbles (FramedTrack & track, std::span<const Byte> values)
    {
        for (Byte value : values)
        {
            FramedNibble  nibble;

            nibble.value = value;
            track.nibbles.push_back (nibble);
        }
    }



    static void AppendSector (FramedTrack & track, const vector<Byte> & addressProlog, const vector<Byte> & dataProlog, DiskFieldKind kind, Byte sector)
    {
        vector<Byte>  body;
        vector<Byte>  bytes (DiskFieldFormat::kSectorBytes, (Byte) (sector * 7));
        Byte          check      = 0;
        Byte          odd        = 0;
        Byte          even       = 0;
        Byte          address[4] = { kVolume, (Byte) kTrack, sector, DiskFieldFormat::ComputeAddressChecksum (kVolume, (Byte) kTrack, sector) };
        vector<Byte>  sync (kSyncNibbles, 0xFF);
        vector<Byte>  epilog     = { 0xDE, 0xAA, 0xEB };



        AppendNibbles (track, sync);
        AppendNibbles (track, addressProlog);

        for (Byte value : address)
        {
            DiskFieldFormat::Encode44 (value, odd, even);
            AppendNibbles (track, vector<Byte> { odd, even });
        }

        AppendNibbles (track, epilog);
        AppendNibbles (track, sync);
        AppendNibbles (track, dataProlog);

        DiskFieldFormat::EncodeData (kind, bytes, body, check);
        AppendNibbles (track, body);
        AppendNibbles (track, vector<Byte> { check });
        AppendNibbles (track, epilog);
    }



    static FramedTrack MakeTrack (const vector<Byte> & addressProlog, const vector<Byte> & dataProlog, DiskFieldKind kind)
    {
        FramedTrack  track;



        for (int sector = 0; sector < kSectors; sector++)
        {
            AppendSector (track, addressProlog, dataProlog, kind, (Byte) sector);
        }

        return track;
    }



    TEST_METHOD (Discover_SixteenSectorTrackWithD4Prolog_FindsIt)
    {
        FramedTrack  track = MakeTrack ({ 0xD4, 0xAA, 0x96 }, { 0xD5, 0xAA, 0xAD }, DiskFieldKind::Sixteen);
        FieldMarks   marks;



        MarkDiscovery::Discover (track, kTrack, marks);

        Assert::AreEqual ((size_t) 1, marks.address16.size());
        Assert::AreEqual (std::string ("D4 AA 96"), marks.address16[0].ToText());
        Assert::IsTrue   (marks.address13.empty());
        Assert::AreEqual ((size_t) 1, marks.data.size());
        Assert::AreEqual (std::string ("D5 AA AD"), marks.data[0].ToText());
    }



    TEST_METHOD (Discover_ThirteenSectorTrackWithUltimaVariant_FindsItAsThirteen)
    {
        FramedTrack  track = MakeTrack ({ 0xD7, 0xAA, 0xB7 }, { 0xD7, 0xAA, 0xBD }, DiskFieldKind::Thirteen);
        FieldMarks   marks;



        MarkDiscovery::Discover (track, kTrack, marks);

        Assert::AreEqual ((size_t) 1, marks.address13.size());
        Assert::AreEqual (std::string ("D7 AA B7"), marks.address13[0].ToText());
        Assert::IsTrue   (marks.address16.empty());
        Assert::AreEqual (std::string ("D7 AA BD"), marks.data[0].ToText());
    }



    TEST_METHOD (Discover_AddressFieldsOfAnotherTrack_FindsNothing)
    {
        FramedTrack  track = MakeTrack ({ 0xD4, 0xAA, 0x96 }, { 0xD5, 0xAA, 0xAD }, DiskFieldKind::Sixteen);
        FieldMarks   marks;



        MarkDiscovery::Discover (track, kTrack + 1, marks);

        Assert::IsTrue (marks.address16.empty());
        Assert::IsTrue (marks.data.empty());
    }



    TEST_METHOD (Discover_OneSector_IsNotEnoughMatches)
    {
        FramedTrack  track;
        FieldMarks   marks;



        AppendSector (track, { 0xD4, 0xAA, 0x96 }, { 0xD5, 0xAA, 0xAD }, DiskFieldKind::Sixteen, 0);

        MarkDiscovery::Discover (track, kTrack, marks);

        Assert::IsTrue (marks.address16.empty());
    }



    TEST_METHOD (Discover_PseudoRandomNibbles_FindsNothing)
    {
        FramedTrack   track;
        vector<Byte>  values;
        uint32_t      state = kSeed;
        FieldMarks    marks;



        for (int i = 0; i < kNoiseNibbles; i++)
        {
            state = state * kMultiplier + kSeed;
            values.push_back ((Byte) (kNibbleBits | (state >> kHighShift)));
        }

        AppendNibbles (track, values);

        MarkDiscovery::Discover (track, kTrack, marks);

        Assert::IsTrue (marks.address16.empty());
        Assert::IsTrue (marks.address13.empty());
        Assert::IsTrue (marks.data.empty());
    }
};
