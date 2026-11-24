#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/DiskSaveTable.h"
#include "Machines/Apple2/Common/Ultima2Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2SaveTests
//
//  A player disk built here with the character sector Ultima II writes to
//  track 4, sector 8: a female dwarf thief at column 18, row 20.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Ultima2SaveTests)
{
public:

    static std::vector<Byte> MakeDisk()
    {
        static constexpr Byte  kRecord[] =
        {
            0x00, 0x4E, 0x00, 0x01, 0xC3, 0xC1, 0xD3, 0xD3, 0xCF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0xC6, 0x03, 0x02, 0x02, 0x00, 0x25, 0x25, 0x18, 0x22, 0x10, 0x15, 0x04,
            0x00, 0x03, 0x99, 0x02, 0x00, 0x00, 0x04, 0x00, 0x12, 0x14, 0x00, 0xD1,
        };
        std::vector<Byte>  disk (Ultima2Save::kImageBytes, 0);

        std::copy (std::begin (kRecord), std::end (kRecord), disk.begin() + Ultima2Save::kRecordAt);

        return disk;
    }


    TEST_METHOD (TheCharacterIsParsed)
    {
        std::vector<Byte>       disk = MakeDisk();
        Ultima2Save::Character  c;

        Assert::IsTrue   (Ultima2Save::TryParse (std::span<const Byte> (disk).subspan (Ultima2Save::kRecordAt, Ultima2Save::kRecordBytes), c));
        Assert::AreEqual (std::wstring (L"CASSO"), c.name);
        Assert::IsTrue   (c.isFemale);
        Assert::AreEqual (std::wstring (L"Dwarf"), Ultima2Save::GetRaceName (c.race));
        Assert::AreEqual (std::wstring (L"Thief"), Ultima2Save::GetClassName (c.characterClass));
        Assert::AreEqual (25,  c.strength);
        Assert::AreEqual (22,  c.charisma);
        Assert::AreEqual (400, c.hitPoints);
        Assert::AreEqual (399, c.food);
        Assert::AreEqual (400, c.gold);
        Assert::AreEqual (18,  c.x);
        Assert::AreEqual (20,  c.y);
    }


    TEST_METHOD (TheInventoryIsListed)
    {
        std::vector<Byte>       disk   = MakeDisk();
        size_t                  at     = Ultima2Save::kRecordAt;
        Ultima2Save::Character  c;

        disk[at + 0x32] = 0x12;
        disk[at + 0x45] = 0x01;
        disk[at + 0x66] = 0x01;
        disk[at + 0xAD] = 0x02;

        Assert::IsTrue   (Ultima2Save::TryParse (std::span<const Byte> (disk).subspan (at, Ultima2Save::kRecordBytes), c));
        Assert::AreEqual (12, c.torches);
        Assert::AreEqual ((size_t) 3, c.inventory.size());
        Assert::AreEqual (std::wstring (L"Dagger"),    c.inventory[0].first);
        Assert::AreEqual (std::wstring (L"Leather"),   c.inventory[1].first);
        Assert::AreEqual (std::wstring (L"Skull key"), c.inventory[2].first);
        Assert::AreEqual (2,                           c.inventory[2].second);
    }


    TEST_METHOD (ABlankDiskIsNotFound)
    {
        Assert::IsFalse (Ultima2Save::IsFound (std::vector<Byte> (Ultima2Save::kImageBytes, 0)));
    }


    TEST_METHOD (TheSaveTableFindsAPlayerDisk)
    {
        std::vector<Byte>             disk  = MakeDisk();
        const DiskSaveTable::Entry  * entry = DiskSaveTable::Find (disk);
        DecodedFile                   decoded;
        std::wstring                  text;

        Assert::IsNotNull (entry);
        AssertSucceeded (entry->decode (disk, {}, decoded));

        for (const Paragraph & paragraph : decoded.paragraphs)
        {
            for (const Run & run : paragraph.runs)
            {
                text += run.text;
            }
        }

        Assert::IsTrue (text.find (L"Ultima II: Revenge of the Enchantress") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Dwarf Thief, female") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Food 399") != std::wstring::npos);
    }
};
