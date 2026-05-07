#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Devices/Disk/DiskImage.h"

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

        Assert::AreEqual (DiskImage::kDefaultTrackCount, img.GetTrackCount (),
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
        Assert::IsFalse (img.IsDirty (), L"Fresh image must be clean");

        img.WriteBit (0, 0, 1);
        Assert::IsTrue  (img.IsDirty (), L"WriteBit must mark image dirty");
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
        Assert::IsFalse (img.IsDirty (),
            L"Write-protected disk must remain clean");
    }

    TEST_METHOD (DefaultSourceFormatIsDsk)
    {
        DiskImage   img;

        Assert::IsTrue (img.GetSourceFormat () == DiskFormat::Dsk,
            L"Default DiskImage reports DSK");
    }

    TEST_METHOD (SetSourceFormatPersists)
    {
        DiskImage   img;

        img.SetSourceFormat (DiskFormat::Woz);
        Assert::IsTrue (img.GetSourceFormat () == DiskFormat::Woz);
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

        Assert::IsTrue (FAILED (hr));
        Assert::IsFalse (img.IsLoaded ());
    }

    TEST_METHOD (SerializeReturnsNotImplWithoutSource)
    {
        DiskImage         img;
        vector<Byte>      bytes;

        HRESULT   hr = img.Serialize (bytes);

        Assert::IsTrue (FAILED (hr),
            L"Serialize without cached source must report failure (Phase 10 lands real path)");
    }
};
