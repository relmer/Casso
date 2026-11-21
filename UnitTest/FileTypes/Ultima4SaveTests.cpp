#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../Inspector/RepairFixtures.h"
#include "FileTypes/DiskSaveTable.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/Ultima4Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4SaveTests
//
//  A save disk built here in the layout Ultima IV writes: a DOS 3.3 disk
//  whose tracks 0 to 15 are a world of grass with a town at column 86, row
//  107, and whose catalog has ROST, with two characters and the virtues,
//  and PRTY, with the party at that town.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Ultima4SaveTests)
{
public:

    static constexpr size_t  kRecord = 32;
    static constexpr Byte    kGrass  = 0x04;
    static constexpr Byte    kTown   = 0x0A;
    static constexpr int     kPartyX = 86;
    static constexpr int     kPartyY = 107;


    static void PutCharacter (std::vector<Byte> & roster, size_t at, const char * name, bool isFemale, Byte classIndex)
    {
        for (size_t i = 0; name[i] != 0; i++)
        {
            roster[at + i] = (Byte) (name[i] | 0x80);
        }

        roster[at + 0x10] = isFemale ? 0x7B : 0x5C;
        roster[at + 0x11] = classIndex;
        roster[at + 0x12] = 'G' | 0x80;
        roster[at + 0x13] = 0x21;
        roster[at + 0x14] = 0x26;
        roster[at + 0x15] = 0x16;
        roster[at + 0x16] = 0x16;
        roster[at + 0x18] = 0x03;
        roster[at + 0x1A] = 0x03;
        roster[at + 0x1C] = 0x02;
        roster[at + 0x1D] = 0x40;
        roster[at + 0x1E] = 3;
        roster[at + 0x1F] = 1;
    }


    static void PutFile (std::vector<Byte> & disk, const char * name, const std::vector<Byte> & bytes)
    {
        FilePayload        payload;
        std::vector<Byte>  written;

        payload.bytes          = bytes;
        payload.type           = 0x04;
        payload.loadAddress    = 0x1000;
        payload.hasLoadAddress = true;

        AssertSucceeded (Dos33Volume (disk).Write (FilePath::Parse (name), payload, written));
        disk = std::move (written);
    }


    static std::vector<Byte> MakeDisk()
    {
        std::vector<Byte>  disk   = RepairFixtures::MakeDos33();
        std::vector<Byte>  roster (Ultima4Save::kRosterBytes, 0);
        std::vector<Byte>  party  (Ultima4Save::kPartyBytes, 0);
        size_t             region = (size_t) (kPartyY / 16) * 16 + (size_t) (kPartyX / 16);



        PutCharacter (roster, 0,       "JAMES",  false, 1);
        PutCharacter (roster, kRecord, "MARIAH", true,  0);

        for (size_t i = 0; i < 8; i++)
        {
            roster[0x100 + i] = (Byte) (0x50 + i);
        }

        party[0] = kPartyX;
        party[1] = kPartyY;

        PutFile (disk, "ROST", roster);
        PutFile (disk, "PRTY", party);

        //  The world after the files, which DOS puts from track 17 out.
        std::fill (disk.begin(), disk.begin() + 16 * 16 * 256, kGrass);
        disk[region * 256 + (size_t) (kPartyY % 16) * 16 + (size_t) (kPartyX % 16)] = kTown;

        return disk;
    }


    static std::wstring GetText (const DecodedFile & decoded)
    {
        std::wstring  text;

        for (const Paragraph & paragraph : FlattenRecordTables (decoded.paragraphs))
        {
            for (const Run & run : paragraph.runs)
            {
                text += run.text;
            }

            text += L"\n";
        }

        return text;
    }


    static const Paragraph & FindMap (const DecodedFile & decoded)
    {
        size_t  at = 0;

        while (at + 1 < decoded.paragraphs.size() && decoded.paragraphs[at].picture.empty())
        {
            at++;
        }

        return decoded.paragraphs[at];
    }


    TEST_METHOD (ASaveDiskIsFound)
    {
        Assert::IsTrue (Ultima4Save::IsFound (MakeDisk()));
    }


    TEST_METHOD (ADosDiskWithoutTheWorldIsNotFound)
    {
        Assert::IsFalse (Ultima4Save::IsFound (RepairFixtures::MakeDos33()));
    }


    TEST_METHOD (ADiskWithTheWorldButNoRosterIsNotFound)
    {
        std::vector<Byte>  disk = RepairFixtures::MakeDos33();

        std::fill (disk.begin(), disk.begin() + 16 * 16 * 256, kGrass);

        Assert::IsFalse (Ultima4Save::IsFound (disk));
    }


    TEST_METHOD (TheWorldIsReadARegionASector)
    {
        std::vector<Byte>  disk = MakeDisk();

        Assert::AreEqual (kTown,  Ultima4Save::GetTile (disk, kPartyX, kPartyY));
        Assert::AreEqual (kGrass, Ultima4Save::GetTile (disk, kPartyX + 1, kPartyY));
    }


    TEST_METHOD (TheMapHasThePartyMarkedAndATipForEachTile)
    {
        std::vector<Byte>  disk = MakeDisk();
        DecodedFile        decoded;
        std::wstring       error;

        AssertSucceeded (Ultima4Save::Decode (disk, {}, decoded, error));

        const Paragraph &   map   = FindMap (decoded);
        const PictureTip *  party = map.GetTipAt (kPartyX * 2, kPartyY * 2);

        Assert::AreEqual (512, map.width);
        Assert::AreEqual ((size_t) 256 * 256, map.tips.size());
        Assert::IsNotNull (party);
        Assert::AreEqual (std::wstring (L"Column 86, row 107"), party->lines[0].second);
        Assert::AreEqual (std::wstring (L"The party"),          party->lines[1].second);
        Assert::AreEqual (std::wstring (L"Town"),               party->lines[2].second);
        Assert::AreEqual (0xFFFFFFFFu, map.picture[(size_t) (kPartyY * 2) * 512 + (size_t) (kPartyX * 2)], L"the party's mark is white");
    }


    TEST_METHOD (TheCharactersAreATableWithArmorAndWeaponLabeled)
    {
        std::vector<Byte>  disk = MakeDisk();
        DecodedFile        decoded;
        std::wstring       error;
        std::wstring       text;

        AssertSucceeded (Ultima4Save::Decode (disk, {}, decoded, error));
        text = GetText (decoded);

        Assert::IsTrue (decoded.kind == DecodedFile::Kind::Document);
        Assert::IsTrue (text.find (L"JAMES") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Bard, male") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Mage, female") != std::wstring::npos);
        Assert::IsTrue (text.find (L"HP 300 / 300") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Level 3   Exp 240") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Armor: Cloth   Weapon: Sling") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Honesty 50") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Humility 57") != std::wstring::npos);
    }


    TEST_METHOD (TheSaveTableFindsASaveDisk)
    {
        std::vector<Byte>             disk  = MakeDisk();
        const DiskSaveTable::Entry  * entry = DiskSaveTable::Find (disk);
        DecodedFile                   decoded;

        Assert::IsNotNull (entry);
        AssertSucceeded (entry->decode (disk, {}, decoded));
        Assert::IsTrue (decoded.format == FileFormat::UltimaSave);
    }
};
