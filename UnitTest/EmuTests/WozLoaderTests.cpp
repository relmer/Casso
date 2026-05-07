#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WozLoaderTests
//
//  Phase 10 / FR-022. Validates the chunked WOZ loader against synthetic
//  in-memory v2 images. No host fixture file is required — every test
//  builds its own bytes via WozLoader::BuildSyntheticV2 (the same helper
//  is used by DiskImageStoreTests).
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr size_t  kTestBitCount = 51200;   // ~6400 bytes / track 0

    vector<Byte> MakeBitStream ()
    {
        vector<Byte>   bits ((kTestBitCount + 7) / 8, 0xFF);

        // Drop in a $D5 $AA $96 marker at offset 160 bits (after 20 sync bytes)
        // so AddressFieldFraming-style probes can find it.
        bits[20] = 0xD5;
        bits[21] = 0xAA;
        bits[22] = 0x96;

        return bits;
    }
}



TEST_CLASS (WozLoaderTests)
{
public:

    TEST_METHOD (LoadRejectsTruncatedFile)
    {
        DiskImage     img;
        vector<Byte>  bytes (4, 0);

        HRESULT   hr = WozLoader::Load (bytes, img);

        Assert::IsTrue (FAILED (hr));
    }

    TEST_METHOD (LoadRejectsBadSignature)
    {
        DiskImage     img;
        vector<Byte>  bytes (32, 0);

        bytes[0] = 'W'; bytes[1] = 'O'; bytes[2] = 'Z'; bytes[3] = '9';

        HRESULT   hr = WozLoader::Load (bytes, img);

        Assert::IsTrue (FAILED (hr), L"Unknown WOZ version magic must be rejected");
    }

    TEST_METHOD (LoadRejectsMissingMandatoryChunks)
    {
        // Only a header — no INFO/TMAP/TRKS chunks.
        DiskImage     img;
        vector<Byte>  bytes (12, 0);

        bytes[0] = 'W'; bytes[1] = 'O'; bytes[2] = 'Z'; bytes[3] = '2';
        bytes[4] = 0xFF; bytes[5] = 0x0A; bytes[6] = 0x0D; bytes[7] = 0x0A;

        HRESULT   hr = WozLoader::Load (bytes, img);

        Assert::IsTrue (FAILED (hr));
    }

    TEST_METHOD (LoadAcceptsSyntheticV2)
    {
        DiskImage     img;
        vector<Byte>  bitStream = MakeBitStream ();
        vector<Byte>  woz;

        Assert::IsTrue (SUCCEEDED (WozLoader::BuildSyntheticV2 (
            1, false, bitStream, kTestBitCount, woz)));

        HRESULT   hr = WozLoader::Load (woz, img);

        Assert::IsTrue (SUCCEEDED (hr), L"Synthetic v2 WOZ must load");
        Assert::IsTrue (img.GetSourceFormat () == DiskFormat::Woz);
        Assert::AreEqual (kTestBitCount, img.GetTrackBitCount (0),
            L"Track 0 bit count must match TRK record");
    }

    TEST_METHOD (LoadHonorsWriteProtectFlagInInfoChunk)
    {
        DiskImage     img;
        vector<Byte>  bitStream = MakeBitStream ();
        vector<Byte>  woz;

        Assert::IsTrue (SUCCEEDED (WozLoader::BuildSyntheticV2 (
            1, true, bitStream, kTestBitCount, woz)));

        Assert::IsTrue (SUCCEEDED (WozLoader::Load (woz, img)));
        Assert::IsTrue (img.IsWriteProtected (),
            L"INFO chunk write_protected=1 must surface on the DiskImage");
    }

    TEST_METHOD (LoadCopiesBitStreamFaithfully)
    {
        DiskImage     img;
        vector<Byte>  bitStream = MakeBitStream ();
        vector<Byte>  woz;
        size_t        i   = 0;
        size_t        byteCount = (kTestBitCount + 7) / 8;
        size_t        diff = 0;

        Assert::IsTrue (SUCCEEDED (WozLoader::BuildSyntheticV2 (
            1, false, bitStream, kTestBitCount, woz)));
        Assert::IsTrue (SUCCEEDED (WozLoader::Load (woz, img)));

        for (i = 0; i < byteCount; i++)
        {
            Byte   actual = 0;
            int    b      = 0;

            for (b = 0; b < 8; b++)
            {
                Byte   bit = img.ReadBit (0, i * 8 + b);
                actual = static_cast<Byte> ((actual << 1) | (bit & 1));
            }

            if (actual != bitStream[i])
            {
                diff++;
            }
        }

        Assert::AreEqual (size_t (0), diff,
            L"WOZ bit stream must round-trip into DiskImage byte-for-byte");
    }

    TEST_METHOD (LoadRejectsMalformedChunkSize)
    {
        DiskImage     img;
        vector<Byte>  bitStream = MakeBitStream ();
        vector<Byte>  woz;

        Assert::IsTrue (SUCCEEDED (WozLoader::BuildSyntheticV2 (
            1, false, bitStream, kTestBitCount, woz)));

        // Corrupt the INFO chunk size to overflow the file.
        woz[12 + 4] = 0xFF;
        woz[12 + 5] = 0xFF;
        woz[12 + 6] = 0xFF;
        woz[12 + 7] = 0x00;

        HRESULT   hr = WozLoader::Load (woz, img);

        Assert::IsTrue (FAILED (hr), L"Chunk size that runs past EOF must be rejected");
    }

    TEST_METHOD (LoadIgnoresOptionalMetaChunk)
    {
        // Append a META chunk after the build helper output — the loader
        // must silently ignore it (not fail).
        DiskImage     img;
        vector<Byte>  bitStream = MakeBitStream ();
        vector<Byte>  woz;

        Assert::IsTrue (SUCCEEDED (WozLoader::BuildSyntheticV2 (
            1, false, bitStream, kTestBitCount, woz)));

        // Append META with empty payload.
        woz.push_back ('M');
        woz.push_back ('E');
        woz.push_back ('T');
        woz.push_back ('A');
        woz.push_back (0); woz.push_back (0); woz.push_back (0); woz.push_back (0);

        HRESULT   hr = WozLoader::Load (woz, img);

        Assert::IsTrue (SUCCEEDED (hr), L"META is optional, must not cause load failure");
    }

    TEST_METHOD (BuildSyntheticV2_ProducesAtLeastFourBlocks)
    {
        // Sanity: smallest synthetic still has header + INFO + TMAP + TRKS
        // payload + 1 block of bit data.
        vector<Byte>  bitStream (8, 0);
        vector<Byte>  woz;

        Assert::IsTrue (SUCCEEDED (WozLoader::BuildSyntheticV2 (1, false, bitStream, 64, woz)));
        Assert::IsTrue (woz.size () >= 4 * WozLoader::kV2BlockSize);

        // Signature bytes intact.
        Assert::AreEqual (static_cast<Byte> ('W'), woz[0]);
        Assert::AreEqual (static_cast<Byte> ('O'), woz[1]);
        Assert::AreEqual (static_cast<Byte> ('Z'), woz[2]);
        Assert::AreEqual (static_cast<Byte> ('2'), woz[3]);
    }
};
