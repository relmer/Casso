#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/Archives/SqueezeDecoder.h"
#include "Devices/Disk/Archives/LzcDecoder.h"
#include "Devices/Disk/Archives/NuFxArchive.h"
#include "Devices/Disk/Archives/NuLzw.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SqueezeLzcTests
//
//  Squeeze streams written bit by bit over a tree whose codes are simple, and
//  LZC streams from a small encoder that writes codes the way compress does:
//  the width grows after the code that fills it, and a width change or a
//  clear pads the current group of eight codes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SqueezeLzcTests)
{
public:

    struct BitWriter
    {
        std::vector<Byte>  bytes;
        uint64_t           bit = 0;

        void Put (int code, int width)
        {
            for (int b = 0; b < width; b++, bit++)
            {
                if (bit / 8 >= bytes.size())
                {
                    bytes.push_back (0);
                }

                bytes[(size_t) (bit / 8)] |= (Byte) (((code >> b) & 1) << (bit % 8));
            }
        }

        void PadGroup (int width)
        {
            uint64_t  group = (uint64_t) width * 8;



            bit = (bit + group - 1) / group * group;
            bytes.resize ((size_t) (bit / 8), 0);
        }
    };

    //  A tree in which node i has symbol i on its 0 branch and node i + 1 on
    //  its 1 branch, the last node having the last two symbols; symbol i's
    //  code is i ones and a zero, and the last symbol's is all ones.
    static std::vector<Byte> Squeeze (const std::vector<int> & alphabet, const std::vector<int> & message)
    {
        BitWriter          writer;
        std::vector<Byte>  out;
        size_t             nodes = alphabet.size() - 1;



        out.push_back ((Byte) nodes);
        out.push_back ((Byte) (nodes >> 8));

        for (size_t i = 0; i < nodes; i++)
        {
            int  left  = -(alphabet[i] + 1);
            int  right = (i + 1 < nodes) ? (int) i + 1 : -(alphabet[i + 1] + 1);

            out.push_back ((Byte) left);
            out.push_back ((Byte) (left >> 8));
            out.push_back ((Byte) right);
            out.push_back ((Byte) (right >> 8));
        }

        for (int symbol : message)
        {
            size_t  index = (size_t) (std::find (alphabet.begin(), alphabet.end(), symbol) - alphabet.begin());

            for (size_t i = 0; i < index; i++)
            {
                writer.Put (1, 1);
            }

            if (index < nodes)
            {
                writer.Put (0, 1);
            }
        }

        out.insert (out.end(), writer.bytes.begin(), writer.bytes.end());
        return out;
    }

    static std::vector<Byte> CompressLzc (const std::vector<Byte> & in, int maxBits, bool header, bool clearWhenFull)
    {
        std::unordered_map<uint32_t, int>  table;
        BitWriter                          writer;
        std::vector<Byte>                  out;
        int                                width     = 9;
        int                                maxCode   = 511;
        int                                maxMax    = 1 << maxBits;
        int                                freeEntry = 257;
        int                                w         = 0;



        auto emit = [&] (int code, bool clear)
        {
            writer.Put (code, width);

            if (clear)
            {
                writer.PadGroup (width);
                width   = 9;
                maxCode = 511;
            }
            else if (freeEntry > maxCode)
            {
                writer.PadGroup (width);
                width++;
                maxCode = (width == maxBits) ? maxMax : (1 << width) - 1;
            }
        };

        if (header)
        {
            out = { 0x1F, 0x9D, (Byte) (0x80 | maxBits) };
        }

        if (!in.empty())
        {
            w = in[0];

            for (size_t i = 1; i < in.size(); i++)
            {
                uint32_t  key = ((uint32_t) w << 8) | in[i];
                auto      it  = table.find (key);

                if (it != table.end())
                {
                    w = it->second;
                    continue;
                }

                emit (w, false);

                if (freeEntry < maxMax)
                {
                    table[key] = freeEntry++;
                }
                else if (clearWhenFull)
                {
                    table.clear();
                    freeEntry = 257;
                    emit (256, true);
                }

                w = in[i];
            }

            emit (w, false);
        }

        out.insert (out.end(), writer.bytes.begin(), writer.bytes.end());
        return out;
    }

    static std::vector<Byte> MakeText (size_t bytes, uint32_t seed)
    {
        static constexpr const char *  kpszWords[] = { "APPLE ", "DISK ", "PRODOS ", "SQUEEZE ", "COMPRESS ", "\r" };

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

    static void LzcRoundTrip (const std::vector<Byte> & original, int maxBits, bool header, bool clearWhenFull)
    {
        std::vector<Byte>  packed = CompressLzc (original, maxBits, header, clearWhenFull);
        std::vector<Byte>  unpacked;
        std::string        error;



        AssertSucceeded (LzcDecoder::Expand (packed, maxBits, original.size(), unpacked, error));
        Assert::IsTrue (unpacked == original, TextEncoding::NarrowToWide (error).c_str());
    }


    TEST_METHOD (Squeeze_ExpandsCodesAndRuns)
    {
        std::vector<int>   alphabet = { 'A', 0x90, 5, 'B', 0, 256 };
        std::vector<Byte>  packed   = Squeeze (alphabet, { 'A', 0x90, 5, 'B', 0x90, 0, 256 });
        std::vector<Byte>  plain    = { 'A', 'A', 'A', 'A', 'A', 'B', 0x90 };
        std::vector<Byte>  unpacked;
        std::string        error;



        AssertSucceeded (SqueezeDecoder::Expand (packed, 100, unpacked, error));
        Assert::IsTrue (unpacked == plain);
    }


    TEST_METHOD (Squeeze_SkipsAQqHeader)
    {
        std::vector<int>   alphabet = { 'H', 'I', 256 };
        std::vector<Byte>  packed   = { 0x76, 0xFF, 0x12, 0x34, 'H', 'I', '.', 'T', 'X', 'T', 0 };
        std::vector<Byte>  body     = Squeeze (alphabet, { 'H', 'I', 256 });
        std::vector<Byte>  unpacked;
        std::string        error;



        packed.insert (packed.end(), body.begin(), body.end());

        AssertSucceeded (SqueezeDecoder::Expand (packed, 100, unpacked, error));
        Assert::IsTrue (unpacked == std::vector<Byte> { 'H', 'I' });
    }


    TEST_METHOD (Squeeze_StopsAtTheThreadLength)
    {
        std::vector<int>   alphabet = { 'Z', 0x90, 200, 256 };
        std::vector<Byte>  packed   = Squeeze (alphabet, { 'Z', 0x90, 200, 256 });
        std::vector<Byte>  unpacked;
        std::string        error;



        AssertSucceeded (SqueezeDecoder::Expand (packed, 50, unpacked, error));
        Assert::AreEqual ((size_t) 50, unpacked.size());
    }


    TEST_METHOD (Squeeze_FailsWithoutItsEndCode)
    {
        std::vector<int>   alphabet = { 'A', 'B', 256 };
        std::vector<Byte>  packed   = Squeeze (alphabet, { 'A', 'B' });
        std::vector<Byte>  unpacked;
        std::string        error;



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), SqueezeDecoder::Expand (packed, 100, unpacked, error));
    }


    TEST_METHOD (Squeeze_FailsATreeLargerThanItsData)
    {
        std::vector<Byte>  packed = { 0x10, 0x00, 0xFF, 0xFF };
        std::vector<Byte>  unpacked;
        std::string        error;



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), SqueezeDecoder::Expand (packed, 100, unpacked, error));
    }


    TEST_METHOD (Lzc_ExpandsACodeThatIsTheNextEntry)
    {
        BitWriter          writer;
        std::vector<Byte>  unpacked;
        std::string        error;



        for (int code : { (int) 'A', (int) 'B', 257, 259 })
        {
            writer.Put (code, 9);
        }

        AssertSucceeded (LzcDecoder::Expand (writer.bytes, 12, 7, unpacked, error));
        Assert::IsTrue (unpacked == std::vector<Byte> { 'A', 'B', 'A', 'B', 'A', 'B', 'A' });
    }


    TEST_METHOD (Lzc_RoundTripsShortData)
    {
        LzcRoundTrip ({},             12, false, false);
        LzcRoundTrip ({ 0x41 },       12, false, false);
        LzcRoundTrip ({ 0, 0, 0, 0 }, 16, false, false);
    }


    TEST_METHOD (Lzc_RoundTripsAcrossEveryWidth)
    {
        LzcRoundTrip (MakeText (200'000, 5), 12, false, false);
        LzcRoundTrip (MakeText (400'000, 9), 16, false, false);
    }


    TEST_METHOD (Lzc_RoundTripsAClearAfterAFullTable)
    {
        LzcRoundTrip (MakeText (300'000, 11), 12, false, true);
    }


    TEST_METHOD (Lzc_ReadsTheHeadersWidth)
    {
        std::vector<Byte>  text   = MakeText (100'000, 13);
        std::vector<Byte>  packed = CompressLzc (text, 12, true, true);
        std::vector<Byte>  unpacked;
        std::string        error;



        AssertSucceeded (LzcDecoder::Expand (packed, 16, text.size(), unpacked, error));
        Assert::IsTrue (unpacked == text);
    }


    TEST_METHOD (Lzc_FailsACodeTheTableLacks)
    {
        BitWriter          writer;
        std::vector<Byte>  unpacked;
        std::string        error;



        writer.Put ('A', 9);
        writer.Put (300, 9);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), LzcDecoder::Expand (writer.bytes, 12, 100, unpacked, error));
    }


    TEST_METHOD (NuFxThread_ExpandsSqueezeAndLzc)
    {
        std::vector<Byte>  text = MakeText (5'000, 17);
        NuFxRecord         record;
        NuFxThread         thread;
        std::vector<Byte>  unpacked;
        std::string        error;



        thread.threadClass = NuFxThread::kClassData;
        thread.kind        = NuFxThread::kKindDataFork;
        thread.eof         = (uint32_t) text.size();
        thread.crc         = NuLzw::ComputeCrc (text, 0xFFFF);

        thread.format = NuFxThread::kFormatLzc12;
        thread.stored = CompressLzc (text, 12, false, false);
        AssertSucceeded (NuFxArchive::ExpandThread (record, thread, unpacked, error));
        Assert::IsTrue (unpacked == text);

        thread.format = NuFxThread::kFormatLzc16;
        thread.stored = CompressLzc (text, 16, false, false);
        AssertSucceeded (NuFxArchive::ExpandThread (record, thread, unpacked, error));
        Assert::IsTrue (unpacked == text);

        thread.format = NuFxThread::kFormatSqueeze;
        thread.eof    = 2;
        thread.crc    = NuLzw::ComputeCrc (std::vector<Byte> { 'O', 'K' }, 0xFFFF);
        thread.stored = Squeeze ({ 'O', 'K', 256 }, { 'O', 'K', 256 });
        AssertSucceeded (NuFxArchive::ExpandThread (record, thread, unpacked, error));
        Assert::IsTrue (unpacked == std::vector<Byte> { 'O', 'K' });
    }
};
