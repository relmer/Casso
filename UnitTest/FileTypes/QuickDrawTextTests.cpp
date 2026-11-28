#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/QuickDrawPicture.h"
#include "FileTypes/QuickDrawFont.h"
#include "FakeQuickDrawFonts.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawTextTests
//
//  Text in pictures built in each test, drawn in a font built in memory:
//  Helvetica with one character, "A", three pixels wide on a four-pixel
//  advance, two rows above the baseline and one below:
//
//      ###
//      #.#
//      ###
//
//  Colors are the standard 320-mode palette: 0 black, 4 blue, 7 red,
//  15 white.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr uint32_t  s_kWhite     = 0xFFFFFFFFu;
static constexpr uint32_t  s_kBlack     = 0xFF000000u;
static constexpr uint32_t  s_kBlue      = 0xFF0000FFu;
static constexpr uint32_t  s_kRed       = 0xFFDD0000u;
static constexpr int       s_kHelvetica = 21;





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
//  MakeFontFile
//
//  The font file: its name, the IIgs header, the font record, the strike
//  two bytes a row, and the location and offset/width tables for "A", the
//  missing symbol and the end.
//
////////////////////////////////////////////////////////////////////////////////

static std::vector<Byte> MakeFontFile()
{
    std::vector<Byte>  bytes = { 1, 'T' };



    AppendWords (bytes, { 6, s_kHelvetica, 0, 3, 0x0101, 4 });
    AppendWords (bytes, { 0, 'A', 'A', 4, 0, 0xFFFF, 4, 3, 0, 2, 1, 0, 1 });
    bytes.insert (bytes.end(), { 0xE0, 0x00, 0xB8, 0x00, 0xE0, 0x00 });
    AppendWords (bytes, { 0, 3, 5 });
    AppendWords (bytes, { 0x0004, 0x0003, 0xFFFF });

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakePicture
//
//  A 320-mode picture 20 pixels square, its font Helvetica.
//
////////////////////////////////////////////////////////////////////////////////

static std::vector<Byte> MakePicture()
{
    std::vector<Byte>  bytes;



    AppendWords (bytes, { 0, 0, 0, 20, 20, 0x8211 });
    AppendWords (bytes, { 0x0003, s_kHelvetica });
    bytes.insert (bytes.end(), { 0, 3 });

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendLongText
//
//  Text with its baseline's left end at (h, v).
//
////////////////////////////////////////////////////////////////////////////////

static void AppendLongText (std::vector<Byte> & bytes, int h, int v, const char * text)
{
    AppendWords (bytes, { 0x0028, v, h });
    bytes.push_back ((Byte) strlen (text));
    bytes.insert (bytes.end(), text, text + strlen (text));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeWithFont
//
////////////////////////////////////////////////////////////////////////////////

static DecodedFile DecodeWithFont (const std::vector<Byte> & bytes)
{
    FakeQuickDrawFonts  fonts;
    DecodeOptions       options;
    DecodedFile         file;
    std::wstring        error;
    bool                parsed  = QuickDrawFont::TryParse (MakeFontFile(), fonts.font);



    Assert::IsTrue (parsed, L"The test font parses");

    options.quickDrawFonts = &fonts;
    AssertSucceeded (QuickDrawPicture::Decode (bytes, FileFacts(), options, file, error));

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





TEST_CLASS (QuickDrawTextTests)
{
public:

    TEST_METHOD (TryParse_ReadsTheHeaderAndTheCharacter)
    {
        QuickDrawFont  font;
        int            advance = 0;
        int            width   = 0;
        int            offset  = 0;



        Assert::IsTrue   (QuickDrawFont::TryParse (MakeFontFile(), font));
        Assert::AreEqual ((int) s_kHelvetica, (int) font.GetFamily());
        Assert::AreEqual (2, font.GetAscent());
        Assert::IsTrue   (font.TryGetChar ('A', advance, width, offset));
        Assert::AreEqual (4, advance);
        Assert::AreEqual (3, width);
        Assert::IsTrue   (font.IsSet ('A', 1, 0));
        Assert::IsFalse  (font.IsSet ('A', 1, 1));
        Assert::IsTrue   (font.TryGetChar ('B', advance, width, offset), L"A missing character is the missing symbol");
        Assert::AreEqual (2, width);
    }

    TEST_METHOD (LongText_DrawsAtThePenInCopyMode)
    {
        std::vector<Byte>  bytes = MakePicture();
        DecodedFile        file;



        AppendLongText (bytes, 2, 4, "A");

        file = DecodeWithFont (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 2, 2), L"Top left of the A");
        Assert::AreEqual (s_kWhite, GetPixel (file, 3, 3), L"The A's hole");
        Assert::AreEqual (s_kBlack, GetPixel (file, 4, 4), L"The row below the baseline");
        Assert::IsTrue (file.notes.empty());
    }

    TEST_METHOD (Colors_ForegroundAndBackground)
    {
        std::vector<Byte>  bytes = MakePicture();
        DecodedFile        file;



        AppendWords    (bytes, { 0x000E, 4, 0x000F, 7 });
        AppendLongText (bytes, 2, 4, "A");

        file = DecodeWithFont (bytes);

        Assert::AreEqual (s_kBlue, GetPixel (file, 2, 2));
        Assert::AreEqual (s_kRed,  GetPixel (file, 3, 3), L"Copy mode draws the background color");
    }

    TEST_METHOD (TxMode_ForegroundOnlyLeavesTheBackground)
    {
        std::vector<Byte>  bytes = MakePicture();
        DecodedFile        file;



        AppendWords    (bytes, { 0x000F, 7, 0x0005, 4 });
        AppendLongText (bytes, 2, 4, "A");

        file = DecodeWithFont (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 2, 2));
        Assert::AreEqual (s_kWhite, GetPixel (file, 3, 3));
    }

    TEST_METHOD (TxFace_BoldWidensTheCharacter)
    {
        std::vector<Byte>  bytes = MakePicture();
        DecodedFile        file;



        AppendWords    (bytes, { 0x0004, 1 });
        AppendLongText (bytes, 2, 4, "A");

        file = DecodeWithFont (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 5, 2));
    }

    TEST_METHOD (ChExtra_SpacesTheCharacters)
    {
        std::vector<Byte>  bytes = MakePicture();
        DecodedFile        file;



        AppendWords    (bytes, { 0x0012, 0, 2 });
        AppendLongText (bytes, 2, 4, "AA");

        file = DecodeWithFont (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 8, 2), L"The second A, 4 plus 2 along");
        Assert::AreEqual (s_kWhite, GetPixel (file, 7, 2));
    }

    TEST_METHOD (TxFace_ItalicOverhangStaysUnderTheNextBox)
    {
        std::vector<Byte>  bytes = MakePicture();
        DecodedFile        file;



        AppendWords    (bytes, { 0x0004, 2 });
        AppendWords    (bytes, { 0x0012, 0, 0xFFFF });
        AppendLongText (bytes, 2, 4, "AA");

        file = DecodeWithFont (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 5, 2), L"The first A's slanted top row, in the second A's box");
        Assert::AreEqual (s_kWhite, GetPixel (file, 2, 2));
    }

    TEST_METHOD (DHText_MovesTheTextPenFromTheLastText)
    {
        std::vector<Byte>  bytes = MakePicture();
        DecodedFile        file;



        AppendLongText (bytes, 2, 4, "A");
        AppendWords    (bytes, { 0x0029 });
        bytes.insert   (bytes.end(), { 10, 1, 'A' });

        file = DecodeWithFont (bytes);

        Assert::AreEqual (s_kBlack, GetPixel (file, 12, 2));
    }

    TEST_METHOD (MissingFont_IsListedByName)
    {
        std::vector<Byte>  bytes = MakePicture();
        DecodedFile        file;
        bool               found = false;



        AppendWords    (bytes, { 0x0003, 20 });
        bytes.insert   (bytes.end(), { 0, 12 });
        AppendLongText (bytes, 2, 4, "A");

        file = DecodeWithFont (bytes);

        for (const std::wstring & note : file.fontNotes)
        {
            found = found || note.find (L"Times 12") != std::wstring::npos;
        }

        Assert::IsTrue (found);
    }

    TEST_METHOD (ArcRot_Bit15WidensA640ModePattern)
    {
        std::vector<Byte>  plain;
        std::vector<Byte>  wide;
        DecodedFile        file;



        //  The first 16 bytes black, the last 16 white.
        for (std::vector<Byte> * bytes : { &plain, &wide })
        {
            AppendWords (*bytes, { 0x80, 0, 0, 8, 16, 0x8211 });

            if (bytes == &wide)
            {
                AppendWords (*bytes, { 0x0014, 0x8000 });
            }

            AppendWords  (*bytes, { 0x0009 });
            bytes->insert (bytes->end(), 16, (Byte) 0x00);
            bytes->insert (bytes->end(), 16, (Byte) 0xFF);
            AppendWords  (*bytes, { 0x0031, 0, 0, 8, 16 });
        }

        file = DecodeWithFont (plain);
        Assert::AreEqual (s_kBlack, GetPixel (file, 0, 4), L"Eight rows of the first 16 bytes");

        file = DecodeWithFont (wide);
        Assert::AreEqual (s_kWhite, GetPixel (file, 0, 4), L"Row 4 is in the last 16 bytes");
    }
};
