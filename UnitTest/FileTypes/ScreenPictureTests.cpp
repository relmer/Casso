#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/MacPaintPicture.h"
#include "FileTypes/ScreenPicture.h"
#include "Machines/Apple2/Common/NtscColorTable.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPictureTests and MacPaintPictureTests
//
//  Saved screens and MacPaint documents built in each test, identified by
//  FileFormatTable and decoded to pictures with each drawing option.
//
////////////////////////////////////////////////////////////////////////////////

class ScreenPictureFiles
{
public:
    static FileEntry    MakeEntry (const char * name, Byte type, Word auxType);
    static DecodedFile  DecodeAs  (FileFormat format, const FileEntry & entry, std::span<const Byte> bytes, const DecodeOptions & options);
};





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPictureFiles::MakeEntry
//
////////////////////////////////////////////////////////////////////////////////

FileEntry ScreenPictureFiles::MakeEntry (const char * name, Byte type, Word auxType)
{
    FileEntry  entry;



    entry.name       = name;
    entry.type       = type;
    entry.auxType    = auxType;
    entry.hasAuxType = true;

    return entry;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPictureFiles::DecodeAs
//
////////////////////////////////////////////////////////////////////////////////

DecodedFile ScreenPictureFiles::DecodeAs (FileFormat format, const FileEntry & entry, std::span<const Byte> bytes, const DecodeOptions & options)
{
    DecodedFile   decoded;
    std::wstring  error;



    AssertSucceeded (FileFormatTable::Decode (format, bytes, FileFacts::From (entry, VolumeKind::ProDos), options, decoded, error));
    Assert::IsTrue (decoded.kind == DecodedFile::Kind::Picture);

    return decoded;
}





TEST_CLASS (ScreenPictureTests)
{
public:

    TEST_METHOD (ABinaryAtEachScreenAddress_IsThatScreen)
    {
        std::vector<Byte>  hiRes (8192);
        std::vector<Byte>  doubleHiRes (16384);
        std::vector<Byte>  loRes (1024);



        Assert::IsTrue (FileFormatTable::Identify (ScreenPictureFiles::MakeEntry ("PIC", 0x06, 0x2000), VolumeKind::ProDos, hiRes)       == FileFormat::HiResScreen);
        Assert::IsTrue (FileFormatTable::Identify (ScreenPictureFiles::MakeEntry ("PIC", 0x06, 0x4000), VolumeKind::ProDos, hiRes)       == FileFormat::HiResScreen);
        Assert::IsTrue (FileFormatTable::Identify (ScreenPictureFiles::MakeEntry ("PIC", 0x06, 0x2000), VolumeKind::ProDos, doubleHiRes) == FileFormat::DoubleHiResScreen);
        Assert::IsTrue (FileFormatTable::Identify (ScreenPictureFiles::MakeEntry ("PIC", 0x06, 0x0400), VolumeKind::ProDos, loRes)       == FileFormat::LoResScreen);
        Assert::IsTrue (FileFormatTable::Identify (ScreenPictureFiles::MakeEntry ("PIC", 0x06, 0x0800), VolumeKind::ProDos, hiRes)       == FileFormat::None, L"a page at an address that isn't a screen's");
        Assert::IsTrue (FileFormatTable::Identify (ScreenPictureFiles::MakeEntry ("PIC", 0x08, 0x0000), VolumeKind::ProDos, doubleHiRes) == FileFormat::DoubleHiResScreen, L"a picture file is drawn by its size");
    }


    TEST_METHOD (APackedPictureFile_UnpacksToAHiResScreen)
    {
        std::vector<Byte>  packed;
        DecodedFile        decoded;



        //  Each pair is one byte repeated four times 64: 256 bytes.
        for (int run = 0; run < 32; run++)
        {
            packed.push_back (0xFF);
            packed.push_back (0x7F);
        }

        Assert::IsTrue (FileFormatTable::Identify (ScreenPictureFiles::MakeEntry ("PIC", 0x08, 0x4000), VolumeKind::ProDos, packed) == FileFormat::HiResScreen);

        decoded = ScreenPictureFiles::DecodeAs (FileFormat::HiResScreen, ScreenPictureFiles::MakeEntry ("PIC", 0x08, 0x4000), packed, DecodeOptions());

        Assert::AreEqual (0xFFFFFFFFu, decoded.bgra[1], L"every dot is lit, and a run of lit dots is white");
    }


    TEST_METHOD (AHiResPicture_InBlackAndWhite_HasNoColor)
    {
        std::vector<Byte>  screen (8192, 0x2A);
        DecodeOptions      mono;
        DecodedFile        color;
        DecodedFile        blackAndWhite;
        bool               colored = false;
        bool               gray    = true;



        mono.hiResMono = true;
        color          = ScreenPictureFiles::DecodeAs (FileFormat::HiResScreen, ScreenPictureFiles::MakeEntry ("PIC", 0x06, 0x2000), screen, DecodeOptions());
        blackAndWhite  = ScreenPictureFiles::DecodeAs (FileFormat::HiResScreen, ScreenPictureFiles::MakeEntry ("PIC", 0x06, 0x2000), screen, mono);

        for (uint32_t pixel : color.bgra)
        {
            colored = colored || ((pixel >> 16) & 0xFF) != (pixel & 0xFF);
        }

        for (uint32_t pixel : blackAndWhite.bgra)
        {
            gray = gray && ((pixel >> 16) & 0xFF) == (pixel & 0xFF) && ((pixel >> 8) & 0xFF) == (pixel & 0xFF);
        }

        Assert::IsTrue (colored, L"alternate dots draw as color");
        Assert::IsTrue (gray,    L"in black and white, they draw as dots");
    }


    TEST_METHOD (ADoubleHiResPicture_IsDrawnInEachForm)
    {
        std::vector<Byte>  screen (16384, 0);
        DecodeOptions      options;
        DecodedFile        simple;
        DecodedFile        windowed;
        DecodedFile        mono;
        FileEntry          entry    = ScreenPictureFiles::MakeEntry ("PIC", 0x06, 0x2000);



        //  The first row's third dot, in the auxiliary half's first byte.
        screen[0] = 0x04;

        options.doubleHiRes = DoubleHiResForm::SimpleColor;
        simple              = ScreenPictureFiles::DecodeAs (FileFormat::DoubleHiResScreen, entry, screen, options);

        options.doubleHiRes = DoubleHiResForm::WindowedColor;
        windowed            = ScreenPictureFiles::DecodeAs (FileFormat::DoubleHiResScreen, entry, screen, options);

        options.doubleHiRes = DoubleHiResForm::BlackAndWhite;
        mono                = ScreenPictureFiles::DecodeAs (FileFormat::DoubleHiResScreen, entry, screen, options);

        Assert::AreEqual (kAppleColors[8], simple.bgra[0],   L"the whole first cell is the dot's color");
        Assert::AreEqual (kAppleColors[8], simple.bgra[3]);
        Assert::AreEqual (kAppleColors[0], simple.bgra[4]);

        Assert::AreEqual (kAppleColors[0], windowed.bgra[1], L"the window ending before the lit dot is dark");
        Assert::AreEqual (kAppleColors[8], windowed.bgra[2]);
        Assert::AreEqual (kAppleColors[8], windowed.bgra[5], L"the window ending three dots after it has the lit dot");
        Assert::AreEqual (kAppleColors[0], windowed.bgra[6]);
        Assert::AreEqual (kAppleColors[8], windowed.bgra[560 + 2], L"each row is drawn twice");

        Assert::AreEqual (0xFF000000u, mono.bgra[1]);
        Assert::AreEqual (0xFFFFFFFFu, mono.bgra[2]);
        Assert::AreEqual (0xFF000000u, mono.bgra[3]);
    }
};





TEST_CLASS (MacPaintPictureTests)
{
public:

    TEST_METHOD (ADocument_IsUnpackedARowAtATime)
    {
        std::vector<Byte>  document (MacPaintPicture::kHeaderBytes, 0);
        DecodedFile        decoded;



        //  -71 is the next byte 72 times: a whole row.
        document.push_back (0xB9);
        document.push_back (0x80);

        for (int row = 1; row < MacPaintPicture::kHeight; row++)
        {
            document.push_back (0xB9);
            document.push_back (0x00);
        }

        Assert::IsTrue (FileFormatTable::Identify (ScreenPictureFiles::MakeEntry ("ART.PNTG", 0x00, 0), VolumeKind::ProDos, document) == FileFormat::MacPaint);
        Assert::IsTrue (FileFormatTable::Identify (ScreenPictureFiles::MakeEntry ("ART",      0x00, 0), VolumeKind::ProDos, document) == FileFormat::None, L"a document is found by its name");

        decoded = ScreenPictureFiles::DecodeAs (FileFormat::MacPaint, ScreenPictureFiles::MakeEntry ("ART.PNTG", 0x00, 0), document, DecodeOptions());

        Assert::AreEqual (MacPaintPicture::kWidth,  decoded.width);
        Assert::AreEqual (MacPaintPicture::kHeight, decoded.height);
        Assert::AreEqual (0xFF000000u, decoded.bgra[0], L"a set bit is black, the high bit leftmost");
        Assert::AreEqual (0xFFFFFFFFu, decoded.bgra[1]);
        Assert::AreEqual (0xFF000000u, decoded.bgra[8]);
        Assert::AreEqual (0xFFFFFFFFu, decoded.bgra[MacPaintPicture::kWidth]);
    }


    TEST_METHOD (AShortDocument_IsNotAMacPaintPicture)
    {
        std::vector<Byte>  document (MacPaintPicture::kHeaderBytes, 0);



        document.push_back (0xB9);
        document.push_back (0x00);

        Assert::IsTrue (FileFormatTable::Identify (ScreenPictureFiles::MakeEntry ("ART.PNTG", 0x00, 0), VolumeKind::ProDos, document) == FileFormat::None);
    }
};
