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


    //  A known game's title and subtitle are the heading.
    TEST_METHOD (InfocomSaveDisk_HeadingIsTheGamesTitle)
    {
        std::vector<Byte>  memory = InfocomSaveBuilder::MakeMemory (0);
        DecodedFile        file;



        InfocomSaveBuilder::PutWord (memory, 0x02, 88);
        memcpy (&memory[0x12], "840726", 6);

        std::vector<Byte>  disk = InfocomSaveBuilder::MakeDisk (memory, 0);

        AssertSucceeded  (DiskSaveTable::Find (disk)->decode (disk, {}, file));
        Assert::AreEqual (std::wstring (L"Zork I: The Great Underground Empire saved game, release 88, serial 840726"), GetText (file.paragraphs[0]));
        Assert::IsTrue   (file.paragraphs[0].runs[0].bold && !file.paragraphs[0].runs[1].bold, L"The title is bold");
    }


    TEST_METHOD (InfocomSaveDisk_IsFoundAndListsEachPosition)
    {
        std::vector<Byte>             disk  = InfocomSaveBuilder::MakeDisk (InfocomSaveBuilder::MakeMemory (0), 2);
        const DiskSaveTable::Entry  * entry = DiskSaveTable::Find (disk);
        DecodedFile                   file;



        Assert::IsNotNull (entry);
        AssertSucceeded   (entry->decode (disk, {}, file));

        Assert::IsTrue   (file.kind == DecodedFile::Kind::Document);
        Assert::AreEqual (std::wstring (L"Infocom saved game, release 5, serial 123456"), GetText (file.paragraphs[0]));
        Assert::AreEqual (std::wstring (L"Save position 2"), GetText (file.paragraphs[2]));
        Assert::AreEqual (std::wstring (L"Room\thall"), GetText (file.paragraphs[3]));
        Assert::IsTrue   (file.paragraphs[2].runs[1].bold, L"The position's number is bold");
        Assert::AreEqual (std::wstring (L"Score\t7"), GetText (file.paragraphs[4]));
        Assert::AreEqual (std::wstring (L"Moves\t12"), GetText (file.paragraphs[5]));
        Assert::AreEqual (std::wstring (L"Carrying\tNothing"), GetText (file.paragraphs[7]));
    }


    TEST_METHOD (BlankDisk_IsNotASave)
    {
        std::vector<Byte>  blank (InfocomSaveBuilder::kDiskBytes, 0);
        DecodedFile        file;
        HRESULT            decoded = DiskSaveTable::DecodeInfocomSave (blank, {}, file);



        Assert::IsNull (DiskSaveTable::Find (blank));
        Assert::IsTrue (FAILED (decoded));
    }
};
