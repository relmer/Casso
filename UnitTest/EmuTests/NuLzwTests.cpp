#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/Archives/NuLzw.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzwTests
//
//  LZW/2 round trips over data of each kind a chunk can take: runs, text,
//  bytes that don't compress, the escape byte, and enough text to fill the
//  table and clear it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (NuLzwTests)
{
public:

    static std::vector<Byte> MakeText (size_t bytes, uint32_t seed)
    {
        static constexpr const char *  kpszWords[] = { "APPLE ", "DISK ", "PRODOS ", "SHRINKIT ", "CASSO ", "\r" };

        std::vector<Byte>  out;
        uint32_t           state = seed;



        while (out.size() < bytes)
        {
            state = state * 1103515245u + 12345u;

            const char *  word = kpszWords[(state >> 16) % _countof (kpszWords)];

            out.insert (out.end(), word, word + strlen (word));
        }

        out.resize (bytes);
        return out;
    }

    static std::vector<Byte> MakeNoise (size_t bytes)
    {
        std::vector<Byte>  out (bytes, 0);
        uint32_t           state = 7;



        for (Byte & b : out)
        {
            state = state * 1664525u + 1013904223u;
            b     = (Byte) (state >> 24);
        }

        return out;
    }

    static void RoundTrip (const std::vector<Byte> & original)
    {
        std::vector<Byte>  packed;
        std::vector<Byte>  unpacked;
        std::string        error;



        NuLzw::CompressLzw2 (original, packed);

        AssertSucceeded (NuLzw::ExpandLzw2 (packed, original.size(), unpacked, error));
        Assert::IsTrue (unpacked == original, TextEncoding::NarrowToWide (error).c_str());
    }


    TEST_METHOD (Lzw2_RoundTripsEmptyAndShortData)
    {
        RoundTrip ({});
        RoundTrip ({ 0x41 });
        RoundTrip ({ 0xDB, 0xDB, 0x00 });
    }


    TEST_METHOD (Lzw2_RoundTripsRunsAndTheEscapeByte)
    {
        std::vector<Byte>  data (9000, 0xDB);



        data[100]  = 0x00;
        data[4096] = 0x01;
        RoundTrip (data);
    }


    TEST_METHOD (Lzw2_RoundTripsText)
    {
        std::vector<Byte>  text = MakeText (20'000, 3);
        std::vector<Byte>  packed;



        RoundTrip (text);

        NuLzw::CompressLzw2 (text, packed);
        Assert::IsTrue (packed.size() < text.size() / 2);
    }


    TEST_METHOD (Lzw2_StoresDataThatDoesNotShrink)
    {
        std::vector<Byte>  noise = MakeNoise (10'000);
        std::vector<Byte>  packed;



        RoundTrip (noise);

        NuLzw::CompressLzw2 (noise, packed);
        Assert::IsTrue (packed.size() <= noise.size() + 2 + 3 * 2 + 4096);
    }


    TEST_METHOD (Lzw2_ClearsAFullTable)
    {
        RoundTrip (MakeText (400'000, 11));
    }


    TEST_METHOD (Lzw2_MixesStoredAndCodedChunks)
    {
        std::vector<Byte>  data  = MakeText (8192, 5);
        std::vector<Byte>  noise = MakeNoise (8192);



        data.insert (data.end(), noise.begin(), noise.end());
        data.insert (data.end(), data.begin(), data.begin() + 8192);
        RoundTrip (data);
    }


    TEST_METHOD (Crc_MatchesXmodem)
    {
        std::vector<Byte>  check = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };

        Assert::AreEqual ((Word) 0x31C3, NuLzw::ComputeCrc (check, 0));
    }


    TEST_METHOD (Lzw2_RefusesTruncatedData)
    {
        std::vector<Byte>  packed;
        std::vector<Byte>  unpacked;
        std::string        error;
        std::vector<Byte>  text = MakeText (20'000, 3);



        NuLzw::CompressLzw2 (text, packed);
        packed.resize (packed.size() / 2);

        AssertFailed (NuLzw::ExpandLzw2 (packed, text.size(), unpacked, error));
        Assert::IsFalse (error.empty());
    }
};