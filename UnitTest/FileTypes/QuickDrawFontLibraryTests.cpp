#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/FilePath.h"
#include "FileTypes/QuickDrawFontLibrary.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





static constexpr Word    s_kHelvetica   = 21;
static constexpr Word    s_kTimes       = 20;
static constexpr Word    s_kShaston     = 0xFFFE;
static constexpr size_t  s_kBlockBytes  = 512;
static constexpr size_t  s_kBlocks      = 1600;





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
//  A font file of one character, "A", in a family at a size.
//
////////////////////////////////////////////////////////////////////////////////

static std::vector<Byte> MakeFontFile (Word family, int size)
{
    std::vector<Byte>  bytes = { 1, 'T' };



    AppendWords (bytes, { 6, family, 0, size, 0x0101, 4 });
    AppendWords (bytes, { 0, 'A', 'A', 4, 0, 0xFFFF, 4, 3, 0, 2, 1, 0, 1 });
    bytes.insert (bytes.end(), { 0xE0, 0x00, 0xB8, 0x00, 0xE0, 0x00 });
    AppendWords (bytes, { 0, 3, 5 });
    AppendWords (bytes, { 0x0004, 0x0003, 0xFFFF });

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteFile
//
////////////////////////////////////////////////////////////////////////////////

static void WriteFile (std::vector<Byte> & ioVolume, const char * path, Byte type, const std::vector<Byte> & bytes)
{
    FilePayload        payload;
    std::vector<Byte>  next;



    payload.bytes      = bytes;
    payload.type       = type;
    payload.hasAuxType = true;

    AssertSucceeded (ProDosVolume (ioVolume).Write (FilePath::Parse (path), payload, next));
    ioVolume = std::move (next);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeFontsVolume
//
//  An 800K volume with two fonts and a text file in System/Fonts, and a
//  font outside it.
//
////////////////////////////////////////////////////////////////////////////////

static std::vector<Byte> MakeFontsVolume()
{
    std::vector<Byte>  volume (s_kBlocks * s_kBlockBytes, 0);
    std::vector<Byte>  next;



    AssertSucceeded (ProDosSkeleton::Write (volume, "FONTS"));

    AssertSucceeded (ProDosVolume (volume).CreateDirectory (FilePath::Parse ("System"), next));
    volume = std::move (next);

    AssertSucceeded (ProDosVolume (volume).CreateDirectory (FilePath::Parse ("System/Fonts"), next));
    volume = std::move (next);

    WriteFile (volume, "System/Fonts/Helvetica.12", QuickDrawFontLibrary::kFontType, MakeFontFile (s_kHelvetica, 12));
    WriteFile (volume, "System/Fonts/Times.10",     QuickDrawFontLibrary::kFontType, MakeFontFile (s_kTimes, 10));
    WriteFile (volume, "System/Fonts/Read.Me",      ProDosVolume::kTypeText,         { 'H', 'I' });
    WriteFile (volume, "Times.24",                  QuickDrawFontLibrary::kFontType, MakeFontFile (s_kTimes, 24));

    return volume;
}





TEST_CLASS (QuickDrawFontLibraryTests)
{
public:

    TEST_METHOD (Find_ReturnsTheSize)
    {
        QuickDrawFontLibrary  fonts;



        Assert::IsTrue (fonts.Add (MakeFontFile (s_kHelvetica, 10)));
        Assert::IsTrue (fonts.Add (MakeFontFile (s_kHelvetica, 12)));

        Assert::AreEqual (12, fonts.Find (s_kHelvetica, 12)->GetSize());
    }

    TEST_METHOD (Find_ReturnsTheNearestSize_TheSmallerOfTwo)
    {
        QuickDrawFontLibrary  fonts;



        fonts.Add (MakeFontFile (s_kHelvetica, 10));
        fonts.Add (MakeFontFile (s_kHelvetica, 14));
        fonts.Add (MakeFontFile (s_kHelvetica, 24));

        Assert::AreEqual (10, fonts.Find (s_kHelvetica, 12)->GetSize());
        Assert::AreEqual (14, fonts.Find (s_kHelvetica, 15)->GetSize());
        Assert::AreEqual (24, fonts.Find (s_kHelvetica, 72)->GetSize());
    }

    TEST_METHOD (Find_OtherFamily_IsNull)
    {
        QuickDrawFontLibrary  fonts;



        fonts.Add (MakeFontFile (s_kHelvetica, 10));

        Assert::IsNull (fonts.Find (s_kTimes, 10));
    }

    TEST_METHOD (Add_NotAFont_IsPassedOver)
    {
        QuickDrawFontLibrary  fonts;
        std::vector<Byte>     bytes = { 0x00, 0x01 };



        Assert::IsFalse  (fonts.Add (bytes));
        Assert::AreEqual ((size_t) 0, fonts.GetCount());
    }

    TEST_METHOD (AddVolume_ReadsTheFontsInSystemFonts)
    {
        QuickDrawFontLibrary  fonts;



        AssertSucceeded (fonts.AddVolume (MakeFontsVolume()));

        Assert::AreEqual ((size_t) 2, fonts.GetCount());
        Assert::AreEqual (12, fonts.Find (s_kHelvetica, 12)->GetSize());
        Assert::AreEqual (10, fonts.Find (s_kTimes, 24)->GetSize(), L"The font outside System/Fonts isn't read");
    }

    TEST_METHOD (AddRom_FindsShaston8ByItsHeader)
    {
        QuickDrawFontLibrary  fonts;
        std::vector<Byte>     rom   (100, 0xEA);
        std::vector<Byte>     font  = MakeFontFile (s_kShaston, 8);



        rom.insert (rom.end(), { 0x06, 0x00, 0xFE, 0xFF, 0x00, 0x00, 0x08, 0x00 });   // a header after a nonzero byte, which isn't a font
        rom.push_back (0);
        rom.insert (rom.end(), font.begin() + 2, font.end());
        rom.resize (rom.size() + 50, 0xEA);

        Assert::IsTrue   (fonts.AddRom (rom));
        Assert::AreEqual (8, fonts.Find (s_kShaston, 8)->GetSize());
    }

    TEST_METHOD (AddRom_NoShaston_IsFalse)
    {
        QuickDrawFontLibrary  fonts;
        std::vector<Byte>     rom (256, 0xEA);



        Assert::IsFalse  (fonts.AddRom (rom));
        Assert::AreEqual ((size_t) 0, fonts.GetCount());
    }

    TEST_METHOD (AddVolume_NoFontsFolder_Fails)
    {
        QuickDrawFontLibrary  fonts;
        std::vector<Byte>     volume (s_kBlocks * s_kBlockBytes, 0);
        HRESULT               hr     = S_OK;



        AssertSucceeded (ProDosSkeleton::Write (volume, "EMPTY"));

        hr = fonts.AddVolume (volume);

        Assert::IsTrue   (FAILED (hr));
        Assert::AreEqual ((size_t) 0, fonts.GetCount());
    }
};
