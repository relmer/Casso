#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/IigsFormats.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  IigsFontTests, IigsIconsTests and IigsAnimationTests
//
//  Files built in each test, then decoded to pictures.
//
////////////////////////////////////////////////////////////////////////////////

static void AppendWords (std::vector<Byte> & bytes, std::initializer_list<Word> words)
{
    for (Word word : words)
    {
        bytes.push_back ((Byte) word);
        bytes.push_back ((Byte) (word >> 8));
    }
}



TEST_CLASS (IigsFontTests)
{
public:

    TEST_METHOD (Decode_DrawsTheStrikeOntoASheet)
    {
        std::vector<Byte>  bytes = { 1, 'F' };
        DecodedFile        file;
        std::wstring       error;
        bool               bold  = false;



        AppendWords (bytes, { 6, 2, 1, 9, 0x0101, 2 });
        AppendWords (bytes, { 0, 65, 66, 2, 0, 0, 2, 2, 0, 2, 0, 0, 1 });
        bytes.insert (bytes.end(), { 0xA0, 0x00, 0x60, 0x00 });
        AppendWords (bytes, { 0, 1, 3, 3 });
        AppendWords (bytes, { 0x0001, 0xFFFF, 0x0001, 0 });

        AssertSucceeded (IigsFont::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::IsTrue (file.kind == DecodedFile::Kind::Picture);
        Assert::IsTrue (file.width > 0 && file.height > 0);

        for (const std::wstring & note : file.notes)
        {
            bold = bold || note.find (L"bold") != std::wstring::npos;
        }

        Assert::IsTrue (bold, L"The style is in the notes");
    }


    TEST_METHOD (Decode_FailsTablesPastTheEnd)
    {
        std::vector<Byte>  bytes = { 1, 'F' };
        DecodedFile        file;
        std::wstring       error;
        HRESULT            hr    = S_OK;



        AppendWords (bytes, { 6, 2, 0, 9, 0x0101, 2 });
        AppendWords (bytes, { 0, 65, 66, 2, 0, 0, 2, 2, 0, 2, 0, 0, 1 });

        hr = IigsFont::Decode (bytes, FileFacts(), DecodeOptions(), file, error);

        Assert::IsTrue  (FAILED (hr));
        Assert::IsFalse (error.empty());
    }


    TEST_METHOD (DescribeStyle_ListsEachBit)
    {
        Assert::AreEqual (std::wstring (L"plain"),           IigsFont::DescribeStyle (0));
        Assert::AreEqual (std::wstring (L"bold, underline"), IigsFont::DescribeStyle (0x05));
    }
};



TEST_CLASS (IigsIconsTests)
{
public:

    TEST_METHOD (Decode_DrawsEachIconAndItsMask)
    {
        std::vector<Byte>  bytes (26, 0);
        std::vector<Byte>  record (86, 0);
        DecodedFile        file;
        std::wstring       error;



        record[66] = 1;
        record[67] = '*';
        record[82] = 0x06;

        for (int icon = 0; icon < 2; icon++)
        {
            AppendWords (record, { 0, 2, 2, 2 });
            record.insert (record.end(), { 0xF0, 0x0F, 0xF0, 0xF0 });
        }

        record[0] = (Byte) record.size();
        bytes.insert (bytes.end(), record.begin(), record.end());
        AppendWords (bytes, { 0 });

        AssertSucceeded (IigsIcons::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::IsTrue   (file.kind == DecodedFile::Kind::Picture);
        Assert::AreEqual (std::wstring (L"1: * (type $06, aux $0000)"), file.notes[0]);
        Assert::AreEqual (0xFFFFFFFFu, file.bgra[4 * (size_t) file.width + 4],  L"A white pixel inside the mask");
        Assert::AreEqual (0xFF808080u, file.bgra[4 * (size_t) file.width + 5],  L"A pixel outside the mask");
        Assert::AreEqual (0xFF000000u, file.bgra[4 * (size_t) file.width + 10], L"The mask, black where set");
    }
};



TEST_CLASS (IigsAnimationTests)
{
public:

    TEST_METHOD (Decode_DrawsTheFirstFrameAndCountsFrames)
    {
        std::vector<Byte>  bytes (0x8000, 0);
        DecodedFile        file;
        std::wstring       error;



        AppendWords (bytes, { 20, 0, 4, 0, 20, 0 });
        AppendWords (bytes, { 0x7E02, 0x0FFF, 0, 0, 0x7E04, 0x0F00, 0, 0 });

        AssertSucceeded (IigsAnimation::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::AreEqual (320, file.width);
        Assert::AreEqual (std::wstring (L"The first of 2 frames, 4 ticks apart."), file.notes[0]);
    }
};
