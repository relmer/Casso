#include "Pch.h"
#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "Core/MemoryBus.h"
#include "FileTypes/HiResFormats.h"
#include "FileTypes/PackBytes.h"
#include "FileTypes/SuperHiRes.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PackBytesTests, DreamGrafixLzwTests, Lz4fhTests, ShapeTableTests and
//  FontrixFontTests
//
//  Streams built in each test, unpacked or checked, and an LZ4FH file
//  previewed as a hi-res picture.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PackBytesTests)
{
public:

    TEST_METHOD (Unpack_ExpandsEachFlagForm)
    {
        std::vector<Byte>  in       = { 0x01, 'A', 'B', 0x42, 'C', 0x80, 1, 2, 3, 4, 0xC1, 'D' };
        std::vector<Byte>  out;
        std::vector<Byte>  correct  = { 'A', 'B', 'C', 'C', 'C', 1, 2, 3, 4 };



        correct.insert (correct.end(), 8, 'D');

        Assert::AreEqual (in.size(), PackBytes::Unpack (in, 1000, out));
        Assert::IsTrue   (out == correct);
    }


    TEST_METHOD (Unpack_StopsAtTheLimit)
    {
        std::vector<Byte>  in = { 0x7F, 'E', 0x7F, 'F' };
        std::vector<Byte>  out;



        PackBytes::Unpack (in, 100, out);

        Assert::AreEqual ((size_t) 100, out.size());
        Assert::AreEqual ((Byte) 'F', out.back());
    }
};



TEST_CLASS (DreamGrafixLzwTests)
{
public:

    //  Codes appended at 9 bits each, least significant bit first.
    static std::vector<Byte> PackCodes (std::initializer_list<int> codes)
    {
        std::vector<Byte>  bytes;
        size_t             bit = 0;



        for (int code : codes)
        {
            for (int b = 0; b < 9; b++, bit++)
            {
                if (bit / 8 >= bytes.size())
                {
                    bytes.push_back (0);
                }

                bytes[bit / 8] |= (Byte) (((code >> b) & 1) << (bit % 8));
            }
        }

        return bytes;
    }


    TEST_METHOD (UnpackLzw_BuildsStringsFromTheTable)
    {
        std::vector<Byte>  out;



        AssertSucceeded (SuperHiRes3200::UnpackLzw (PackCodes ({ 'A', 'B', 258, 260, 257 }), 100, out));

        Assert::AreEqual (std::string ("ABABABA"), std::string (out.begin(), out.end()));
    }


    TEST_METHOD (UnpackLzw_FailsACodePastTheTable)
    {
        std::vector<Byte>  out;
        HRESULT            hr = SuperHiRes3200::UnpackLzw (PackCodes ({ 'A', 300, 257 }), 100, out);



        Assert::IsTrue (FAILED (hr));
    }
};



TEST_CLASS (Lz4fhTests)
{
public:

    TEST_METHOD (Unpack_CopiesLiteralsAndMatches)
    {
        std::vector<Byte>  in = { 0x66, 0x20, 'A', 'B', 0x00, 0x00, 0x0F, 254 };
        std::vector<Byte>  page;



        AssertSucceeded (Lz4fh::Unpack (in, page));

        Assert::AreEqual ((size_t) 8192, page.size());
        Assert::AreEqual (std::string ("ABABAB"), std::string (page.begin(), page.begin() + 6));
        Assert::AreEqual ((Byte) 0, page[6]);
    }


    TEST_METHOD (Render_PreviewsAsAHiResPicture)
    {
        FileEntry       entry;
        FilePayload     payload;
        PreviewContent  content;
        MemoryBus       bus;



        entry.name       = "PICTURE";
        entry.type       = 0x08;
        entry.auxType    = 0x8066;
        entry.hasAuxType = true;
        payload.bytes    = { 0x66, 0x1F, 0x7F, 254 };

        AssertSucceeded (PreviewDecoder::Render (entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content));

        Assert::IsTrue   (content.kind == PreviewContent::Kind::Picture);
        Assert::IsTrue   (content.pictureMode == PicturePreview::Mode::HiRes);
        Assert::AreEqual ((size_t) 8192, content.bytes.size());
        Assert::AreEqual ((Byte) 0x7F, content.bytes[0]);
    }
};



TEST_CLASS (ShapeTableTests)
{
public:

    TEST_METHOD (Check_AcceptsATableAndFailsAnUnendedShape)
    {
        std::vector<Byte>  table = { 2, 0, 6, 0, 8, 0, 0x2D, 0x00, 0x12, 0x00 };
        std::vector<Byte>  cut   = { 1, 0, 4, 0, 0x2D, 0x2D };
        DecodedFile        file;
        std::wstring       error;



        Assert::IsTrue  (ShapeTable::Check (table));
        Assert::IsFalse (ShapeTable::Check (cut));

        AssertSucceeded (ShapeTable::Decode (table, FileFacts(), DecodeOptions(), file, error));
        Assert::IsTrue  (file.kind == DecodedFile::Kind::Picture);
        Assert::IsFalse (file.bgra.empty());
    }
};


TEST_CLASS (FontrixFontTests)
{
public:

    TEST_METHOD (Decode_ProportionalGlyphsFromTheTable)
    {
        std::vector<Byte>  bytes (0x20, 0);
        FileFacts          facts;
        DecodedFile        file;
        std::wstring       error;



        bytes[0x10] = 2;
        bytes[0x11] = 'A';
        bytes[0x12] = 1;
        bytes[0x14] = 2;
        bytes[0x18] = 0x90;
        bytes[0x19] = 0xF7;
        bytes[0x1A] = 0xB2;

        bytes.resize (0x20 + 2 * ('A' - 32), 0);
        bytes.insert (bytes.end(), { 0x80, 0x00, 0x82, 0x00, 0x84, 0x00 });
        bytes.resize (0x80, 0);
        bytes.insert (bytes.end(), { 0x01, 0x03, 0x05, 0x00 });

        facts.volume = VolumeKind::Dos33;
        facts.type   = 0x04;
        facts.name   = "SET.TEST";

        Assert::IsTrue (FontrixFont::Matches (facts, bytes));
        AssertSucceeded (FontrixFont::Decode (bytes, facts, DecodeOptions(), file, error));
        Assert::IsTrue (file.kind == DecodedFile::Kind::Picture);

        bytes[0x18] = 0;
        Assert::IsFalse (FontrixFont::Matches (facts, bytes), L"Without the signature");
    }
};
