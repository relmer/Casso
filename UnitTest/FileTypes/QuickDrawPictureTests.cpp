#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/QuickDrawPicture.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPictureTests
//
//  Pictures built in each test from opcodes, then drawn. Colors are the
//  standard 320-mode palette: 0 black, 4 blue, 15 white.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr uint32_t  s_kWhite = 0xFFFFFFFFu;
static constexpr uint32_t  s_kBlack = 0xFF000000u;
static constexpr uint32_t  s_kBlue  = 0xFF0000FFu;





////////////////////////////////////////////////////////////////////////////////
//
//  AppendWords
//
////////////////////////////////////////////////////////////////////////////////

static void AppendWords (std::vector<Byte> & bytes, std::initializer_list<int> words)
{
    for (int word : words)
    {
        bytes.push_back ((Byte) word);
        bytes.push_back ((Byte) (word >> 8));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakePicture
//
//  The SCB, a frame from (0, 0) to (height, width), and the version opcode.
//
////////////////////////////////////////////////////////////////////////////////

static std::vector<Byte> MakePicture (int scb, int width, int height)
{
    std::vector<Byte>  bytes;



    AppendWords (bytes, { scb, 0, 0, height, width, 0x8211 });

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendPenColor
//
//  A pen pattern of one 320-mode color.
//
////////////////////////////////////////////////////////////////////////////////

static void AppendPenColor (std::vector<Byte> & bytes, Byte color)
{
    AppendWords (bytes, { 0x0009 });
    bytes.insert (bytes.end(), 32, (Byte) ((color << 4) | color));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodePicture
//
////////////////////////////////////////////////////////////////////////////////

static DecodedFile DecodePicture (const std::vector<Byte> & bytes)
{
    DecodedFile   file;
    std::wstring  error;



    AssertSucceeded (QuickDrawPicture::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

    return file;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPixel
//
////////////////////////////////////////////////////////////////////////////////

static uint32_t GetPixel (const DecodedFile & file, int x, int y)
{
    return file.bgra[(size_t) y * file.width + x];
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasNote
//
////////////////////////////////////////////////////////////////////////////////

static bool HasNote (const DecodedFile & file, const wchar_t * text)
{
    bool  found = false;



    for (const std::wstring & note : file.notes)
    {
        found = found || note.find (text) != std::wstring::npos;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeFacts
//
////////////////////////////////////////////////////////////////////////////////

static FileFacts MakeFacts (Byte type, Word auxType)
{
    FileFacts  facts;



    facts.volume     = VolumeKind::ProDos;
    facts.type       = type;
    facts.auxType    = auxType;
    facts.hasAuxType = true;

    return facts;
}





TEST_CLASS (QuickDrawPictureTests)
{
public:

    TEST_METHOD (Decode_EmptyPicture_IsTheFrameInWhite)
    {
        DecodedFile  file = DecodePicture (MakePicture (0, 12, 7));



        Assert::IsTrue (file.kind == DecodedFile::Kind::Picture);
        Assert::AreEqual (12, file.width);
        Assert::AreEqual (7,  file.height);
        Assert::AreEqual (s_kWhite, GetPixel (file, 11, 6));
        Assert::IsTrue (file.notes.empty());
    }

    TEST_METHOD (Decode_PaintRect_FillsTheRectInThePenPattern)
    {
        std::vector<Byte>  bytes = MakePicture (0, 10, 10);
        DecodedFile        file;



        AppendPenColor (bytes, 4);
        AppendWords (bytes, { 0x0031, 2, 3, 5, 6 });

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kBlue,  GetPixel (file, 3, 2));
        Assert::AreEqual (s_kBlue,  GetPixel (file, 5, 4));
        Assert::AreEqual (s_kWhite, GetPixel (file, 6, 4), L"The right edge is outside");
        Assert::AreEqual (s_kWhite, GetPixel (file, 3, 5), L"The bottom edge is outside");
    }

    TEST_METHOD (Decode_FrameRect_DrawsTheEdgeAlone)
    {
        std::vector<Byte>  bytes = MakePicture (0, 10, 10);
        DecodedFile        file;



        AppendWords (bytes, { 0x0030, 1, 1, 8, 8 });

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 1, 1));
        Assert::AreEqual (s_kBlack, GetPixel (file, 7, 4));
        Assert::AreEqual (s_kWhite, GetPixel (file, 4, 4), L"The inside isn't drawn");
    }

    TEST_METHOD (Decode_SameRect_UsesTheLastRect)
    {
        std::vector<Byte>  bytes = MakePicture (0, 10, 10);
        DecodedFile        file;



        AppendWords (bytes, { 0x0031, 2, 2, 4, 4 });
        AppendWords (bytes, { 0x003B });

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kWhite, GetPixel (file, 2, 2), L"Inverting the same rect restores it");
    }

    TEST_METHOD (Decode_PaintOval_LeavesTheCornersOut)
    {
        std::vector<Byte>  bytes = MakePicture (0, 20, 20);
        DecodedFile        file;



        AppendWords (bytes, { 0x0051, 0, 0, 20, 20 });

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 10, 10));
        Assert::AreEqual (s_kWhite, GetPixel (file, 0, 0));
        Assert::AreEqual (s_kWhite, GetPixel (file, 19, 19));
    }

    TEST_METHOD (Decode_PaintArc_DrawsItsQuarter)
    {
        std::vector<Byte>  bytes = MakePicture (0, 20, 20);
        DecodedFile        file;



        AppendWords (bytes, { 0x0061, 0, 0, 20, 20, 0, 90 });

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 14, 5),  L"Top right is in the arc");
        Assert::AreEqual (s_kWhite, GetPixel (file, 5,  14), L"Bottom left isn't");
    }

    TEST_METHOD (Decode_LineAndLineFrom_DrawFromThePen)
    {
        std::vector<Byte>  bytes = MakePicture (0, 10, 10);
        DecodedFile        file;



        AppendWords (bytes, { 0x0020, 0, 0, 4, 4 });
        AppendWords (bytes, { 0x0021, 4, 9 });

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 2, 2));
        Assert::AreEqual (s_kBlack, GetPixel (file, 7, 4), L"LineFrom starts at the end of the line");
        Assert::AreEqual (s_kWhite, GetPixel (file, 2, 7));
    }

    TEST_METHOD (Decode_ShallowLine_ReachesItsEnd)
    {
        std::vector<Byte>  bytes = MakePicture (0, 20, 20);
        DecodedFile        file;



        AppendWords (bytes, { 0x0020, 1, 2, 7, 19 });

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 2,  1));
        Assert::AreEqual (s_kBlack, GetPixel (file, 19, 7));
    }

    TEST_METHOD (Decode_PaintPoly_FillsInside)
    {
        std::vector<Byte>  bytes = MakePicture (0, 10, 10);
        DecodedFile        file;



        AppendWords (bytes, { 0x0071, 10 + 4 * 4, 0, 0, 10, 10, 0, 0, 0, 10, 10, 10, 0, 0 });

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 8, 2), L"Above the diagonal");
        Assert::AreEqual (s_kWhite, GetPixel (file, 2, 8), L"Below the diagonal");
    }

    TEST_METHOD (Decode_PaintRegion_FollowsItsRows)
    {
        std::vector<Byte>  bytes = MakePicture (0, 10, 10);
        DecodedFile        file;



        //  Inside from h 2 to 4 on rows 0 to 1, and from 6 to 8 on rows 2 to 3.
        AppendWords (bytes, { 0x0081, 40, 0, 2, 4, 8 });
        AppendWords (bytes, { 0, 2, 4, 0x7FFF, 2, 2, 4, 6, 8, 0x7FFF, 4, 6, 8, 0x7FFF, 0x7FFF });

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 3, 1));
        Assert::AreEqual (s_kWhite, GetPixel (file, 3, 2));
        Assert::AreEqual (s_kBlack, GetPixel (file, 7, 3));
        Assert::AreEqual (s_kWhite, GetPixel (file, 5, 0));
    }

    TEST_METHOD (Decode_ClipRgn_LimitsDrawing)
    {
        std::vector<Byte>  bytes = MakePicture (0, 10, 10);
        DecodedFile        file;



        AppendWords (bytes, { 0x0001, 10, 0, 0, 5, 5 });
        AppendWords (bytes, { 0x0031, 0, 0, 10, 10 });

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 4, 4));
        Assert::AreEqual (s_kWhite, GetPixel (file, 5, 5));
    }

    TEST_METHOD (Decode_BitsRect_CopiesThePixels)
    {
        std::vector<Byte>  bytes = MakePicture (0, 4, 2);
        DecodedFile        file;



        AppendWords (bytes, { 0x0090, 0, 0, 2, 0, 0, 2, 4, 0, 0, 2, 4, 0, 0, 2, 4, 0 });
        bytes.insert (bytes.end(), { 0x40, 0xF0, 0x04, 0x00 });

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kBlue,  GetPixel (file, 0, 0));
        Assert::AreEqual (s_kBlack, GetPixel (file, 1, 0));
        Assert::AreEqual (s_kWhite, GetPixel (file, 2, 0));
        Assert::AreEqual (s_kBlue,  GetPixel (file, 1, 1));
        Assert::IsTrue (file.notes.empty());
    }

    TEST_METHOD (Decode_PnMask_LimitsThePen)
    {
        std::vector<Byte>  bytes = MakePicture (0, 8, 8);
        DecodedFile        file;



        //  Only the left four pixels of each row.
        AppendWords (bytes, { 0x0013 });
        bytes.insert (bytes.end(), 8, (Byte) 0xF0);
        AppendWords (bytes, { 0x0031, 0, 0, 8, 8 });

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 3, 2));
        Assert::AreEqual (s_kWhite, GetPixel (file, 4, 2));
        Assert::IsTrue (file.notes.empty());
    }

    TEST_METHOD (Decode_BitsRgn_CopiesOnlyInsideItsMask)
    {
        std::vector<Byte>  bytes = MakePicture (0, 4, 2);
        DecodedFile        file;



        AppendWords (bytes, { 0x0091, 0, 0, 2, 0, 0, 2, 4, 0, 0, 2, 4, 0, 0, 2, 4, 0 });
        AppendWords (bytes, { 10, 0, 0, 2, 2 });
        bytes.insert (bytes.end(), 4, (Byte) 0x44);

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kBlue,  GetPixel (file, 1, 1));
        Assert::AreEqual (s_kWhite, GetPixel (file, 2, 1), L"Outside the mask");
        Assert::IsTrue (file.notes.empty());
    }

    TEST_METHOD (Decode_Text_IsListedAndPassedOver)
    {
        std::vector<Byte>  bytes = MakePicture (0, 10, 10);
        DecodedFile        file;



        AppendWords (bytes, { 0x0028, 5, 1 });
        bytes.insert (bytes.end(), { 3, 'A', 'B', 'C' });
        AppendWords (bytes, { 0x0031, 0, 0, 2, 2 });

        file = DecodePicture (bytes);

        Assert::IsTrue (HasNote (file, L"LongText ($0028)"));
        Assert::AreEqual (s_kBlack, GetPixel (file, 1, 1), L"The rect after the text is drawn");
    }

    TEST_METHOD (Decode_UndefinedOpcode_StopsWithANote)
    {
        std::vector<Byte>  bytes = MakePicture (0, 10, 10);
        DecodedFile        file;



        AppendWords (bytes, { 0x00F0, 0x0031, 0, 0, 2, 2 });

        file = DecodePicture (bytes);

        Assert::IsTrue (HasNote (file, L"$00F0"));
        Assert::AreEqual (s_kWhite, GetPixel (file, 1, 1));
    }

    TEST_METHOD (Decode_Truncated_SaysSo)
    {
        std::vector<Byte>  bytes = MakePicture (0, 10, 10);
        DecodedFile        file;



        AppendWords (bytes, { 0x0031, 0, 0 });

        file = DecodePicture (bytes);

        Assert::IsTrue (HasNote (file, L"partway"));
    }

    TEST_METHOD (Decode_640Mode_UsesThe640Palette)
    {
        std::vector<Byte>  bytes = MakePicture (0x80, 8, 2);
        DecodedFile        file;



        //  Color 5 is pixels 1 and 1: blue in the first column of four, red in the second.
        AppendPenColor (bytes, 5);
        AppendWords (bytes, { 0x0031, 0, 0, 2, 8 });

        file = DecodePicture (bytes);

        Assert::AreEqual (s_kBlue,       GetPixel (file, 0, 0));
        Assert::AreEqual (0xFFDD0000u,   GetPixel (file, 1, 0));
    }

    TEST_METHOD (Decode_Packed_UnpacksFirst)
    {
        std::vector<Byte>  picture = MakePicture (0, 10, 10);
        std::vector<Byte>  packed;
        DecodedFile        file;
        std::wstring       error;



        AppendWords (picture, { 0x0031, 0, 0, 2, 2 });
        packed.push_back ((Byte) (picture.size() - 1));
        packed.insert (packed.end(), picture.begin(), picture.end());

        AssertSucceeded (QuickDrawPicture::Decode (packed, MakeFacts (0xC0, 0x0003), DecodeOptions(), file, error));

        Assert::AreEqual (10, file.width);
        Assert::AreEqual (s_kBlack, GetPixel (file, 1, 1));
    }

    TEST_METHOD (Decode_BadFrame_Fails)
    {
        DecodedFile   file;
        std::wstring  error;
        HRESULT       hr    = QuickDrawPicture::Decode (MakePicture (0, 0, 10), FileFacts(), DecodeOptions(), file, error);



        Assert::IsTrue (FAILED (hr));
        Assert::IsFalse (error.empty());
    }

    TEST_METHOD (Matches_ByTypeAndAux)
    {
        std::vector<Byte>  bytes = MakePicture (0, 10, 10);



        Assert::IsTrue  (QuickDrawPicture::MatchesPicture (MakeFacts (0xC1, 0x0001), bytes));
        Assert::IsTrue  (QuickDrawPicture::MatchesPacked  (MakeFacts (0xC0, 0x0003), bytes));
        Assert::IsFalse (QuickDrawPicture::MatchesPicture (MakeFacts (0xC1, 0x0000), bytes));
        Assert::IsTrue  (QuickDrawPicture::MatchesPicture (MakeFacts (0xC1, 0x8000), bytes), L"Beagle Bros clip art");
        Assert::IsFalse (QuickDrawPicture::MatchesPicture (MakeFacts (0xC1, 0x8000), std::vector<Byte> (32)), L"no version opcode");
        Assert::IsFalse (QuickDrawPicture::MatchesPacked  (MakeFacts (0xC0, 0x0001), bytes));
        Assert::AreEqual (std::wstring (L".QuickDraw II.png"), std::wstring (FileFormatTable::GetSuffix (FileFormat::QuickDrawPicture)));
    }
};
