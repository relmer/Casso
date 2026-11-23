#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/DiskSaveTable.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/Ultima1Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1SaveTests
//
//  A player disk built here in the layout Ultima I: The Beginning writes: a
//  ProDOS disk with U1.VARS for a female dwarf thief at column 39, row 33.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Ultima1SaveTests)
{
public:

    static void PutWord (std::vector<Byte> & vars, size_t at, int value)
    {
        vars[at]     = (Byte) (value & 0xFF);
        vars[at + 1] = (Byte) (value >> 8);
    }


    static std::vector<Byte> MakeVars()
    {
        std::vector<Byte>  vars (458, 0);
        const char       * name = "Casso";
        int                words[] = { 150, 25, 25, 17, 10, 13, 15, 100, 3, 4 };

        vars[0x06] = 39;
        vars[0x07] = 33;
        vars[0x4A] = 1;

        for (size_t i = 0; name[i] != 0; i++)
        {
            vars[0x4B + i] = (Byte) name[i];
        }

        for (size_t i = 0; i < std::size (words); i++)
        {
            PutWord (vars, 0x59 + i * 2, words[i]);
        }

        return vars;
    }


    static std::vector<Byte> MakeDisk (const std::vector<Byte> & vars)
    {
        std::vector<Byte>  disk (143360, 0);
        std::vector<Byte>  written;
        FilePayload        payload;

        AssertSucceeded (ProDosSkeleton::Write (disk, "U1.PLAYER"));

        payload.bytes          = vars;
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x1000;
        payload.hasLoadAddress = true;

        AssertSucceeded (ProDosVolume (disk).Write (FilePath::Parse ("U1.VARS"), payload, written));

        return written;
    }


    TEST_METHOD (TheCharacterIsParsed)
    {
        Ultima1Save::Character  c;

        Assert::IsTrue   (Ultima1Save::TryParse (MakeVars(), c));
        Assert::AreEqual (std::wstring (L"Casso"), c.name);
        Assert::IsTrue   (c.isFemale);
        Assert::AreEqual (std::wstring (L"Dwarf"), Ultima1Save::GetRaceName (c.race));
        Assert::AreEqual (std::wstring (L"Thief"), Ultima1Save::GetClassName (c.characterClass));
        Assert::AreEqual (150, c.hitPoints);
        Assert::AreEqual (15,  c.intelligence);
        Assert::AreEqual (100, c.gold);
        Assert::AreEqual (39,  c.x);
        Assert::AreEqual (33,  c.y);
    }


    TEST_METHOD (ARaceOutOfRangeIsNotACharacter)
    {
        std::vector<Byte>       vars = MakeVars();
        Ultima1Save::Character  c;

        PutWord (vars, 0x69, 9);

        Assert::IsFalse (Ultima1Save::TryParse (vars, c));
    }


    TEST_METHOD (TheSaveTableFindsAPlayerDisk)
    {
        std::vector<Byte>             disk  = MakeDisk (MakeVars());
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

        Assert::IsTrue (text.find (L"Ultima I: The Beginning") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Dwarf Thief, female") != std::wstring::npos);
        Assert::IsTrue (text.find (L"column 39, row 33") != std::wstring::npos);
    }
};
