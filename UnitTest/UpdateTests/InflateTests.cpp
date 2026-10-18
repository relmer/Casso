#include "Pch.h"

#include "Update/Inflate.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  A dynamic-Huffman stream: .NET's DeflateStream at CompressionLevel.Optimal
//  over s_kpszDynamicText, captured once. Its first block's BTYPE is 2.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char * s_kpszDynamicText =
    "The quick brown fox jumps over the lazy dog. Pack my box with five dozen liquor jugs! "
    "How vexingly quick daft zebras jump; sphinx of black quartz, judge my vow. "
    "The quick brown fox jumps over the lazy dog again and again.";

static constexpr Byte s_kDynamicStream[] =
{
    0x8D, 0x8E, 0xCB, 0x15, 0x82, 0x30, 0x14, 0x05, 0x5B, 0xB9, 0xEE, 0x3D, 0x34, 0x60, 0x03, 0x2E,
    0x5D, 0xD0, 0xC0, 0x0B, 0xF9, 0x42, 0xC8, 0x83, 0x84, 0x7C, 0x48, 0xF5, 0x1E, 0xD4, 0x02, 0xDC,
    0xCD, 0x6A, 0x66, 0x46, 0xAB, 0xB0, 0x67, 0x37, 0x2D, 0x10, 0x91, 0x6B, 0x80, 0xE6, 0x86, 0x39,
    0xAF, 0x5B, 0x02, 0x17, 0x15, 0x71, 0x58, 0x05, 0x4F, 0xFD, 0x84, 0x64, 0x33, 0xE0, 0x45, 0xD3,
    0x82, 0xF5, 0x84, 0xE0, 0x86, 0xEA, 0x0E, 0x0B, 0xED, 0x8A, 0x82, 0xE4, 0xAE, 0x02, 0xBC, 0xDB,
    0x33, 0x47, 0xCC, 0xD9, 0xA4, 0x1B, 0x9E, 0x5C, 0x51, 0x54, 0x73, 0xC1, 0xF8, 0xF3, 0xA7, 0x97,
    0xA4, 0x0F, 0x74, 0x25, 0x22, 0xA5, 0x4F, 0xE0, 0x81, 0xB4, 0x59, 0x17, 0x1A, 0x58, 0x43, 0xF8,
    0x4B, 0xBC, 0x67, 0x8A, 0x47, 0xBF, 0x63, 0xCE, 0xD2, 0xA8, 0x2B, 0x53, 0xB8, 0x0E, 0x18, 0xFF,
    0x1F, 0x04, 0x19, 0x72, 0x01, 0x14, 0xE4, 0x97, 0x86, 0x37
};





////////////////////////////////////////////////////////////////////////////////
//
//  DeflateBitWriter
//
//  Builds fixed-Huffman test streams by hand: plain fields go in least
//  significant bit first, Huffman codes most significant bit first, as
//  RFC 1951 section 3.1.1 packs them.
//
////////////////////////////////////////////////////////////////////////////////

class DeflateBitWriter
{
public:
    std::vector<Byte>  bytes;

    void WriteBits (std::uint32_t value, int count)
    {
        for (int i = 0; i < count; i++)
        {
            PutBit ((value >> i) & 1);
        }
    }

    void WriteCode (std::uint32_t code, int length)
    {
        for (int i = length - 1; i >= 0; i--)
        {
            PutBit ((code >> i) & 1);
        }
    }

    // Fixed-code literal 0-143: 8 bits from 0x30.
    void WriteLiteral (char ch)
    {
        WriteCode (0x30 + (Byte) ch, 8);
    }

    // Fixed-code symbol 256-279: 7 bits from 0.
    void WriteShortSymbol (int symbol)
    {
        WriteCode ((std::uint32_t) (symbol - 256), 7);
    }

private:
    int  m_bit = 0;

    void PutBit (std::uint32_t bit)
    {
        if (m_bit == 0)
        {
            bytes.push_back (0);
        }

        bytes.back() |= (Byte) (bit << m_bit);
        m_bit = (m_bit + 1) % 8;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  InflateTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InflateTests)
{
public:

    static constexpr size_t  kLimit = 1 << 20;

    static std::string ToString (const std::vector<Byte> & bytes)
    {
        return std::string (bytes.begin(), bytes.end());
    }



    TEST_METHOD (Stored_SingleBlock)
    {
        static constexpr Byte kStream[] = { 0x01, 0x05, 0x00, 0xFA, 0xFF, 'H', 'e', 'l', 'l', 'o' };

        std::vector<Byte>  out;
        HRESULT            hr  = S_OK;



        hr = Inflate::Decompress (kStream, kLimit, out);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("Hello"), ToString (out));
    }



    TEST_METHOD (Stored_MultipleBlocksAndEmptyFinal)
    {
        static constexpr Byte kStream[] =
        {
            0x00, 0x02, 0x00, 0xFD, 0xFF, 'a', 'b',
            0x00, 0x01, 0x00, 0xFE, 0xFF, 'c',
            0x01, 0x00, 0x00, 0xFF, 0xFF,
        };

        std::vector<Byte>  out;
        HRESULT            hr  = S_OK;



        hr = Inflate::Decompress (kStream, kLimit, out);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("abc"), ToString (out));
    }



    TEST_METHOD (Fixed_SingleLiteral_KnownBytes)
    {
        // RFC 1951 fixed codes for "a": BFINAL 1, BTYPE 01, literal 0x61
        // (8-bit code 10010001), end of block (7-bit 0000000).
        static constexpr Byte kStream[] = { 0x4B, 0x04, 0x00 };

        std::vector<Byte>  out;
        HRESULT            hr  = S_OK;



        hr = Inflate::Decompress (kStream, kLimit, out);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("a"), ToString (out));
    }



    TEST_METHOD (Fixed_OverlappingMatch)
    {
        DeflateBitWriter   writer;
        std::vector<Byte>  out;
        HRESULT            hr     = S_OK;



        writer.WriteBits (1, 1);            // BFINAL
        writer.WriteBits (1, 2);            // BTYPE fixed
        writer.WriteLiteral ('a');
        writer.WriteLiteral ('b');
        writer.WriteLiteral ('c');
        writer.WriteShortSymbol (260);      // length 6
        writer.WriteCode (2, 5);            // distance code 2: distance 3
        writer.WriteShortSymbol (256);      // end of block

        hr = Inflate::Decompress (writer.bytes, kLimit, out);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("abcabcabc"), ToString (out), L"a match may copy bytes it is producing");
    }



    TEST_METHOD (Dynamic_CapturedStream)
    {
        std::vector<Byte>  out;
        HRESULT            hr  = S_OK;



        Assert::AreEqual (2, (s_kDynamicStream[0] >> 1) & 3, L"the vector really is a dynamic block");

        hr = Inflate::Decompress (s_kDynamicStream, kLimit, out);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string (s_kpszDynamicText), ToString (out));
    }



    TEST_METHOD (Corrupt_ReservedBlockType)
    {
        static constexpr Byte kStream[] = { 0x07, 0x00 };

        std::vector<Byte>  out;
        HRESULT            hr  = S_OK;



        hr = Inflate::Decompress (kStream, kLimit, out);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
        Assert::IsTrue   (out.empty());
    }



    TEST_METHOD (Corrupt_StoredLengthComplementMismatch)
    {
        static constexpr Byte kStream[] = { 0x01, 0x05, 0x00, 0xFA, 0xFE, 'H', 'e', 'l', 'l', 'o' };

        std::vector<Byte>  out;
        HRESULT            hr  = S_OK;



        hr = Inflate::Decompress (kStream, kLimit, out);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }



    TEST_METHOD (Corrupt_TruncatedAnywhere)
    {
        std::vector<Byte>  out;
        HRESULT            hr  = S_OK;



        for (size_t length = 0; length < sizeof (s_kDynamicStream); length++)
        {
            hr = Inflate::Decompress (std::span<const Byte> (s_kDynamicStream, length), kLimit, out);
            Assert::IsTrue (FAILED (hr), std::format (L"truncated to {} bytes", length).c_str());
            Assert::IsTrue (out.empty(), L"no partial output on failure");
        }
    }



    TEST_METHOD (Corrupt_DistanceBeforeStart)
    {
        DeflateBitWriter   writer;
        std::vector<Byte>  out;
        HRESULT            hr     = S_OK;



        writer.WriteBits (1, 1);
        writer.WriteBits (1, 2);
        writer.WriteLiteral ('a');
        writer.WriteShortSymbol (257);      // length 3
        writer.WriteCode (1, 5);            // distance 2, but only 1 byte out
        writer.WriteShortSymbol (256);

        hr = Inflate::Decompress (writer.bytes, kLimit, out);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }



    TEST_METHOD (Corrupt_OutputPastLimit)
    {
        std::vector<Byte>  out;
        HRESULT            hr  = S_OK;



        hr = Inflate::Decompress (s_kDynamicStream, strlen (s_kpszDynamicText) - 1, out);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);

        hr = Inflate::Decompress (s_kDynamicStream, strlen (s_kpszDynamicText), out);
        AssertSucceeded (hr, L"exactly the limit is fine");
    }
};
