#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/DiskSaveTable.h"
#include "Machines/Apple2/Common/Ultima3Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3SaveTests
//
//  A player disk built here in the layout Ultima III writes: a party of
//  two at column 6, row 13, from roster slots 3 and 1; the two members'
//  characters; a roster with those two and one more; and a map of grass
//  with water along its top row and a town under the party.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Ultima3SaveTests)
{
public:

    static constexpr size_t  kPartyAt   = 0x3500;
    static constexpr size_t  kMembersAt = 0x3600;
    static constexpr size_t  kRosterAt  = 0x3700;
    static constexpr size_t  kMapAt     = 0x3D00;
    static constexpr size_t  kRecord    = 64;
    static constexpr Byte    kGrass     = 0x04;
    static constexpr Byte    kTown      = 0x18;


    static void PutName (std::vector<Byte> & bytes, size_t at, const char * name)
    {
        for (size_t i = 0; name[i] != 0; i++)
        {
            bytes[at + i] = (Byte) (name[i] | 0x80);
        }
    }


    //  A character: an elf paladin, or a dwarf cleric, with stats and
    //  amounts in binary-coded decimal.
    static void PutCharacter (std::vector<Byte> & bytes, size_t at, const char * name, bool isPaladin)
    {
        PutName (bytes, at, name);

        bytes[at + 0x11] = 'G' | 0x80;
        bytes[at + 0x12] = 0x20;
        bytes[at + 0x13] = 0x15;
        bytes[at + 0x14] = 0x05;
        bytes[at + 0x15] = 0x10;
        bytes[at + 0x16] = (isPaladin ? 'E' : 'D') | 0x80;
        bytes[at + 0x17] = (isPaladin ? 'P' : 'C') | 0x80;
        bytes[at + 0x18] = (isPaladin ? 'M' : 'F') | 0x80;
        bytes[at + 0x19] = 0x02;
        bytes[at + 0x1A] = 0x01;
        bytes[at + 0x1B] = 0x23;
        bytes[at + 0x1C] = 0x01;
        bytes[at + 0x1D] = 0x50;
        bytes[at + 0x1F] = 0x30;
        bytes[at + 0x21] = 0x35;
        bytes[at + 0x24] = 0x55;
        bytes[at + 0x28] = 2;
        bytes[at + 0x30] = 5;
    }


    static std::vector<Byte> MakeDisk()
    {
        std::vector<Byte>  bytes (Ultima3Save::kImageBytes, 0);



        bytes[kPartyAt + 1] = 2;
        bytes[kPartyAt + 3] = 6;
        bytes[kPartyAt + 4] = 13;
        bytes[kPartyAt + 6] = 3;
        bytes[kPartyAt + 7] = 1;

        PutCharacter (bytes, kMembersAt,               "ROB",    true);
        PutCharacter (bytes, kMembersAt + kRecord,     "MANDI",  false);
        PutCharacter (bytes, kRosterAt,                "MANDI",  false);
        PutCharacter (bytes, kRosterAt + kRecord,      "LOKIL",  false);
        PutCharacter (bytes, kRosterAt + 2 * kRecord,  "ROB",    true);

        std::fill (bytes.begin() + kMapAt + 64, bytes.begin() + kMapAt + 64 * 64, kGrass);
        bytes[kMapAt + 13 * 64 + 6] = kTown;

        return bytes;
    }


    static std::wstring GetText (const DecodedFile & decoded)
    {
        std::wstring  text;

        for (const Paragraph & paragraph : decoded.paragraphs)
        {
            for (const Run & run : paragraph.runs)
            {
                text += run.text;
            }

            text += L"\n";
        }

        return text;
    }


    static size_t CountOf (const std::wstring & text, const wchar_t * what)
    {
        size_t  count = 0;

        for (size_t at = text.find (what); at != std::wstring::npos; at = text.find (what, at + 1))
        {
            count++;
        }

        return count;
    }


    TEST_METHOD (APlayerDiskIsFound)
    {
        std::vector<Byte>  disk = MakeDisk();

        Assert::IsTrue (Ultima3Save::IsFound (disk));
    }


    TEST_METHOD (ADiskWithNoPartyIsNotFound)
    {
        std::vector<Byte>  disk = MakeDisk();

        disk[kPartyAt + 1] = 0;

        Assert::IsFalse (Ultima3Save::IsFound (disk), L"the scenario side has no party, and nothing to show");
    }


    TEST_METHOD (ADiskWhoseMapIsNotTerrainIsNotFound)
    {
        std::vector<Byte>  disk = MakeDisk();

        std::fill (disk.begin() + kMapAt, disk.begin() + kMapAt + 64 * 64, (Byte) 0x9D);

        Assert::IsFalse (Ultima3Save::IsFound (disk));
    }


    TEST_METHOD (ADiskOfTheWrongSizeIsNotFound)
    {
        std::vector<Byte>  disk = MakeDisk();

        disk.resize (disk.size() / 2);

        Assert::IsFalse (Ultima3Save::IsFound (disk));
    }


    TEST_METHOD (ThePartyIsReadWithItsSlotsFromZero)
    {
        std::vector<Byte>   disk = MakeDisk();
        Ultima3Save::Party  party;

        Assert::IsTrue (Ultima3Save::TryReadParty (disk, party));
        Assert::AreEqual (6,  party.x);
        Assert::AreEqual (13, party.y);
        Assert::AreEqual (2,  party.size);
        Assert::AreEqual ((size_t) 2, party.slots.size());
        Assert::AreEqual (2, party.slots[0]);
        Assert::AreEqual (0, party.slots[1]);
    }


    TEST_METHOD (ACharacterIsReadAsDecimal)
    {
        std::vector<Byte>       disk = MakeDisk();
        Ultima3Save::Character  character;

        Assert::IsTrue (Ultima3Save::TryReadCharacter (std::span<const Byte> (disk).subspan (kMembersAt, kRecord), character));
        Assert::AreEqual (std::wstring (L"ROB"), character.name);
        Assert::AreEqual (20,  character.strength);
        Assert::AreEqual (15,  character.dexterity);
        Assert::AreEqual (123, character.hitPoints);
        Assert::AreEqual (150, character.maxHitPoints);
        Assert::AreEqual (30,  character.experience);
        Assert::AreEqual (35,  character.food);
        Assert::AreEqual (55,  character.gold);
        Assert::AreEqual (L'P', character.characterClass);
    }


    TEST_METHOD (AnEmptySlotIsNotACharacter)
    {
        std::vector<Byte>       disk = MakeDisk();
        Ultima3Save::Character  character;

        Assert::IsFalse (Ultima3Save::TryReadCharacter (std::span<const Byte> (disk).subspan (kRosterAt + 5 * kRecord, kRecord), character));
    }


    TEST_METHOD (TheDocumentListsTheMembersAndTheRestOfTheRoster)
    {
        std::vector<Byte>  disk = MakeDisk();
        DecodedFile        decoded;
        std::wstring       error;
        std::wstring       text;

        AssertSucceeded (Ultima3Save::Decode (disk, {}, decoded, error));
        text = GetText (decoded);

        Assert::IsTrue (decoded.kind == DecodedFile::Kind::Document);
        Assert::IsTrue (decoded.format == FileFormat::UltimaSave);
        Assert::AreEqual ((size_t) 1, CountOf (text, L"ROB"),   L"a member's roster slot isn't listed again");
        Assert::AreEqual ((size_t) 1, CountOf (text, L"MANDI"), L"a member's roster slot isn't listed again");
        Assert::AreEqual ((size_t) 1, CountOf (text, L"LOKIL"));
        Assert::IsTrue (text.find (L"Elf paladin, male") != std::wstring::npos);
        Assert::IsTrue (text.find (L"HP 123 / 150") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Leather, Bow") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Party") < text.find (L"Roster"));
    }


    TEST_METHOD (TheMapComesFirstWithThePartyMarked)
    {
        std::vector<Byte>  disk      = MakeDisk();
        DecodedFile        decoded;
        std::wstring       error;
        size_t             pictureAt = 0;
        int                tile      = 5;
        int                center    = 2;

        AssertSucceeded (Ultima3Save::Decode (disk, {}, decoded, error));

        while (pictureAt < decoded.paragraphs.size() && decoded.paragraphs[pictureAt].picture.empty())
        {
            pictureAt++;
        }

        Assert::IsTrue (pictureAt < decoded.paragraphs.size());
        Assert::IsTrue (pictureAt <= 2, L"the map comes right after the title");

        const Paragraph &  map = decoded.paragraphs[pictureAt];

        Assert::AreEqual (64 * tile, map.width);
        Assert::AreEqual (0xFFFFFFFFu, map.picture[(size_t) (13 * tile + center) * (size_t) map.width + (size_t) (6 * tile + center)], L"the party's tile is white");
        Assert::AreEqual (0xFF5AA046u, map.picture[(size_t) (20 * tile + center) * (size_t) map.width + (size_t) (20 * tile + center)], L"grass is green");
        Assert::AreEqual (0xFF2846A0u, map.picture[(size_t) center * (size_t) map.width + (size_t) center], L"water is blue");
    }


    TEST_METHOD (AnUnreadableSectorsRowsAreStriped)
    {
        std::vector<Byte>  disk       = MakeDisk();
        bool               lost[560]  = {};
        DecodedFile        decoded;
        std::wstring       error;
        size_t             pictureAt  = 0;
        int                tile       = 5;
        uint32_t           pixel      = 0;

        //  Track 4 sector 0 is the map's rows 12 to 15.
        lost[4 * 16] = true;

        AssertSucceeded (Ultima3Save::Decode (disk, std::span<const bool> (lost), decoded, error));

        while (decoded.paragraphs[pictureAt].picture.empty())
        {
            pictureAt++;
        }

        pixel = decoded.paragraphs[pictureAt].picture[(size_t) (14 * tile) * (size_t) decoded.paragraphs[pictureAt].width + (size_t) (30 * tile)];

        Assert::IsTrue (pixel == 0xFF282828u || pixel == 0xFFC8C8C8u, L"a lost row is striped");
    }


    TEST_METHOD (EachMapTileHasALabelWithItsTerrainAndPlace)
    {
        std::vector<Byte>  disk = MakeDisk();
        DecodedFile        decoded;
        std::wstring       error;
        size_t             at   = 0;

        AssertSucceeded (Ultima3Save::Decode (disk, {}, decoded, error));

        while (decoded.paragraphs[at].picture.empty())
        {
            at++;
        }

        const Paragraph &  map = decoded.paragraphs[at];

        Assert::AreEqual (5, map.cellWidth);
        Assert::AreEqual ((size_t) 64 * 64, map.cellLabels.size());
        Assert::AreEqual (std::wstring (L"Water, column 3, row 0"),           map.labels[map.cellLabels[3]]);
        Assert::AreEqual (std::wstring (L"Grass, column 20, row 30"),         map.labels[map.cellLabels[30 * 64 + 20]]);
        Assert::AreEqual (std::wstring (L"The party, column 6, row 13"),   map.labels[map.cellLabels[13 * 64 + 6]]);
    }


    TEST_METHOD (TheSaveTableFindsAPlayerDisk)
    {
        std::vector<Byte>             disk  = MakeDisk();
        const DiskSaveTable::Entry  * entry = DiskSaveTable::Find (disk);
        DecodedFile                   decoded;

        Assert::IsNotNull (entry);
        AssertSucceeded (entry->decode (disk, decoded));
        Assert::IsTrue (decoded.format == FileFormat::UltimaSave);
    }


    TEST_METHOD (ADiskWithNoSaveFailsToDecode)
    {
        std::vector<Byte>  disk (Ultima3Save::kImageBytes, 0);
        DecodedFile        decoded;
        std::wstring       error;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), Ultima3Save::Decode (disk, {}, decoded, error));
        Assert::IsFalse (error.empty());
    }
};
