#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/InfocomSaveBuilder.h"
#include "FileTypes/DiskSaveTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTableTests
//
//  A disk with no file system found and decoded as a game's saved game:
//  InfocomSaveBuilder's save disk, and a blank disk that no entry accepts.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskSaveTableTests)
{
public:

    static std::wstring GetText (const Paragraph & paragraph)
    {
        std::wstring  text;

        for (const Run & run : paragraph.runs)
        {
            text += run.text;
        }

        return text;
    }


    TEST_METHOD (InfocomSaveDisk_IsFoundAndListsEachPosition)
    {
        std::vector<Byte>             disk  = InfocomSaveBuilder::MakeDisk (InfocomSaveBuilder::MakeMemory (0), 2);
        const DiskSaveTable::Entry  * entry = DiskSaveTable::Find (disk);
        DecodedFile                   file;



        Assert::IsNotNull (entry);
        AssertSucceeded   (entry->decode (disk, file));

        Assert::IsTrue   (file.kind == DecodedFile::Kind::Document);
        Assert::AreEqual (std::wstring (L"Infocom saved game"), GetText (file.paragraphs[0]));
        Assert::AreEqual (std::wstring (L"Position 2   release 5: hall, score 7, 12 moves"), GetText (file.paragraphs[2]));
        Assert::IsTrue   (file.paragraphs[2].runs[0].bold, L"The position's number is bold");
    }


    TEST_METHOD (BlankDisk_IsNotASave)
    {
        std::vector<Byte>  blank (InfocomSaveBuilder::kDiskBytes, 0);
        DecodedFile        file;
        HRESULT            decoded = DiskSaveTable::DecodeInfocomSave (blank, file);



        Assert::IsNull (DiskSaveTable::Find (blank));
        Assert::IsTrue (FAILED (decoded));
    }
};
