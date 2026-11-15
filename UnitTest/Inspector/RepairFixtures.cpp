#include "Pch.h"

#include "RepairFixtures.h"
#include "../EmuTests/FileMapTestImages.h"
#include "Devices/Disk/Inspector/FileMap/Dos33MapReader.h"





static constexpr int   s_kTracks        = 35;
static constexpr int   s_kSectors       = 16;
static constexpr int   s_kSectorBytes   = 256;
static constexpr int   s_kBootTracks    = 3;
static constexpr int   s_kVtocTrack     = 17;
static constexpr int   s_kBitmap        = 0x38;
static constexpr int   s_kBitmapBytes   = 4;
static constexpr int   s_kFirstEntry    = 0x0B;
static constexpr int   s_kEntryCount    = 0x21;
static constexpr int   s_kFirstPair     = 0x0C;
static constexpr int   s_kHelloList     = 15;
static constexpr int   s_kHelloData     = 14;
static constexpr int   s_kBigList       = 11;
static constexpr int   s_kFileTrack     = 18;
static constexpr int   s_kOffDisk       = 40;
static constexpr int   s_kBitmapBlock   = 6;
static constexpr int   s_kKeyBlock      = 2;
static constexpr int   s_kFileCount     = 0x25;
static constexpr int   s_kFirstProDosEntry = 0x04 + 0x27;
static constexpr int   s_kBlocksUsed    = 0x13;
static constexpr int   s_kTreeData      = 10;
static constexpr int   s_kPascalCount   = 16;
static constexpr int   s_kBitsPerByte   = 8;





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MakeDos33
//
//  FileMapTestImages' DOS 3.3 volume, with the header DOS's INIT writes, a
//  boot sector that begins with $01, and DOS's image on tracks 0 to 2.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> RepairFixtures::MakeDos33()
{
    vector<Byte>  image = FileMapTestImages::MakeDos33();
    Byte *        vtoc  = GetDosSector (image, s_kVtocTrack, 0);



    Dos33MapReader::MakeStandardHeader (Dos33MapReader::kStandardVolume, vtoc);

    for (int t = 0; t < s_kBootTracks; t++)
    {
        for (int s = 0; s < s_kSectors; s++)
        {
            Byte *  sector = GetDosSector (image, t, s);

            for (int i = 0; i < s_kSectorBytes; i++)
            {
                sector[i] = static_cast<Byte> (i * 3 + t * s_kSectors + s);
            }
        }
    }

    GetDosSector (image, 0, 0)[0] = 0x01;

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MakeProDos / MakePascal
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> RepairFixtures::MakeProDos()
{
    return FileMapTestImages::MakeProDos();
}


vector<Byte> RepairFixtures::MakePascal()
{
    return FileMapTestImages::MakePascal();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MakeDos33Bitmap
//
//  HELLO's first data sector marked free, and a sector no file uses on
//  track 10 marked used.
//
////////////////////////////////////////////////////////////////////////////////

RepairFixture RepairFixtures::MakeDos33Bitmap()
{
    RepairFixture  fixture { L"DOS 3.3 bitmap", MakeDos33(), {}, false, true };



    fixture.damaged = fixture.before;
    MarkDos33 (fixture.damaged, s_kFileTrack, s_kHelloData, false);
    MarkDos33 (fixture.damaged, kUnusedTrack, kUnusedSector, true);

    return fixture;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MakeDos33CatalogLoop
//
//  The catalog's last sector, $1, links back to its second, $E.
//
////////////////////////////////////////////////////////////////////////////////

RepairFixture RepairFixtures::MakeDos33CatalogLoop()
{
    RepairFixture  fixture { L"DOS 3.3 catalog loop", MakeDos33(), {}, false, true };



    fixture.damaged = fixture.before;
    GetDosSector (fixture.damaged, s_kVtocTrack, 1)[0x01] = s_kVtocTrack;
    GetDosSector (fixture.damaged, s_kVtocTrack, 1)[0x02] = 0x0E;

    return fixture;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MakeDos33HeaderZeroed
//
//  Bytes $00 to $37 of the VTOC zeroed; the bitmap after them intact.
//
////////////////////////////////////////////////////////////////////////////////

RepairFixture RepairFixtures::MakeDos33HeaderZeroed()
{
    RepairFixture  fixture { L"DOS 3.3 header zeroed", MakeDos33(), {}, false, true };
    Byte *         vtoc    = nullptr;



    fixture.damaged = fixture.before;
    vtoc            = GetDosSector (fixture.damaged, s_kVtocTrack, 0);
    std::fill (vtoc, vtoc + Dos33MapReader::kHeaderBytes, Byte (0));

    return fixture;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MakeDos33NextTrackOff
//
////////////////////////////////////////////////////////////////////////////////

RepairFixture RepairFixtures::MakeDos33NextTrackOff()
{
    RepairFixture  fixture { L"DOS 3.3 next track off the disk", MakeDos33(), {}, false, true };



    fixture.damaged = fixture.before;
    GetDosSector (fixture.damaged, s_kVtocTrack, 0)[Dos33MapReader::kNextTrack] = s_kOffDisk;

    return fixture;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MakeDos33CountDiffers
//
//  HELLO's catalog count is 9; it uses 4.
//
////////////////////////////////////////////////////////////////////////////////

RepairFixture RepairFixtures::MakeDos33CountDiffers()
{
    RepairFixture  fixture { L"DOS 3.3 count", MakeDos33(), {}, false, true };



    fixture.damaged = fixture.before;
    GetDosSector (fixture.damaged, s_kVtocTrack, Dos33MapReader::kCatalogSector)[s_kFirstEntry + s_kEntryCount] = 9;

    return fixture;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MakeDos33CrossLink
//
//  BIG's first data sector is HELLO's first.
//
////////////////////////////////////////////////////////////////////////////////

RepairFixture RepairFixtures::MakeDos33CrossLink()
{
    RepairFixture  fixture { L"DOS 3.3 cross-link", MakeDos33(), {}, false, false };



    fixture.damaged = fixture.before;
    GetDosSector (fixture.damaged, s_kFileTrack, s_kBigList)[s_kFirstPair]     = s_kFileTrack;
    GetDosSector (fixture.damaged, s_kFileTrack, s_kBigList)[s_kFirstPair + 1] = s_kHelloData;

    return fixture;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MakeDos33ChainOffDisk
//
//  HELLO's first pair is on track 40.
//
////////////////////////////////////////////////////////////////////////////////

RepairFixture RepairFixtures::MakeDos33ChainOffDisk()
{
    RepairFixture  fixture { L"DOS 3.3 chain off the disk", MakeDos33(), {}, false, false };



    fixture.damaged = fixture.before;
    GetDosSector (fixture.damaged, s_kFileTrack, s_kHelloList)[s_kFirstPair] = s_kOffDisk;

    return fixture;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MakeProDosBitmapAndCount
//
//  A block of SPARSE.TREE marked free, block 100 marked used, and a
//  volume directory file count of 7 for its 5 entries.
//
////////////////////////////////////////////////////////////////////////////////

RepairFixture RepairFixtures::MakeProDosBitmapAndCount()
{
    RepairFixture  fixture { L"ProDOS bitmap and count", MakeProDos(), {}, true, true };
    Byte *         bitmap  = nullptr;



    fixture.damaged = fixture.before;
    bitmap          = FileMapTestImages::GetBlock (fixture.damaged, s_kBitmapBlock);

    bitmap[s_kTreeData / s_kBitsPerByte]   |= static_cast<Byte> (0x80 >> (s_kTreeData % s_kBitsPerByte));
    bitmap[kUnusedBlock / s_kBitsPerByte]  &= static_cast<Byte> (~(0x80 >> (kUnusedBlock % s_kBitsPerByte)));
    FileMapTestImages::GetBlock (fixture.damaged, s_kKeyBlock)[s_kFileCount] = 7;

    return fixture;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MakeProDosBlocksUsed
//
//  SEED's blocks-used count is 3; it uses 1.
//
////////////////////////////////////////////////////////////////////////////////

RepairFixture RepairFixtures::MakeProDosBlocksUsed()
{
    RepairFixture  fixture { L"ProDOS blocks used", MakeProDos(), {}, true, true };



    fixture.damaged = fixture.before;
    FileMapTestImages::GetBlock (fixture.damaged, s_kKeyBlock)[s_kFirstProDosEntry + s_kBlocksUsed] = 3;

    return fixture;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MakePascalCount
//
//  A directory file count of 4 for its 3 entries.
//
////////////////////////////////////////////////////////////////////////////////

RepairFixture RepairFixtures::MakePascalCount()
{
    RepairFixture  fixture { L"Apple Pascal count", MakePascal(), {}, true, true };



    fixture.damaged = fixture.before;
    FileMapTestImages::GetBlock (fixture.damaged, s_kKeyBlock)[s_kPascalCount] = 4;

    return fixture;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MakeAll
//
////////////////////////////////////////////////////////////////////////////////

vector<RepairFixture> RepairFixtures::MakeAll()
{
    return { MakeDos33Bitmap(), MakeDos33CatalogLoop(), MakeDos33HeaderZeroed(), MakeDos33NextTrackOff(), MakeDos33CountDiffers(),
             MakeProDosBitmapAndCount(), MakeProDosBlocksUsed(), MakePascalCount(), MakeDos33CrossLink(), MakeDos33ChainOffDisk() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::GetDosSector
//
////////////////////////////////////////////////////////////////////////////////

Byte * RepairFixtures::GetDosSector (vector<Byte> & image, int track, int sector)
{
    return image.data() + (track * s_kSectors + sector) * s_kSectorBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixtures::MarkDos33
//
//  A sector's bit in the VTOC's bitmap: set for free, clear for used.
//
////////////////////////////////////////////////////////////////////////////////

void RepairFixtures::MarkDos33 (vector<Byte> & image, int track, int sector, bool isUsed)
{
    Byte &  bits = GetDosSector (image, s_kVtocTrack, 0)[s_kBitmap + track * s_kBitmapBytes + (sector >= s_kBitsPerByte ? 0 : 1)];
    Byte    mask = static_cast<Byte> (1 << (sector % s_kBitsPerByte));



    bits = isUsed ? static_cast<Byte> (bits & ~mask) : static_cast<Byte> (bits | mask);
}
