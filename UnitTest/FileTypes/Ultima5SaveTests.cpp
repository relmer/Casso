#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../Inspector/RepairFixtures.h"
#include "FileTypes/DiskSaveTable.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/Ultima5Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5SaveTests
//
//  A Britannia disk built here in the layout Ultima V writes: a ProDOS disk
//  whose blocks 152 to 279 are a world of deep water with grass at column
//  74, row 9, and whose catalog has ROSTER, with two characters, food and
//  gold, and PRTY.DATA, with the party on that grass and the date.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Ultima5SaveTests)
{
public:

    static constexpr Byte  kDeepWater = 0x01;
    static constexpr Byte  kGrass     = 0x05;
    static constexpr int   kPartyX    = 74;
    static constexpr int   kPartyY    = 9;


    static void PutCharacter (std::vector<Byte> & roster, int index, const char * name, bool inParty, bool isFemale, char characterClass)
    {
        size_t  length = strlen (name);
        size_t  at     = 0x80 + (size_t) index * 16;

        for (size_t i = 0; i < length; i++)
        {
            roster[(size_t) index * 8 + i] = (Byte) (i + 1 < length ? name[i] | 0x80 : name[i]);
        }

        roster[at]      = inParty ? 0x00 : 0xFF;
        roster[at + 1]  = isFemale ? 0x3D : 0x3E;
        roster[at + 2]  = (Byte) characterClass;
        roster[at + 3]  = 'G';
        roster[at + 4]  = 0x21;
        roster[at + 5]  = 0x19;
        roster[at + 6]  = 0x15;
        roster[at + 7]  = 0x07;
        roster[at + 8]  = 0x01;
        roster[at + 9]  = 0x20;
        roster[at + 10] = 0x02;
        roster[at + 11] = 0x40;
        roster[at + 12] = 0x03;
        roster[at + 13] = 0x50;
        roster[at + 14] = 0x03;
        roster[at + 15] = 0xFF;
    }


    static void PutFile (std::vector<Byte> & disk, const char * name, const std::vector<Byte> & bytes)
    {
        FilePayload        payload;
        std::vector<Byte>  written;

        payload.bytes          = bytes;
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x1000;
        payload.hasLoadAddress = true;

        AssertSucceeded (ProDosVolume (disk).Write (FilePath::Parse (name), payload, written));
        disk = std::move (written);
    }


    static void PutTile (std::vector<Byte> & disk, int x, int y, Byte tile)
    {
        int  chunk = (y / 16) * 16 + (x / 16);

        disk[ProDosSkeleton::GetBlockByteOffset (disk.size(), 152 + chunk / 2, (size_t) (chunk % 2) * 256 + (size_t) (y % 16) * 16 + (size_t) (x % 16))] = tile;
    }


    static std::vector<Byte> MakeDisk()
    {
        std::vector<Byte>  disk   (Ultima5Save::kImageBytes, 0);
        std::vector<Byte>  roster (Ultima5Save::kRosterBytes, 0);
        std::vector<Byte>  party  (Ultima5Save::kPartyBytes, 0);



        AssertSucceeded (ProDosSkeleton::Write (disk, "BRITANNIA"));
        PutCharacter (roster, 0, "CASSO", true,  false, 'A');
        PutCharacter (roster, 1, "IOLO",  false, false, 'B');
        roster[0x180] = 0x02;
        roster[0x181] = 0x34;
        roster[0x182] = 0x06;
        roster[0x183] = 0x60;

        party[0x00] = kPartyX;
        party[0x01] = kPartyY;
        party[0x1A] = 0x01;
        party[0x1B] = 0x39;
        party[0x1C] = 0x04;
        party[0x1D] = 0x07;
        party[0x1E] = 0x09;
        party[0x1F] = 0x05;

        PutFile (disk, "ROSTER",    roster);
        PutFile (disk, "PRTY.DATA", party);

        for (int y = 0; y < Ultima5Save::kWorldSide; y++)
        {
            for (int x = 0; x < Ultima5Save::kWorldSide; x++)
            {
                PutTile (disk, x, y, kDeepWater);
            }
        }

        PutTile (disk, kPartyX, kPartyY, kGrass);

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


    TEST_METHOD (ABritanniaDiskIsFound)
    {
        Assert::IsTrue (Ultima5Save::IsFound (MakeDisk()));
    }


    TEST_METHOD (AProDosDiskWithoutTheWorldIsNotFound)
    {
        Assert::IsFalse (Ultima5Save::IsFound (RepairFixtures::MakeProDos()));
    }


    TEST_METHOD (TheWorldIsReadAChunkAHalfBlock)
    {
        std::vector<Byte>  disk = MakeDisk();

        Assert::AreEqual (kGrass,     Ultima5Save::GetTile (disk, kPartyX, kPartyY));
        Assert::AreEqual (kDeepWater, Ultima5Save::GetTile (disk, kPartyX + 1, kPartyY));
    }


    TEST_METHOD (ANameEndsAtItsLowBitLetter)
    {
        std::vector<Byte>       roster (Ultima5Save::kRosterBytes, 0);
        Ultima5Save::Character  character;

        PutCharacter (roster, 0, "IOLO", true, false, 'B');
        roster[4] = 'X' | 0x80;

        Assert::IsTrue   (Ultima5Save::TryReadCharacter (roster, 0, character));
        Assert::AreEqual (std::wstring (L"IOLO"), character.name);
        Assert::AreEqual (240, character.maxHitPoints);
        Assert::AreEqual (350, character.experience);
        Assert::IsFalse  (Ultima5Save::TryReadCharacter (roster, 2, character), L"an empty slot");
    }


    TEST_METHOD (TheMapHasThePartyMarkedAndATipForEachTile)
    {
        std::vector<Byte>  disk = MakeDisk();
        DecodedFile        decoded;
        std::wstring       error;

        AssertSucceeded (Ultima5Save::Decode (disk, {}, decoded, error));

        const Paragraph &   map   = FindMap (decoded);
        const PictureTip *  party = map.GetTipAt (kPartyX * 2, kPartyY * 2);

        Assert::AreEqual (512, map.width);
        Assert::AreEqual (kPartyX * 2 + 1, map.pingX);
        Assert::IsTrue   (map.canZoom);
        Assert::IsTrue   ((&map)[1].isLegend, L"the legend follows the map");
        Assert::IsNotNull (party);
        Assert::AreEqual (std::wstring (L"Column 74, row 9"), party->lines[0].second);
        Assert::AreEqual (std::wstring (L"The party"),        party->lines[1].second);
        Assert::AreEqual (std::wstring (L"Grass"),            party->lines[2].second);
    }


    TEST_METHOD (ThePartyAndRosterAreTables)
    {
        std::vector<Byte>  disk = MakeDisk();
        DecodedFile        decoded;
        std::wstring       error;
        std::wstring       text;

        AssertSucceeded (Ultima5Save::Decode (disk, {}, decoded, error));
        text = GetText (decoded);

        Assert::IsTrue (text.find (L"CASSO") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Avatar, male") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Bard, male") != std::wstring::npos);
        Assert::IsTrue (text.find (L"HP 120 / 240") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Str 21   Dex 19   Int 15") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Food 234   Gold 660") != std::wstring::npos);
        Assert::IsTrue (text.find (L"on 4-7-139 at 9:05") != std::wstring::npos);
        Assert::IsTrue (text.find (L"Party") < text.find (L"Roster"));
    }


    TEST_METHOD (TheSaveTableFindsABritanniaDisk)
    {
        std::vector<Byte>             disk  = MakeDisk();
        const DiskSaveTable::Entry  * entry = DiskSaveTable::Find (disk);
        DecodedFile                   decoded;

        Assert::IsNotNull (entry);
        AssertSucceeded (entry->decode (disk, {}, decoded));
        Assert::IsTrue (decoded.format == FileFormat::UltimaSave);
    }
};
