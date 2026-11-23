#include "Pch.h"

#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/SectorReadObserver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorAccessTests
//
//  Byte reads and writes over an in-memory 35-track, 16-sector buffer, with a
//  fake read observer.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Dos33SectorAccessTests)
{
public:

    static constexpr int     kTrack           = 18;
    static constexpr int     kSector          = 5;
    static constexpr size_t  kOffset          = 0x42;
    static constexpr Byte    kValue           = 0xA5;
    static constexpr int     kTrackOffDisk    = 40;
    static constexpr size_t  kSectorsPerTrack = 16;

    class FakeObserver : public ISectorReadObserver
    {
    public:
        void  OnSectorRead (size_t sector) override { sectors.push_back (sector); }

        vector<size_t>  sectors;
    };

    static size_t GetOffset (int track, int sector, size_t offset)
    {
        return ((size_t) track * kSectorsPerTrack + (size_t) sector) * NibbleFormat::kSectorByteSize + offset;
    }

    TEST_METHOD (ReadByte_ReturnsTheByteAndReportsItsSector)
    {
        vector<Byte>       buffer (NibbleFormat::kImageByteSize, 0);
        Dos33SectorAccess  access (buffer);
        FakeObserver       observer;

        buffer[GetOffset (kTrack, kSector, kOffset)] = kValue;
        access.SetReadObserver (&observer);

        Assert::AreEqual (kValue, access.ReadByte (kTrack, kSector, kOffset));
        Assert::AreEqual (size_t (1), observer.sectors.size());
        Assert::AreEqual ((size_t) access.ToUnit (kTrack, kSector), observer.sectors[0]);
    }

    TEST_METHOD (ReadByte_OffTheVolume_ReturnsZeroAndReportsNothing)
    {
        vector<Byte>       buffer (NibbleFormat::kImageByteSize, kValue);
        Dos33SectorAccess  access (buffer);
        FakeObserver       observer;

        access.SetReadObserver (&observer);

        Assert::AreEqual (Byte (0), access.ReadByte (kTrackOffDisk, 0, 0));
        Assert::AreEqual (Byte (0), access.ReadByte (-1, 0, 0));
        Assert::AreEqual (size_t (0), observer.sectors.size());
    }

    TEST_METHOD (WriteByteAt_WritesTheCopyAndLeavesTheSourceAlone)
    {
        vector<Byte>       buffer (NibbleFormat::kImageByteSize, 0);
        vector<Byte>       copy   = buffer;
        Dos33SectorAccess  access (buffer);

        access.WriteByteAt (copy, kTrack, kSector, kOffset, kValue);

        Assert::AreEqual (kValue, copy[GetOffset (kTrack, kSector, kOffset)]);
        Assert::AreEqual (Byte (0), buffer[GetOffset (kTrack, kSector, kOffset)]);
    }

    TEST_METHOD (WriteByteAt_OffTheVolume_WritesNothing)
    {
        vector<Byte>       buffer (NibbleFormat::kImageByteSize, 0);
        vector<Byte>       copy   = buffer;
        Dos33SectorAccess  access (buffer);

        access.WriteByteAt (copy, kTrackOffDisk, 0, 0, kValue);

        Assert::IsTrue (copy == buffer);
    }

    TEST_METHOD (UnitAddressing_RoundTripsTrackAndSector)
    {
        vector<Byte>       buffer (NibbleFormat::kImageByteSize, 0);
        Dos33SectorAccess  access (buffer);
        uint32_t           unit   = access.ToUnit (kTrack, kSector);

        Assert::AreEqual (uint32_t (kTrack * kSectorsPerTrack + kSector), unit);
        Assert::AreEqual (kTrack,  access.GetTrack (unit));
        Assert::AreEqual (kSector, access.GetSector (unit));
    }
};





