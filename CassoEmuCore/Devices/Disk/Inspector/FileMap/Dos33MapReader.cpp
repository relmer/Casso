#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/Dos33MapReader.h"





static constexpr int   s_kCatalogTrack     = 0x01;
static constexpr int   s_kCatalogSector    = 0x02;
static constexpr int   s_kVolume           = 0x06;
static constexpr int   s_kTracksPerDisk    = 0x34;
static constexpr int   s_kSectorsPerTrack  = 0x35;
static constexpr int   s_kBytesPerSector   = 0x36;
static constexpr int   s_kBitmap           = 0x38;
static constexpr int   s_kBitmapBytes      = 4;
static constexpr int   s_kFirstEntry       = 0x0B;
static constexpr int   s_kEntrySize        = 0x23;
static constexpr int   s_kEntries          = 7;
static constexpr int   s_kEntryName        = 0x03;
static constexpr int   s_kNameLength       = 30;
static constexpr int   s_kEntryType        = 0x02;
static constexpr int   s_kEntryCount       = 0x21;
static constexpr int   s_kFirstPair        = 0x0C;
static constexpr int   s_kPairs            = 122;
static constexpr Byte  s_kDeleted          = 0xFF;
static constexpr Byte  s_kLocked           = 0x80;
static constexpr int   s_kBootTracks       = 3;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::IsFound
//
//  256 bytes per sector, a track count and a sector count, and a catalog
//  track and sector inside the volume those counts give (FR-084).
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33MapReader::IsFound (const SectorSource & source, int & outTracks, int & outSectors)
{
    const SectorSource::Sector &  vtoc    = source.GetDos33 (kVtocTrack, 0);
    bool                          isFound = SectorSource::IsReadable (vtoc.result) && vtoc.bytes != nullptr;



    if (isFound)
    {
        outTracks  = vtoc.bytes[s_kTracksPerDisk];
        outSectors = vtoc.bytes[s_kSectorsPerTrack];
        isFound    = vtoc.bytes[s_kBytesPerSector] == 0x00 && vtoc.bytes[s_kBytesPerSector + 1] == 0x01 && outTracks > 0 && outSectors > 0 &&
                     vtoc.bytes[s_kCatalogTrack] < outTracks && vtoc.bytes[s_kCatalogSector] < outSectors;
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::Read
//
////////////////////////////////////////////////////////////////////////////////

void Dos33MapReader::Read (const SectorSource & source, FileMap & outMap)
{
    FileMapWriter  writer      (source, outMap, MapFileSystem::Dos33);
    const Byte *   vtoc        = source.GetDos33 (kVtocTrack, 0).bytes;
    int            catTrack    = vtoc[s_kCatalogTrack];
    int            track       = vtoc[s_kCatalogTrack];
    int            sector      = vtoc[s_kCatalogSector];
    int            cell        = 0;
    int            t           = 0;
    int            s           = 0;
    ChainReason    reason      = ChainReason::None;
    vector<bool>   markedUsed  (static_cast<size_t> (writer.GetCellCount()), false);
    vector<bool>   isCatalog   (static_cast<size_t> (writer.GetCellCount()), false);
    std::set<int>  visited;



    outMap.volumeName = std::format (L"DOS 3.3 volume {}", vtoc[s_kVolume]);

    //  The VTOC's bitmap: four bytes a track, sectors $F to $8 then $7 to $0,
    //  a set bit free.
    for (t = 0; t < SectorSource::kTracks; t++)
    {
        for (s = 0; s < SectorSource::kSectors; s++)
        {
            Byte  bits = vtoc[s_kBitmap + t * s_kBitmapBytes + (s >= 8 ? 0 : 1)];

            markedUsed[GetCell (t, s)] = ((bits >> (s % 8)) & 1) == 0;
        }
    }

    writer.SetStructure (GetCell (kVtocTrack, 0), SectorRole::VtocOrKeyBlock);

    //  The catalog chain; where it breaks, the catalog is incomplete and the
    //  file list says which sector stopped it (FR-092).
    while (track != 0 || sector != 0)
    {
        cell = (track < SectorSource::kTracks && sector < SectorSource::kSectors) ? GetCell (track, sector) : -1;

        if (!writer.Step (cell, visited, reason))
        {
            outMap.isCatalogComplete = false;
            outMap.unreadableCell    = cell;
            writer.AddFinding (FindingKind::FileChainBroken, cell, L"The catalog stops at " + writer.DescribeCell (cell) + L", " + FileMapWriter::DescribeReason (reason));
            break;
        }

        const Byte *  bytes = source.GetDos33 (track, sector).bytes;

        isCatalog[cell] = true;
        writer.SetStructure (cell, SectorRole::CatalogOrDirectory);

        for (int e = 0; e < s_kEntries; e++)
        {
            ReadEntry (writer, source, bytes + s_kFirstEntry + e * s_kEntrySize);
        }

        track  = bytes[s_kCatalogTrack];
        sector = bytes[s_kCatalogSector];
    }

    //  Boot and DOS image where the VTOC marks tracks 0 to 2 used, track 0
    //  sector 0 always; catalog track sectors marked used that no catalog
    //  sector links to (FR-086).
    for (t = 0; t < s_kBootTracks; t++)
    {
        for (s = 0; s < SectorSource::kSectors; s++)
        {
            writer.SetStructure (GetCell (t, s), (markedUsed[GetCell (t, s)] || (t == 0 && s == 0)) ? SectorRole::BootArea : SectorRole::Free);
        }
    }

    for (s = 1; catTrack < SectorSource::kTracks && s < SectorSource::kSectors; s++)
    {
        cell = GetCell (catTrack, s);

        if (markedUsed[cell] && !isCatalog[cell] && !writer.IsOwned (cell))
        {
            writer.SetStructure (cell, SectorRole::UnusedCatalogSector);
        }
    }

    writer.SetMarkedUsed (std::move (markedUsed));
    writer.Finalize();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::ReadEntry
//
//  An entry with track 0 is unused; $FF is deleted, its first list's track
//  kept in the name's last byte.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33MapReader::ReadEntry (FileMapWriter & writer, const SectorSource & source, const Byte * entry)
{
    MappedFile  file;
    int         index = -1;
    Byte        track = entry[0];



    if (track != 0)
    {
        file.isDeleted     = track == s_kDeleted;
        track              = file.isDeleted ? entry[s_kEntryName + s_kNameLength - 1] : track;
        file.path          = ReadName (entry + s_kEntryName, file.isDeleted ? s_kNameLength - 1 : s_kNameLength);
        file.type          = GetTypeLetter (entry[s_kEntryType]);
        file.isLocked      = (entry[s_kEntryType] & s_kLocked) != 0;
        file.recordedCount = entry[s_kEntryCount] | (entry[s_kEntryCount + 1] << 8);
        index              = writer.AddFile (std::move (file));

        WalkLists (writer, source, index, track, entry[1]);
        writer.CheckCount (index, L"the catalog gives", L"sectors");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::WalkLists
//
//  List by list: each track/sector list, then the data sectors it gives. A
//  pair of zeros is a hole when a later pair gives a sector; trailing zeros
//  only end the file.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33MapReader::WalkLists (FileMapWriter & writer, const SectorSource & source, int file, int track, int sector)
{
    std::set<int>  visited;
    ChainReason    reason   = ChainReason::None;
    int            cell     = 0;
    int            holes    = 0;
    int            pair     = 0;
    bool           isBroken = false;



    while (!isBroken && (track != 0 || sector != 0))
    {
        cell = (track < SectorSource::kTracks && sector < SectorSource::kSectors) ? GetCell (track, sector) : -1;

        if (!writer.Step (cell, visited, reason))
        {
            writer.BreakChain (file, reason, cell);
            break;
        }

        const Byte *  list = source.GetDos33 (track, sector).bytes;

        writer.Own (file, cell, SectorRole::IndexBlock);

        for (pair = 0; !isBroken && pair < s_kPairs; pair++)
        {
            int  dataTrack  = list[s_kFirstPair + 2 * pair];
            int  dataSector = list[s_kFirstPair + 2 * pair + 1];

            if (dataTrack == 0 && dataSector == 0)
            {
                holes++;
                continue;
            }

            if (dataTrack >= SectorSource::kTracks || dataSector >= SectorSource::kSectors)
            {
                writer.BreakChain (file, ChainReason::OutsideVolume, -1);
                isBroken = true;
                continue;
            }

            for (; holes > 0; holes--)
            {
                writer.AddHole (file);
            }

            writer.Own (file, GetCell (dataTrack, dataSector), SectorRole::FileData);
        }

        track  = list[s_kCatalogTrack];
        sector = list[s_kCatalogSector];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::ReadName
//
//  High-bit ASCII, padded with spaces.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Dos33MapReader::ReadName (const Byte * name, int length)
{
    std::wstring  text;



    for (int i = 0; i < length; i++)
    {
        Byte  ch = static_cast<Byte> (name[i] & 0x7F);

        text.push_back ((ch >= 0x20 && ch < 0x7F) ? static_cast<wchar_t> (ch) : L'?');
    }

    while (!text.empty() && text.back() == L' ')
    {
        text.pop_back();
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::GetTypeLetter
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Dos33MapReader::GetTypeLetter (Byte type)
{
    std::wstring  letter = L"?";



    switch (type & ~s_kLocked)
    {
        case 0x00: letter = L"T"; break;
        case 0x01: letter = L"I"; break;
        case 0x02: letter = L"A"; break;
        case 0x04: letter = L"B"; break;
        case 0x08: letter = L"S"; break;
        case 0x10: letter = L"R"; break;
        case 0x20: letter = L"A"; break;
        case 0x40: letter = L"B"; break;
        default:                  break;
    }

    return letter;
}
