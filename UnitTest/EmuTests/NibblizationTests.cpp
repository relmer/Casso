#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizationTests
//
//  Phase 10 / FR-023 / audit §7. Validates the .DSK / .DO / .PO loaders
//  end-to-end: encode a synthetic 143360-byte sector image, then either
//  decode the bit stream back to bytes (round-trip identity) or assert
//  the prologue/epilogue framing landed in the expected positions.
//
//  No host fixture files are required — tests build raw images in memory.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr int      kImageSize = NibblizationLayer::kImageByteSize;
    static constexpr int      kPattern1  = 0x55;
    static constexpr int      kPattern2  = 0xAA;

    vector<Byte> MakeAllValueImage (Byte v)
    {
        return vector<Byte> (kImageSize, v);
    }

    vector<Byte> MakeAlternatingImage (Byte a, Byte b)
    {
        vector<Byte>   img (kImageSize, 0);
        size_t         i   = 0;

        for (i = 0; i < img.size (); i++)
        {
            img[i] = ((i & 1) == 0) ? a : b;
        }

        return img;
    }

    vector<Byte> MakePinnedRandomImage (uint32_t seed)
    {
        vector<Byte>   img (kImageSize, 0);
        uint32_t       state = seed;
        size_t         i     = 0;

        for (i = 0; i < img.size (); i++)
        {
            state = state * 1664525u + 1013904223u;
            img[i] = static_cast<Byte> ((state >> 24) & 0xFF);
        }

        return img;
    }
}



TEST_CLASS (NibblizationTests)
{
public:

    TEST_METHOD (NibblizeDsk_AcceptsCorrectSizedImage)
    {
        DiskImage      img;
        vector<Byte>   raw = MakeAllValueImage (0);

        HRESULT   hr = NibblizationLayer::NibblizeDsk (raw, img);

        Assert::IsTrue (SUCCEEDED (hr), L"DSK nibblization must accept 143360-byte image");
        Assert::AreEqual (DiskImage::kDefaultTrackCount, img.GetTrackCount ());
        Assert::IsTrue (img.GetTrackBitCount (0) > 0,
            L"Track 0 must have bit data after nibblization");
    }

    TEST_METHOD (NibblizeDsk_RejectsShortImage)
    {
        DiskImage      img;
        vector<Byte>   raw (1024, 0);

        HRESULT   hr = NibblizationLayer::NibblizeDsk (raw, img);

        Assert::IsTrue (FAILED (hr));
    }

    TEST_METHOD (DskRoundTripIdentity_AllZeros)
    {
        DiskImage      img;
        vector<Byte>   raw       = MakeAllValueImage (0);
        vector<Byte>   recovered;
        HRESULT        hrLoad    = NibblizationLayer::NibblizeDsk (raw, img);
        HRESULT        hrSave    = S_OK;

        Assert::IsTrue (SUCCEEDED (hrLoad));

        hrSave = NibblizationLayer::Denibblize (img, DiskFormat::Dsk, recovered);

        Assert::IsTrue (SUCCEEDED (hrSave));
        Assert::AreEqual (raw.size (), recovered.size ());
        Assert::IsTrue   (raw == recovered, L"DSK round-trip identity (zeros)");
    }

    TEST_METHOD (DskRoundTripIdentity_AllPatternA5)
    {
        DiskImage      img;
        vector<Byte>   raw       = MakeAllValueImage (0xA5);
        vector<Byte>   recovered;

        Assert::IsTrue (SUCCEEDED (NibblizationLayer::NibblizeDsk (raw, img)));
        Assert::IsTrue (SUCCEEDED (NibblizationLayer::Denibblize  (img, DiskFormat::Dsk, recovered)));

        Assert::IsTrue (raw == recovered, L"DSK round-trip identity (0xA5)");
    }

    TEST_METHOD (DskRoundTripIdentity_Alternating)
    {
        DiskImage      img;
        vector<Byte>   raw       = MakeAlternatingImage (kPattern1, kPattern2);
        vector<Byte>   recovered;

        Assert::IsTrue (SUCCEEDED (NibblizationLayer::NibblizeDsk (raw, img)));
        Assert::IsTrue (SUCCEEDED (NibblizationLayer::Denibblize  (img, DiskFormat::Dsk, recovered)));

        Assert::IsTrue (raw == recovered, L"DSK round-trip identity (alternating)");
    }

    TEST_METHOD (DskRoundTripIdentity_PinnedPrng)
    {
        DiskImage      img;
        vector<Byte>   raw       = MakePinnedRandomImage (0xCA550001u);
        vector<Byte>   recovered;

        Assert::IsTrue (SUCCEEDED (NibblizationLayer::NibblizeDsk (raw, img)));
        Assert::IsTrue (SUCCEEDED (NibblizationLayer::Denibblize  (img, DiskFormat::Dsk, recovered)));

        Assert::IsTrue (raw == recovered, L"DSK round-trip identity (pinned PRNG)");
    }

    TEST_METHOD (DoRoundTripIdentity)
    {
        DiskImage      img;
        vector<Byte>   raw       = MakePinnedRandomImage (0xCA550002u);
        vector<Byte>   recovered;

        Assert::IsTrue (SUCCEEDED (NibblizationLayer::NibblizeDo (raw, img)));
        Assert::IsTrue (img.GetSourceFormat () == DiskFormat::Do);
        Assert::IsTrue (SUCCEEDED (NibblizationLayer::Denibblize (img, DiskFormat::Do, recovered)));

        Assert::IsTrue (raw == recovered, L".DO round-trip identity");
    }

    TEST_METHOD (PoRoundTripIdentity)
    {
        DiskImage      img;
        vector<Byte>   raw       = MakePinnedRandomImage (0xCA550003u);
        vector<Byte>   recovered;

        Assert::IsTrue (SUCCEEDED (NibblizationLayer::NibblizePo (raw, img)));
        Assert::IsTrue (img.GetSourceFormat () == DiskFormat::Po);
        Assert::IsTrue (SUCCEEDED (NibblizationLayer::Denibblize (img, DiskFormat::Po, recovered)));

        Assert::IsTrue (raw == recovered, L".PO round-trip identity");
    }

    TEST_METHOD (PoAndDskInterleavesDifferOnSameSourceBytes)
    {
        // Same flat 143360-byte source produces different nibble streams
        // when interpreted as DSK vs PO because the sector mapping differs.
        DiskImage      dskImg;
        DiskImage      poImg;
        vector<Byte>   raw = MakePinnedRandomImage (0xCA550004u);
        bool           differ = false;

        Assert::IsTrue (SUCCEEDED (NibblizationLayer::NibblizeDsk (raw, dskImg)));
        Assert::IsTrue (SUCCEEDED (NibblizationLayer::NibblizePo  (raw, poImg)));

        // Compare track 1 bit streams (track 0 sectors all colocated for this
        // input but other tracks reorder differently).
        size_t   bits = dskImg.GetTrackBitCount (1);

        Assert::AreEqual (bits, poImg.GetTrackBitCount (1));

        for (size_t i = 0; i < bits; i++)
        {
            if (dskImg.ReadBit (1, i) != poImg.ReadBit (1, i))
            {
                differ = true;
                break;
            }
        }

        Assert::IsTrue (differ, L"DSK and PO must produce different bit streams for the same source");
    }

    TEST_METHOD (AddressFieldFraming_FirstSectorHasD5AA96)
    {
        DiskImage      img;
        vector<Byte>   raw = MakeAllValueImage (0);
        size_t         i   = 0;
        Byte           b0  = 0;
        Byte           b1  = 0;
        Byte           b2  = 0;

        Assert::IsTrue (SUCCEEDED (NibblizationLayer::NibblizeDsk (raw, img)));

        // After 20 0xFF sync bytes (160 bits), expect $D5 $AA $96 prologue.
        // Pack the next 24 bits MSB-first into three bytes.
        for (int n = 0; n < 24; n++)
        {
            Byte   bit = img.ReadBit (0, 160 + n);
            int    pos = n / 8;
            if (pos == 0) { b0 = static_cast<Byte> ((b0 << 1) | bit); }
            else if (pos == 1) { b1 = static_cast<Byte> ((b1 << 1) | bit); }
            else { b2 = static_cast<Byte> ((b2 << 1) | bit); }
        }

        Assert::AreEqual (static_cast<Byte> (0xD5), b0, L"Address prologue byte 0");
        Assert::AreEqual (static_cast<Byte> (0xAA), b1, L"Address prologue byte 1");
        Assert::AreEqual (static_cast<Byte> (0x96), b2, L"Address prologue byte 2");

        UNREFERENCED_PARAMETER (i);
    }

    TEST_METHOD (NibblizeThenDenibblizeProducesByteEqualOriginal)
    {
        // Aggregate test mirrors the tasks.md naming.
        DiskImage      img;
        vector<Byte>   raw       = MakePinnedRandomImage (0xDEADBEEFu);
        vector<Byte>   recovered;

        Assert::IsTrue (SUCCEEDED (NibblizationLayer::NibblizeDsk (raw, img)));
        Assert::IsTrue (SUCCEEDED (NibblizationLayer::Denibblize  (img, DiskFormat::Dsk, recovered)));
        Assert::IsTrue (raw == recovered);
    }

    TEST_METHOD (SerializeOnSyntheticImageMatchesDenibblize)
    {
        // DiskImage::Serialize must agree with NibblizationLayer::Denibblize.
        DiskImage      img;
        vector<Byte>   raw      = MakePinnedRandomImage (0xC0FFEE01u);
        vector<Byte>   viaSer;
        vector<Byte>   viaDirect;

        Assert::IsTrue (SUCCEEDED (NibblizationLayer::NibblizeDsk (raw, img)));
        Assert::IsTrue (SUCCEEDED (img.Serialize (viaSer)));
        Assert::IsTrue (SUCCEEDED (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, viaDirect)));

        Assert::IsTrue (viaSer == viaDirect);
        Assert::IsTrue (viaSer == raw);
    }
};
