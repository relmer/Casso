#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../Inspector/RepairFixtures.h"
#include "FileTypes/DiskSaveTable.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/Ultima1981Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981SaveTests
//
//  A player disk built here in the layout the 1981 Ultima writes: a DOS 3.3
//  disk with the file NAME=CASSO, and Applesoft variables on track 26 for a
//  dwarf thief at column 40, row 34 of the second continent, whose map is
//  the file BTERRA1.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Ultima1981SaveTests)
{
public:

    //  Applesoft floats: 100, 25, 18, 12, 10, 15, 3, 4, 96.5, 1, 40 and 34.
    static constexpr Byte  k100[]  = { 0x87, 0x48, 0, 0, 0 };
    static constexpr Byte  k25[]   = { 0x85, 0x48, 0, 0, 0 };
    static constexpr Byte  k18[]   = { 0x85, 0x10, 0, 0, 0 };
    static constexpr Byte  k12[]   = { 0x84, 0x40, 0, 0, 0 };
    static constexpr Byte  k10[]   = { 0x84, 0x20, 0, 0, 0 };
    static constexpr Byte  k15[]   = { 0x84, 0x70, 0, 0, 0 };
    static constexpr Byte  k3[]    = { 0x82, 0x40, 0, 0, 0 };
    static constexpr Byte  k4[]    = { 0x83, 0x00, 0, 0, 0 };
    static constexpr Byte  k96[]   = { 0x87, 0x41, 0, 0, 0 };
    static constexpr Byte  k1[]    = { 0x81, 0x00, 0, 0, 0 };
    static constexpr Byte  k40[]   = { 0x86, 0x20, 0, 0, 0 };
    static constexpr Byte  k34[]   = { 0x86, 0x08, 0, 0, 0 };
    static constexpr Byte  k0[]    = { 0x00, 0x00, 0, 0, 0 };
    static constexpr Byte  k2[]    = { 0x82, 0x00, 0, 0, 0 };


    static void Append (std::vector<Byte> & image, std::initializer_list<Byte> bytes)
    {
        image.insert (image.end(), bytes);
    }


    static void AppendFloat (std::vector<Byte> & image, const Byte (&value)[5])
    {
        image.insert (image.end(), std::begin (value), std::end (value));
    }


    static void AppendVariable (std::vector<Byte> & image, char first, char second, const Byte (&value)[5])
    {
        Append (image, { (Byte) first, (Byte) second });
        AppendFloat (image, value);
    }


    static std::vector<Byte> MakeDisk()
    {
        std::vector<Byte>  disk = RepairFixtures::MakeDos33();
        std::vector<Byte>  written;
        std::vector<Byte>  image (0x15, 0);
        FilePayload        payload;
        FilePayload        terrain;

        payload.bytes = { 0x8D };
        payload.type  = 0x00;

        AssertSucceeded (Dos33Volume (disk).Write (FilePath::Parse ("NAME=CASSO"), payload, written));
        disk = std::move (written);

        //  Grass everywhere, with a town at the top left.
        terrain.bytes.assign (4096, 0x10);
        terrain.bytes[0]       = 0x62;
        terrain.type           = 0x04;
        terrain.loadAddress    = 0x1000;
        terrain.hasLoadAddress = true;

        AssertSucceeded (Dos33Volume (disk).Write (FilePath::Parse ("BTERRA1"), terrain, written));
        disk = std::move (written);

        AppendVariable (image, 'X',  0,  k1);
        AppendVariable (image, 'F', 'O', k96);
        AppendVariable (image, 'E', 'P', k1);
        AppendVariable (image, 'T', 'X', k40);
        AppendVariable (image, 'T', 'Y', k34);
        AppendVariable (image, 'F', 'A', k1);
        Append (image, { 'C', 0x00, 0x39, 0x00, 0x01, 0x00, 0x0A });

        for (const auto * value : { &k100, &k25, &k25, &k18, &k12, &k10, &k15, &k100, &k3, &k4 })
        {
            AppendFloat (image, *value);
        }

        //  PW: hands, then two daggers.
        Append (image, { 'P', 'W', 0x16, 0x00, 0x01, 0x00, 0x02 });
        AppendFloat (image, k0);
        AppendFloat (image, k2);
        AppendFloat (image, k0);

        for (size_t i = 0; i < image.size(); i++)
        {
            size_t  sector = 11 - i / 256;

            disk[(26 * 16 + sector) * 256 + i % 256] = image[i];
        }

        return disk;
    }


    TEST_METHOD (AnApplesoftFloatIsRead)
    {
        Assert::AreEqual (100.0, Ultima1981Save::ReadFloat (k100, 0));
        Assert::AreEqual (96.5,  Ultima1981Save::ReadFloat (k96,  0));
        Assert::AreEqual (0.0,   Ultima1981Save::ReadFloat (std::vector<Byte> (5, 0), 0));
    }


    TEST_METHOD (TheCharacterIsParsed)
    {
        Ultima1981Save::Character  c;

        Assert::IsTrue   (Ultima1981Save::TryParse (MakeDisk(), c));
        Assert::AreEqual (std::wstring (L"CASSO"), c.name);
        Assert::AreEqual (std::wstring (L"Dwarf"), Ultima1981Save::GetRaceName (c.race));
        Assert::AreEqual (std::wstring (L"Thief"), Ultima1981Save::GetClassName (c.characterClass));
        Assert::AreEqual (100, c.hitPoints);
        Assert::AreEqual (18,  c.stamina);
        Assert::AreEqual (15,  c.intelligence);
        Assert::AreEqual (100, c.gold);
        Assert::AreEqual (96,  c.food);
        Assert::AreEqual (40,  c.x);
        Assert::AreEqual (34,  c.y);
        Assert::AreEqual (1,   c.continent);
        Assert::AreEqual ((size_t) 1, c.inventory.size());
        Assert::AreEqual (std::wstring (L"Dagger"), c.inventory[0].first);
        Assert::AreEqual (2, c.inventory[0].second);
    }


    TEST_METHOD (ADosDiskWithoutTheVariablesIsNotFound)
    {
        Assert::IsFalse (Ultima1981Save::IsFound (RepairFixtures::MakeDos33()));
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

        Assert::IsTrue (text.find (L"Ultima (1981)") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Dwarf Thief") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Food 96") != std::wstring::npos);
        Assert::IsTrue (text.find (L"The Lands of the Feudal Lords") != std::wstring::npos);
    }


    TEST_METHOD (TheContinentIsDrawnFromItsTerrainFile)
    {
        std::vector<Byte>  disk = MakeDisk();
        DecodedFile        decoded;
        std::wstring       error;
        const Paragraph  * map  = nullptr;

        AssertSucceeded (Ultima1981Save::Decode (disk, {}, decoded, error));

        for (const Paragraph & paragraph : decoded.paragraphs)
        {
            map = paragraph.picture.empty() ? map : &paragraph;
        }

        Assert::IsNotNull (map);
        Assert::IsTrue   (map->pingX >= 0);
        Assert::AreEqual (std::wstring (L"Town"), map->tips[0].lines.back().second);
        Assert::AreEqual (std::wstring (L"Grass"), map->tips[1].lines.back().second);
    }
};
