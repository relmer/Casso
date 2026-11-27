#include "Pch.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/Denibblizer.h"

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

    TEST_METHOD (SerializeReturnsBytesAfterPhase10Denibblize)
    {
        DiskImage         img;
        vector<Byte>      bytes;

        HRESULT   hr = img.Serialize (bytes);

        AssertSucceeded (hr,
            L"Phase 10: Serialize routes DSK through Denibblizer::Denibblize");
        Assert::AreEqual (size_t (143360), bytes.size(),
            L"Phase 10: a default 35-track DSK serializes to 143360 bytes");
    }


    //  Each whole track of a sector image, 64 bits of its number.
    static void MakeTracks (DiskImage & img)
    {
        for (int track = 0; track < DiskImage::kDefaultTrackCount; track++)
        {
            img.ResizeTrack (track, 64);

            for (size_t at = 0; at < 64; at++)
            {
                img.WriteBit (track, at, (uint8_t) ((track >> (at % 6)) & 1));
            }
        }
    }


    //  Writes at tracks 2, 2.5 and 3 of a sector image, whose quarter tracks
    //  8 to 11 all map to track 2: 2.5 gets a new slot, so track 2 is as it
    //  was written, and each quarter track maps to the write nearest it.
    TEST_METHOD (AHalfTrackWrite_GetsItsOwnSlot_AndLeavesTheTrackBeside)
    {
        DiskImage  img;
        int        two     = 0;
        int        half    = 0;
        int        three   = 0;



        MakeTracks (img);

        two   = img.PrepareQuarterTrackWrite (8, 64);
        half  = img.PrepareQuarterTrackWrite (10, 64);

        img.WriteBit (half, 0, 1);
        img.WriteBit (half, 1, 1);

        three = img.PrepareQuarterTrackWrite (12, 64);

        Assert::AreEqual (2, two);
        Assert::AreEqual (3, three);
        Assert::AreEqual (DiskImage::kDefaultTrackCount, half, L"the half track's write is in a new slot");

        Assert::AreEqual (2,     img.ResolveQuarterTrack (7));
        Assert::AreEqual (2,     img.ResolveQuarterTrack (8));
        Assert::AreEqual (half,  img.ResolveQuarterTrack (9));
        Assert::AreEqual (half,  img.ResolveQuarterTrack (10));
        Assert::AreEqual (3,     img.ResolveQuarterTrack (11));
        Assert::AreEqual (3,     img.ResolveQuarterTrack (12));

        //  Track 2's bits: 2 is 0b10, so bit 0 is 0 and bit 1 is 1.
        Assert::AreEqual ((uint8_t) 0, img.ReadBit (2, 0));
        Assert::AreEqual ((uint8_t) 1, img.ReadBit (half, 0));
    }


    //  A write at the same half track again goes into the slot it has.
    TEST_METHOD (AHalfTrackWrittenAgain_KeepsItsSlot)
    {
        DiskImage  img;
        int        first  = 0;
        int        second = 0;



        MakeTracks (img);

        first  = img.PrepareQuarterTrackWrite (10, 64);
        second = img.PrepareQuarterTrackWrite (10, 64);

        Assert::AreEqual (first, second);
        Assert::AreEqual (DiskImage::kDefaultTrackCount + 1, img.GetTrackCount());
    }


    //  A write at a whole track of a sector image goes into that track.
    TEST_METHOD (AWholeTrackWrite_GoesIntoItsTrack)
    {
        DiskImage  img;



        MakeTracks (img);

        Assert::AreEqual (5, img.PrepareQuarterTrackWrite (20, 64));
        Assert::AreEqual (DiskImage::kDefaultTrackCount, img.GetTrackCount());
    }


    //  A write where the image has no data gets a blank slot of the given
    //  length, past the tracks the map has entries for but no slot.
    TEST_METHOD (AWriteWithNoDataUnderTheHead_GetsABlankSlot)
    {
        DiskImage  img;
        int        slot = 0;



        MakeTracks (img);

        slot = img.PrepareQuarterTrackWrite (100, 64);

        Assert::AreEqual (25, slot);
        Assert::AreEqual (-1, img.ResolveQuarterTrack (140), L"track 35 has no data");

        slot = img.PrepareQuarterTrackWrite (142, 500);

        Assert::AreEqual (DiskImage::kDefaultTrackCount, slot);
        Assert::AreEqual ((size_t) 500, img.GetTrackBitCount (slot));
        Assert::AreEqual (slot, img.ResolveQuarterTrack (142));
        Assert::AreEqual (-1,   img.ResolveQuarterTrack (144), L"track 36 has no data");
    }
};
