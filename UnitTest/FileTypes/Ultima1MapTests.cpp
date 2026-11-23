#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "FileTypes/DiskSaveTable.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/Ultima1Map.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1MapTests
//
//  A GEN file built here in the layout the game disk of Ultima I: The
//  Beginning has: a castle at the top left of each continent and grass
//  everywhere else, run-length coded after the table at kTableAt.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Ultima1MapTests)
{
public:

    static constexpr Byte  kGrass  = 1;
    static constexpr Byte  kCastle = 4;


    static std::vector<Byte> MakeGen (size_t tilesInLast = (size_t) Ultima1Map::kMapSide * Ultima1Map::kMapSide)
    {
        constexpr size_t   kMostRun = 31;
        std::vector<Byte>  gen (Ultima1Map::kTableAt + Ultima1Map::kContinents * 2, 0);

        for (int k = 0; k < Ultima1Map::kContinents; k++)
        {
            size_t  offset = gen.size() - Ultima1Map::kTableAt;
            size_t  left   = (k == Ultima1Map::kContinents - 1 ? tilesInLast : (size_t) Ultima1Map::kMapSide * Ultima1Map::kMapSide) - 1;

            gen[Ultima1Map::kTableAt + (size_t) k * 2]     = (Byte) (offset & 0xFF);
            gen[Ultima1Map::kTableAt + (size_t) k * 2 + 1] = (Byte) (offset >> 8);
            gen.push_back ((Byte) (1 << 3 | kCastle));

            while (left > 0)
            {
                size_t  run = (std::min) (left, kMostRun);

                gen.push_back ((Byte) (run << 3 | kGrass));
                left -= run;
            }

            gen.push_back (0);
        }

        return gen;
    }


    static std::vector<Byte> MakeDisk (const char * volumeName, const char * fileName, const std::vector<Byte> & bytes)
    {
        std::vector<Byte>  disk (143360, 0);
        std::vector<Byte>  written;
        FilePayload        payload;

        AssertSucceeded (ProDosSkeleton::Write (disk, volumeName));

        payload.bytes          = bytes;
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x1000;
        payload.hasLoadAddress = true;

        AssertSucceeded (ProDosVolume (disk).Write (FilePath::Parse (fileName), payload, written));

        return written;
    }


    static std::vector<Byte> MakeVars()
    {
        std::vector<Byte>  vars (458, 0);

        vars[0x02] = 2;
        vars[0x06] = 5;
        vars[0x07] = 9;
        vars[0x4B] = 'C';
        vars[0x59] = 150;
        vars[0x69] = 1;
        vars[0x6B] = 1;

        return vars;
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
        }

        return text;
    }


    TEST_METHOD (EachContinentIsUnpacked)
    {
        std::array<Ultima1Map::Continent, Ultima1Map::kContinents>  continents;

        Assert::IsTrue (Ultima1Map::TryUnpack (MakeGen(), continents));

        for (const Ultima1Map::Continent & continent : continents)
        {
            Assert::AreEqual ((size_t) Ultima1Map::kMapSide * Ultima1Map::kMapSide, continent.size());
            Assert::AreEqual (kCastle, continent[0]);
            Assert::AreEqual (kGrass,  continent[1]);
        }
    }


    TEST_METHOD (AShortContinentIsNotAMap)
    {
        std::array<Ultima1Map::Continent, Ultima1Map::kContinents>  continents;

        Assert::IsFalse (Ultima1Map::TryUnpack (MakeGen (100), continents));
        Assert::IsFalse (Ultima1Map::TryUnpack (std::vector<Byte> (64, 0), continents));
    }


    TEST_METHOD (TheGameDiskShowsFourContinents)
    {
        std::vector<Byte>             disk  = MakeDisk ("U1", "GEN", MakeGen());
        const DiskSaveTable::Entry  * entry = DiskSaveTable::Find (disk);
        DecodedFile                   decoded;
        size_t                        maps  = 0;
        std::wstring                  text;

        Assert::IsNotNull (entry);
        AssertSucceeded (entry->decode (disk, {}, decoded));

        for (const Paragraph & paragraph : decoded.paragraphs)
        {
            maps += paragraph.picture.empty() ? 0 : 1;
        }

        text = GetText (decoded);

        Assert::AreEqual ((size_t) Ultima1Map::kContinents, maps);
        Assert::IsTrue (text.find (L"The Lands of Lord British") != std::wstring::npos);
        Assert::IsTrue (text.find (L"The Lands of Danger and Despair") != std::wstring::npos);
    }


    TEST_METHOD (APlayerDiskBesideTheGameDiskShowsItsContinent)
    {
        FakeDiskFileIo     io;
        std::vector<Byte>  player = MakeDisk ("U1.PLAYER", "U1.VARS", MakeVars());
        DecodedFile        decoded;
        const Paragraph  * map    = nullptr;

        io.files["C:\\Disks\\game.dsk"]    = MakeDisk ("U1", "GEN", MakeGen());
        io.stamps["C:\\Disks\\game.dsk"]   = FileStamp { 143360, 0 };
        io.files["C:\\Disks\\player.dsk"]  = player;
        io.stamps["C:\\Disks\\player.dsk"] = FileStamp { 143360, 0 };

        AssertSucceeded (DiskSaveTable::DecodeImage (player, "C:\\Disks\\player.dsk", [&io] (DiskSaveTable::IsFoundFn isCompanion, std::vector<Byte> & outSectors)
                                                     {
                                                         return DiskSaveTable::FindBeside (io, "C:\\Disks\\player.dsk", isCompanion, outSectors);
                                                     }, decoded));

        for (const Paragraph & paragraph : decoded.paragraphs)
        {
            map = paragraph.picture.empty() ? map : &paragraph;
        }

        Assert::IsNotNull (map);
        Assert::IsTrue   (map->pingX >= 0);
        Assert::IsTrue   (GetText (decoded).find (L"The Lands of the Dark Unknown") != std::wstring::npos);
    }


    TEST_METHOD (APlayerDiskAloneHasNoMap)
    {
        FakeDiskFileIo     io;
        std::vector<Byte>  player = MakeDisk ("U1.PLAYER", "U1.VARS", MakeVars());
        std::vector<Byte>  found;

        io.files["C:\\Disks\\player.dsk"]  = player;
        io.stamps["C:\\Disks\\player.dsk"] = FileStamp { 143360, 0 };

        Assert::IsFalse (DiskSaveTable::FindBeside (io, "C:\\Disks\\player.dsk", &DiskSaveTable::IsUltima1GameDisk, found));
    }
};
