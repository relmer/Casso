#include "Pch.h"

#include "FileMapTestImages.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages::MakeDos33
//
//  VTOC at track 17 sector 0, the catalog on sectors $F down to $1 of track
//  17, files from track 18 up and then from track 3 up, each sector marked
//  used in the VTOC's bitmap but OLD's, whose entry was deleted.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> FileMapTestImages::MakeDos33()
{
    vector<Byte>                    image (kImageBytes, 0);
    Byte *                          vtoc  = GetDosSector (image, 17, 0);
    vector<std::pair<int, int>>     free;
    size_t                          next  = 0;
    int                             t     = 0;
    int                             s     = 0;



    auto  markUsed = [&] (int track, int sector)
    {
        vtoc[0x38 + 4 * track + (sector >= 8 ? 0 : 1)] &= static_cast<Byte> (~(1 << (sector % 8)));
    };

    auto  take = [&] ()
    {
        std::pair<int, int>  at = free[next++];

        markUsed (at.first, at.second);
        return at;
    };

    vtoc[0x01] = 17;
    vtoc[0x02] = 15;
    vtoc[0x03] = 3;
    vtoc[0x06] = 254;
    vtoc[0x27] = 122;
    vtoc[0x34] = 35;
    vtoc[0x35] = 16;
    vtoc[0x36] = 0x00;
    vtoc[0x37] = 0x01;

    for (t = 0; t < kTracks; t++)
    {
        vtoc[0x38 + 4 * t]     = 0xFF;
        vtoc[0x38 + 4 * t + 1] = 0xFF;

        for (s = 0; t < 3 && s < kSectors; s++)
        {
            markUsed (t, s);
        }
    }

    for (s = 0; s < kSectors; s++)
    {
        markUsed (17, s);
    }

    for (s = 15; s >= 1; s--)
    {
        GetDosSector (image, 17, s)[0x01] = (s > 1) ? 17 : 0;
        GetDosSector (image, 17, s)[0x02] = static_cast<Byte> (s > 1 ? s - 1 : 0);
    }

    for (t = 18; t < kTracks; t++)
    {
        for (s = 15; s >= 0; s--)
        {
            free.push_back ({ t, s });
        }
    }

    for (t = 3; t < 17; t++)
    {
        for (s = 15; s >= 0; s--)
        {
            free.push_back ({ t, s });
        }
    }

    Byte *  catalog = GetDosSector (image, 17, 15);

    //  HELLO: one list, three data sectors.
    {
        Byte *               entry = catalog + 0x0B;
        std::pair<int, int>  list  = take();
        Byte *               bytes = GetDosSector (image, list.first, list.second);

        entry[0] = static_cast<Byte> (list.first);
        entry[1] = static_cast<Byte> (list.second);
        entry[2] = 0x02;
        SetDosName (entry + 3, "HELLO");
        SetWord (entry + 0x21, 4);

        for (int d = 0; d < 3; d++)
        {
            std::pair<int, int>  data = take();

            bytes[0x0C + 2 * d]     = static_cast<Byte> (data.first);
            bytes[0x0C + 2 * d + 1] = static_cast<Byte> (data.second);
        }
    }

    //  BIG: three lists of 122, 122 and 6 data sectors.
    {
        Byte *               entry    = catalog + 0x0B + 0x23;
        std::pair<int, int>  list     = take();
        Byte *               previous = nullptr;

        entry[0] = static_cast<Byte> (list.first);
        entry[1] = static_cast<Byte> (list.second);
        entry[2] = 0x04;
        SetDosName (entry + 3, "BIG");
        SetWord (entry + 0x21, 253);

        for (int l = 0; l < 3; l++)
        {
            Byte *  bytes = GetDosSector (image, list.first, list.second);

            if (previous != nullptr)
            {
                previous[0x01] = static_cast<Byte> (list.first);
                previous[0x02] = static_cast<Byte> (list.second);
            }

            SetWord (bytes + 0x05, 122 * l);

            for (int d = 0; d < (l < 2 ? 122 : 6); d++)
            {
                std::pair<int, int>  data = take();

                bytes[0x0C + 2 * d]     = static_cast<Byte> (data.first);
                bytes[0x0C + 2 * d + 1] = static_cast<Byte> (data.second);
            }

            previous = bytes;
            list     = (l < 2) ? take() : list;
        }
    }

    //  SPARSE: data, a hole, data.
    {
        Byte *               entry = catalog + 0x0B + 2 * 0x23;
        std::pair<int, int>  list  = take();
        Byte *               bytes = GetDosSector (image, list.first, list.second);
        std::pair<int, int>  d0    = take();
        std::pair<int, int>  d2    = take();

        entry[0] = static_cast<Byte> (list.first);
        entry[1] = static_cast<Byte> (list.second);
        entry[2] = 0x00;
        SetDosName (entry + 3, "SPARSE");
        SetWord (entry + 0x21, 3);

        bytes[0x0C] = static_cast<Byte> (d0.first);
        bytes[0x0D] = static_cast<Byte> (d0.second);
        bytes[0x10] = static_cast<Byte> (d2.first);
        bytes[0x11] = static_cast<Byte> (d2.second);
    }

    //  Slot 3 unused; OLD in slot 4, deleted, its sectors free again.
    {
        Byte *               entry = catalog + 0x0B + 4 * 0x23;
        std::pair<int, int>  list  = free[next++];
        Byte *               bytes = GetDosSector (image, list.first, list.second);
        std::pair<int, int>  data  = free[next++];

        entry[0] = 0xFF;
        entry[1] = static_cast<Byte> (list.second);
        entry[2] = 0x04;
        SetDosName (entry + 3, "OLD");
        entry[0x20] = static_cast<Byte> (list.first);
        SetWord (entry + 0x21, 2);

        bytes[0x0C] = static_cast<Byte> (data.first);
        bytes[0x0D] = static_cast<Byte> (data.second);
    }

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages::MakeProDos
//
//  Boot blocks 0 and 1, the volume directory in blocks 2 to 5, the bitmap in
//  block 6, then the files from block 7. GONE was deleted, so its block is
//  free.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> FileMapTestImages::MakeProDos()
{
    vector<Byte>  image (kImageBytes, 0);
    Byte *        key    = GetBlock (image, 2);
    Byte *        bitmap = GetBlock (image, 6);
    int           b      = 0;



    auto  entry = [&] (Byte * block, int index) { return block + 4 + index * 0x27; };

    auto  setEntry = [&] (Byte * at, int storage, const char * name, int type, int keyBlock, int used, int eof)
    {
        int  length = static_cast<int> (strlen (name));

        at[0] = static_cast<Byte> ((storage << 4) | length);
        memcpy (at + 1, name, length);
        at[0x10] = static_cast<Byte> (type);
        SetWord (at + 0x11, keyBlock);
        SetWord (at + 0x13, used);
        at[0x15] = static_cast<Byte> (eof);
        at[0x16] = static_cast<Byte> (eof >> 8);
        at[0x17] = static_cast<Byte> (eof >> 16);
        at[0x1E] = 0xC3;
    };

    auto  setHeader = [&] (Byte * block, int storage, const char * name, int files)
    {
        int  length = static_cast<int> (strlen (name));

        block[4] = static_cast<Byte> ((storage << 4) | length);
        memcpy (block + 5, name, length);
        block[0x23] = 0x27;
        block[0x24] = 0x0D;
        SetWord (block + 0x25, files);
    };

    setHeader (key, 0xF, "TEST", 5);
    SetWord (key + 0x02, 3);
    SetWord (key + 0x27, 6);
    SetWord (key + 0x29, 280);

    for (b = 3; b <= 5; b++)
    {
        SetWord (GetBlock (image, b),     b - 1);
        SetWord (GetBlock (image, b) + 2, b < 5 ? b + 1 : 0);
    }

    //  SEED in block 7.
    setEntry (entry (key, 1), 0x1, "SEED", 0x06, 7, 1, 100);

    //  SPARSE.TREE: master index 8, index 9 (data 10, a hole, data 11, then
    //  holes), index 12 (data 13); 257 blocks long.
    setEntry (entry (key, 2), 0x3, "SPARSE.TREE", 0x06, 8, 6, 257 * 512);
    GetBlock (image, 8)[0] = 9;
    GetBlock (image, 8)[1] = 12;
    GetBlock (image, 9)[0] = 10;
    GetBlock (image, 9)[2] = 11;
    GetBlock (image, 12)[0] = 13;

    //  FORKED: extended key 14; data fork seedling 15; resource fork sapling
    //  with index 16 over data 17 and 18.
    setEntry (entry (key, 3), 0x5, "FORKED", 0x06, 14, 5, 512);
    GetBlock (image, 14)[0] = 0x1;
    SetWord (GetBlock (image, 14) + 1, 15);
    SetWord (GetBlock (image, 14) + 3, 1);
    GetBlock (image, 14)[5] = 10;
    GetBlock (image, 14)[0x100] = 0x2;
    SetWord (GetBlock (image, 14) + 0x101, 16);
    SetWord (GetBlock (image, 14) + 0x103, 3);
    GetBlock (image, 14)[0x106] = 4;
    GetBlock (image, 16)[0] = 17;
    GetBlock (image, 16)[1] = 18;

    //  EMPTY: an entry that uses no blocks.
    setEntry (entry (key, 4), 0x1, "EMPTY", 0x04, 0, 0, 0);

    //  GONE: deleted, its seedling in block 19 free again.
    setEntry (entry (key, 5), 0x0, "GONE", 0x04, 19, 1, 50);

    //  DIR1 at block 20 holds DIR2 at block 21, which holds DEEP in block 22.
    setEntry (entry (key, 6), 0xD, "DIR1", 0x0F, 20, 1, 512);
    setHeader (GetBlock (image, 20), 0xE, "DIR1", 1);
    setEntry (entry (GetBlock (image, 20), 1), 0xD, "DIR2", 0x0F, 21, 1, 512);
    setHeader (GetBlock (image, 21), 0xE, "DIR2", 1);
    setEntry (entry (GetBlock (image, 21), 1), 0x1, "DEEP", 0x04, 22, 1, 5);

    //  The bitmap: every block free, then the ones in use taken.
    std::fill (bitmap, bitmap + 35, Byte (0xFF));

    for (b = 0; b <= 22; b++)
    {
        if (b != 19)
        {
            bitmap[b / 8] &= static_cast<Byte> (~(0x80 >> (b % 8)));
        }
    }

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages::MakePascal
//
//  The directory in blocks 2 to 5; SYSTEM.PASCAL in blocks 6 to 10, a gap,
//  NOTES.TEXT in 14 to 17 with 100 bytes in its last block, and a .BAD file
//  over blocks 20 and 21.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> FileMapTestImages::MakePascal()
{
    vector<Byte>  image     (kImageBytes, 0);
    Byte *        directory = GetBlock (image, 2);



    auto  setEntry = [&] (int index, int first, int next, int kind, const char * name, int lastBytes)
    {
        Byte *  at     = directory + 26 * index;
        int     length = static_cast<int> (strlen (name));

        SetWord (at, first);
        SetWord (at + 2, next);
        SetWord (at + 4, kind);
        at[6] = static_cast<Byte> (length);
        memcpy (at + 7, name, length);
        SetWord (at + 22, lastBytes);
    };

    SetWord (directory, 0);
    SetWord (directory + 2, 6);
    SetWord (directory + 4, 0);
    directory[6] = 6;
    memcpy (directory + 7, "PASVOL", 6);
    SetWord (directory + 14, 280);
    SetWord (directory + 16, 3);

    setEntry (1, 6, 11, 2, "SYSTEM.PASCAL", 512);
    setEntry (2, 14, 18, 3, "NOTES.TEXT", 100);
    setEntry (3, 20, 22, 1, "BAD.00020.BAD", 512);

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages::MakeCpm
//
//  The directory in CP/M sectors $0 to $7 of track 3, free entries $E5:
//  PIP.COM's two extents, SPARSE.DAT with a hole, NOTES.TXT for user 5, a
//  deleted OLD.TXT, and the system entry of user 31 over blocks 128 to 139.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> FileMapTestImages::MakeCpm()
{
    vector<Byte>  image (kImageBytes, 0);



    for (int s = 0; s < 8; s++)
    {
        std::fill (GetCpmSector (image, 3, s), GetCpmSector (image, 3, s) + kSectorBytes, Byte (0xE5));
    }

    auto  setEntry = [&] (int index, int user, const char * name, int extent, int records, std::initializer_list<int> blocks)
    {
        Byte *  at = GetCpmSector (image, 3, index / 8) + (index % 8) * 32;
        int     i  = 0;

        std::fill (at, at + 32, Byte (0));
        at[0] = static_cast<Byte> (user);
        memcpy (at + 1, name, 11);
        at[12] = static_cast<Byte> (extent);
        at[15] = static_cast<Byte> (records);

        for (int block : blocks)
        {
            at[16 + i++] = static_cast<Byte> (block);
        }
    };

    setEntry (0, 0,    "PIP     COM", 0, 0x80, { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 });
    setEntry (1, 0,    "PIP     COM", 1, 0x10, { 18, 19 });
    setEntry (2, 0,    "SPARSE  DAT", 0, 0x18, { 20, 0, 21 });
    setEntry (3, 5,    "NOTES   TXT", 0, 0x08, { 22 });
    setEntry (4, 0xE5, "OLD     TXT", 0, 0x08, { 23 });
    setEntry (5, 31,   "CPM3    SYS", 0, 0x80, { 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139 });

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages::GetDosSector
//
////////////////////////////////////////////////////////////////////////////////

Byte * FileMapTestImages::GetDosSector (vector<Byte> & image, int track, int logical)
{
    return image.data() + (track * kSectors + logical) * kSectorBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages::GetBlock
//
////////////////////////////////////////////////////////////////////////////////

Byte * FileMapTestImages::GetBlock (vector<Byte> & image, int block)
{
    return image.data() + block * kBlockBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages::GetCpmSector
//
////////////////////////////////////////////////////////////////////////////////

Byte * FileMapTestImages::GetCpmSector (vector<Byte> & dosOrder, int track, int cpmSector)
{
    return GetDosSector (dosOrder, track, NibblizationLayer::GetDosFileIndexForPhysicalSector ((3 * cpmSector) % kSectors));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages::ToDosOrder
//
//  The same sectors with each ProDOS-order file index moved to its DOS-order
//  one; ToProDosOrder moves them back.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> FileMapTestImages::ToDosOrder (const vector<Byte> & proDosOrder)
{
    vector<Byte>  out (proDosOrder.size(), 0);



    for (int t = 0; t < kTracks; t++)
    {
        for (int dos = 0; dos < kSectors; dos++)
        {
            int  po = NibblizationLayer::GetPoFileIndexForDosLogicalSector (dos);

            memcpy (out.data() + (t * kSectors + dos) * kSectorBytes, proDosOrder.data() + (t * kSectors + po) * kSectorBytes, kSectorBytes);
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages::ToProDosOrder
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> FileMapTestImages::ToProDosOrder (const vector<Byte> & dosOrder)
{
    vector<Byte>  out (dosOrder.size(), 0);



    for (int t = 0; t < kTracks; t++)
    {
        for (int dos = 0; dos < kSectors; dos++)
        {
            int  po = NibblizationLayer::GetPoFileIndexForDosLogicalSector (dos);

            memcpy (out.data() + (t * kSectors + po) * kSectorBytes, dosOrder.data() + (t * kSectors + dos) * kSectorBytes, kSectorBytes);
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages::Analyze
//
////////////////////////////////////////////////////////////////////////////////

void FileMapTestImages::Analyze (const vector<Byte> & image, bool isProDosOrder, DiskAnalysis & out)
{
    DiskImage  disk;



    AssertSucceeded (isProDosOrder ? NibblizationLayer::NibblizePo (image, disk) : NibblizationLayer::NibblizeDsk (image, disk));
    DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (disk, 1, "test", 0, false), DecodeSettings::MakeStandard(), out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages::BreakSector
//
////////////////////////////////////////////////////////////////////////////////

void FileMapTestImages::BreakSector (DiskAnalysis & inOut, int track, int physical, bool isMissing)
{
    int            slot = inOut.entries[track * DiskImage::kQuarterTracksPerWholeTrack].slot;
    TrackAnalysis  copy = *inOut.tracks[slot];



    for (size_t s = 0; s < copy.sectors.size(); s++)
    {
        if (copy.sectors[s].sector == physical)
        {
            if (isMissing)
            {
                copy.sectors.erase (copy.sectors.begin() + static_cast<ptrdiff_t> (s));
            }
            else
            {
                copy.sectors[s].isDataGood = false;
                copy.sectors[s].state      = SectorState::BadData;
            }

            break;
        }
    }

    inOut.tracks[slot] = std::make_shared<const TrackAnalysis> (std::move (copy));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages::SetWord
//
////////////////////////////////////////////////////////////////////////////////

void FileMapTestImages::SetWord (Byte * at, int value)
{
    at[0] = static_cast<Byte> (value);
    at[1] = static_cast<Byte> (value >> 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages::SetDosName
//
////////////////////////////////////////////////////////////////////////////////

void FileMapTestImages::SetDosName (Byte * at, const char * name)
{
    size_t  length = strlen (name);



    for (size_t i = 0; i < 30; i++)
    {
        at[i] = static_cast<Byte> ((i < length ? name[i] : ' ') | 0x80);
    }
}
