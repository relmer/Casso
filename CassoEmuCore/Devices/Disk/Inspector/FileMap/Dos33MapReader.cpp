#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/Dos33MapReader.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"





static constexpr int   s_kCatalogTrack     = 0x01;
static constexpr int   s_kCatalogSector    = 0x02;
static constexpr int   s_kRelease          = 0x03;
static constexpr int   s_kVolume           = 0x06;
static constexpr int   s_kPairsPerList     = 0x27;
static constexpr int   s_kTracksPerDisk    = 0x34;
static constexpr int   s_kSectorsPerTrack  = 0x35;
static constexpr int   s_kBytesPerSector   = 0x36;
static constexpr int   s_kBitmap           = 0x38;
static constexpr int   s_kBitmapBytes      = 4;
static constexpr int   s_kBitmapWordBytes  = 2;
static constexpr int   s_kFirstEntry       = 0x0B;
static constexpr int   s_kEntrySize        = 0x23;
static constexpr int   s_kEntries          = 7;
static constexpr int   s_kEntryName        = 0x03;
static constexpr int   s_kNameLength       = 30;
static constexpr int   s_kEntryType        = 0x02;
static constexpr int   s_kEntryCount       = 0x21;
static constexpr int   s_kListOffset       = 0x05;
static constexpr int   s_kFirstPair        = 0x0C;
static constexpr int   s_kPairs            = 122;
static constexpr Byte  s_kDeleted          = 0xFF;
static constexpr Byte  s_kLocked           = 0x80;
static constexpr Byte  s_kHighBit          = 0x80;
static constexpr int   s_kBootTracks       = 3;
static constexpr int   s_kRelease33        = 3;
static constexpr int   s_kSectorBytes      = 256;
static constexpr Byte  s_kUp               = 0x01;
static constexpr Byte  s_kDown             = 0xFF;
static constexpr Byte  s_kBootSectorCount  = 0x01;
static constexpr Byte  s_kHeaderFirstByte  = 0x04;
static constexpr int   s_kOddPercent       = 15;
static constexpr int   s_kPercent          = 100;
static constexpr int   s_kCountTolerance   = 1;
static constexpr int   s_kCountModulus     = 256;
static constexpr int   s_kIntegerType      = 0x01;
static constexpr int   s_kApplesoftType    = 0x02;
static constexpr int   s_kBinaryType       = 0x04;
static constexpr int   s_kBasicHeader      = 2;
static constexpr int   s_kBinaryHeader     = 4;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::IsFound
//
//  A track count and a sector count, 256 bytes a sector or the 35 tracks of
//  16 sectors a 140 KB disk has, and a catalog track and sector inside the
//  volume of that size. A size that differs is a finding of the header check,
//  not a reason to miss the volume.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33MapReader::IsFound (const SectorSource & source, int & outTracks, int & outSectors)
{
    const SectorSource::Sector &  vtoc         = source.GetDos33 (kVtocTrack, 0);
    bool                          isFound      = SectorSource::IsReadable (vtoc.result) && vtoc.bytes != nullptr;
    bool                          isSectorSize = false;
    bool                          isDiskSize   = false;



    if (isFound)
    {
        outTracks    = vtoc.bytes[s_kTracksPerDisk];
        outSectors   = vtoc.bytes[s_kSectorsPerTrack];
        isSectorSize = vtoc.bytes[s_kBytesPerSector] == 0x00 && vtoc.bytes[s_kBytesPerSector + 1] == 0x01;
        isDiskSize   = outTracks == SectorSource::kTracks && outSectors == SectorSource::kSectors;
        isFound      = (isSectorSize || isDiskSize) && outTracks > 0 && outSectors > 0 &&
                       vtoc.bytes[s_kCatalogTrack] < outTracks && vtoc.bytes[s_kCatalogSector] < outSectors;
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::IsHeaderLost
//
//  A boot sector that begins as DOS's does, a VTOC that can be read for its
//  bitmap, and a catalog that DOS's INIT linked: on such a disk a header
//  that isn't DOS's is read, and repaired, as DOS's.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33MapReader::IsHeaderLost (const SectorSource & source)
{
    const SectorSource::Sector &  vtoc    = source.GetDos33 (kVtocTrack, 0);
    const SectorSource::Sector &  boot    = source.GetDos33 (0, 0);
    bool                          isLost  = SectorSource::IsReadable (vtoc.result) && vtoc.bytes != nullptr;
    vector<int>                   sectors;
    int                           linked  = 0;



    isLost = isLost && SectorSource::IsReadable (boot.result) && boot.bytes != nullptr && boot.bytes[0] == s_kBootSectorCount &&
             std::any_of (boot.bytes, boot.bytes + s_kSectorBytes, [&] (Byte b) { return b != boot.bytes[0]; });

    if (isLost)
    {
        GetInitCatalog (source, sectors, linked);
        isLost = linked >= kInitLinkedMin;
    }

    return isLost;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::MakeStandardHeader
//
//  As the System Master's VTOC has it: $04, catalog at track 17 sector $F,
//  release 3, the volume, 122 pairs a list, 35 tracks of 16 sectors of 256
//  bytes; every other byte of the header zero.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33MapReader::MakeStandardHeader (Byte volume, Byte * outVtoc)
{
    std::fill (outVtoc, outVtoc + kHeaderBytes, Byte (0));

    outVtoc[0]                     = s_kHeaderFirstByte;
    outVtoc[s_kCatalogTrack]       = static_cast<Byte> (kVtocTrack);
    outVtoc[s_kCatalogSector]      = static_cast<Byte> (kCatalogSector);
    outVtoc[s_kRelease]            = static_cast<Byte> (s_kRelease33);
    outVtoc[s_kVolume]             = volume;
    outVtoc[s_kPairsPerList]       = static_cast<Byte> (s_kPairs);
    outVtoc[kNextTrack]            = static_cast<Byte> (kVtocTrack);
    outVtoc[kDirection]            = s_kUp;
    outVtoc[s_kTracksPerDisk]      = static_cast<Byte> (SectorSource::kTracks);
    outVtoc[s_kSectorsPerTrack]    = static_cast<Byte> (SectorSource::kSectors);
    outVtoc[s_kBytesPerSector]     = 0x00;
    outVtoc[s_kBytesPerSector + 1] = 0x01;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::GetInitCatalog
//
////////////////////////////////////////////////////////////////////////////////

void Dos33MapReader::GetInitCatalog (const SectorSource & source, vector<int> & outSectors, int & outLinked)
{
    int   s        = 0;
    bool  isLinked = false;



    outSectors.clear();
    outLinked = 0;

    for (s = kCatalogSector; s >= 1; s--)
    {
        const SectorSource::Sector &  sector = source.GetDos33 (kVtocTrack, s);

        if (!SectorSource::IsReadable (sector.result) || sector.bytes == nullptr || !IsBelievableSector (sector.bytes))
        {
            break;
        }

        isLinked   = (s > 1) ? (sector.bytes[s_kCatalogTrack] == kVtocTrack && sector.bytes[s_kCatalogSector] == s - 1)
                             : (sector.bytes[s_kCatalogTrack] == 0 && sector.bytes[s_kCatalogSector] == 0);
        outLinked += isLinked ? 1 : 0;
        outSectors.push_back (s);
    }
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
    vector<Entry>  entries;



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
    CheckHeader (writer, vtoc);

    //  The catalog chain; where it breaks, the catalog is incomplete and the
    //  file list has the sector it stops at (FR-092).
    while (track != 0 || sector != 0)
    {
        cell = (track < SectorSource::kTracks && sector < SectorSource::kSectors) ? GetCell (track, sector) : -1;

        if (!writer.Step (cell, visited, reason))
        {
            outMap.isCatalogComplete = false;
            outMap.unreadableCell    = cell;
            writer.AddFinding (FindingKind::FileChainBroken, cell, L"The catalog chain " + writer.DescribeChain (reason, cell));
            break;
        }

        const Byte *  bytes = source.GetDos33 (track, sector).bytes;

        isCatalog[cell] = true;
        writer.SetStructure (cell, SectorRole::CatalogOrDirectory);
        outMap.catalogCells.push_back (cell);

        for (int e = 0; e < s_kEntries; e++)
        {
            entries.push_back ({ cell, s_kFirstEntry + e * s_kEntrySize, bytes + s_kFirstEntry + e * s_kEntrySize });
        }

        track  = bytes[s_kCatalogTrack];
        sector = bytes[s_kCatalogSector];
    }

    outMap.isCatalogStandard = CheckEntries (writer, entries);

    //  The VTOC and the catalog's sectors marked used, so DOS doesn't
    //  allocate them to a file.
    for (int at : outMap.catalogCells)
    {
        if (!markedUsed[at])
        {
            writer.AddFinding (FindingKind::Dos33StructureMarkedFree, at, FileMapWriter::Capitalize (writer.DescribeCell (at)) + L" is a catalog sector but marked free");
        }
    }

    if (!markedUsed[GetCell (kVtocTrack, 0)])
    {
        writer.AddFinding (FindingKind::Dos33StructureMarkedFree, GetCell (kVtocTrack, 0), L"The VTOC is marked free");
    }

    for (size_t e = 0; outMap.isCatalogStandard && e < entries.size(); e++)
    {
        ReadEntry (writer, source, entries[e]);
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
//  Dos33MapReader::CheckHeader
//
//  The VTOC's header against DOS's: the sector size and the pairs a
//  list make the volume Suspect when they differ, as does a next track off
//  the disk or a direction other than up or down; the release, the volume
//  number and the bytes after each track's bitmap word are notes.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33MapReader::CheckHeader (FileMapWriter & writer, const Byte * vtoc)
{
    int  at       = GetCell (kVtocTrack, 0);
    int  size     = vtoc[s_kBytesPerSector] | (vtoc[s_kBytesPerSector + 1] << 8);
    int  padded   = 0;
    int  t        = 0;



    if (size != s_kSectorBytes)
    {
        writer.AddFinding (FindingKind::Dos33SectorBytesDiffer, at, std::format (L"Expected a VTOC sector size of 256 bytes, but found {}", size));
    }

    if (vtoc[s_kPairsPerList] != s_kPairs)
    {
        writer.AddFinding (FindingKind::Dos33PairsDiffer, at, std::format (L"Expected 122 track/sector pairs per list in the VTOC, but found {}", vtoc[s_kPairsPerList]));
    }

    if (vtoc[kNextTrack] >= SectorSource::kTracks)
    {
        writer.AddFinding (FindingKind::Dos33NextTrackOffDisk, at, std::format (L"The VTOC's next track to allocate, track {}, is off the disk", vtoc[kNextTrack]));
    }
    else if (vtoc[kDirection] != s_kUp && vtoc[kDirection] != s_kDown)
    {
        writer.AddFinding (FindingKind::Dos33NextTrackOffDisk, at, std::format (L"The VTOC's allocation direction is ${:02X} instead of $01 or $FF", vtoc[kDirection]));
    }

    if (vtoc[s_kRelease] != s_kRelease33)
    {
        writer.AddFinding (FindingKind::Dos33ReleaseDiffers, at, std::format (L"Expected DOS release 3 in the VTOC, but found {}", vtoc[s_kRelease]));
    }

    if (vtoc[s_kVolume] == 0 || vtoc[s_kVolume] == 0xFF)
    {
        writer.AddFinding (FindingKind::Dos33VolumeOutOfRange, at, std::format (L"The VTOC's volume number {} is outside 1 to 254", vtoc[s_kVolume]));
    }

    for (t = 0; t < SectorSource::kTracks; t++)
    {
        const Byte *  word = vtoc + s_kBitmap + t * s_kBitmapBytes;

        padded += (word[s_kBitmapWordBytes] != 0 || word[s_kBitmapWordBytes + 1] != 0) ? 1 : 0;
    }

    if (padded > 0)
    {
        writer.AddFinding (FindingKind::Dos33BitmapPadding, at, std::format (L"The VTOC has nonzero bytes after the bitmap of {} tracks", padded));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::CheckEntries
//
//  Each entry in use: a name with the high bit set on some characters and
//  clear on others, a type DOS 3.3 doesn't have, or a count larger than the
//  disk. When at least two entries and more than 15% of them fail,
//  the catalog isn't standard and its files aren't followed; otherwise each
//  failure is a separate finding.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33MapReader::CheckEntries (FileMapWriter & writer, const vector<Entry> & entries)
{
    int   inUse      = 0;
    int   failed     = 0;
    int   count      = 0;
    bool  isMixed    = false;
    bool  isUnknown  = false;
    bool  isLarge    = false;
    bool  isStandard = true;



    for (const Entry & entry : entries)
    {
        Byte  track = entry.bytes[0];

        if (track == 0 || track == s_kDeleted)
        {
            continue;
        }

        count   = entry.bytes[s_kEntryCount] | (entry.bytes[s_kEntryCount + 1] << 8);
        inUse  += 1;
        failed += (IsNameMixed (entry.bytes + s_kEntryName) || !IsKnownType (entry.bytes[s_kEntryType]) || count > writer.GetCellCount()) ? 1 : 0;
    }

    isStandard = inUse < 2 || failed * s_kPercent <= s_kOddPercent * inUse;

    if (!isStandard)
    {
        writer.AddFinding (FindingKind::Dos33CatalogNonstandard, entries.front().cell,
                           std::format (L"The catalog isn't standard: {} of {} entries have an invalid file name, type or sector count. Files in this catalog aren't read.",
                                        failed, inUse));
    }

    for (const Entry & entry : entries)
    {
        Byte          track = entry.bytes[0];
        std::wstring  name  = ReadName (entry.bytes + s_kEntryName, s_kNameLength);

        if (!isStandard || track == 0 || track == s_kDeleted)
        {
            continue;
        }

        count     = entry.bytes[s_kEntryCount] | (entry.bytes[s_kEntryCount + 1] << 8);
        isMixed   = IsNameMixed (entry.bytes + s_kEntryName);
        isUnknown = !IsKnownType (entry.bytes[s_kEntryType]);
        isLarge   = count > writer.GetCellCount();

        if (isMixed)
        {
            writer.AddFinding (FindingKind::Dos33EntryNameMixed, entry.cell, std::format (L"{}: {} characters of the name have the high bit clear",
                                                                                     name, CountClearBits (entry.bytes + s_kEntryName)));
        }

        if (isUnknown)
        {
            writer.AddFinding (FindingKind::Dos33EntryTypeUnknown, entry.cell, std::format (L"{}: type ${:02X} isn't a DOS 3.3 type", name, entry.bytes[s_kEntryType]));
        }

        if (isLarge)
        {
            writer.AddFinding (FindingKind::Dos33EntryCountTooLarge, entry.cell, std::format (L"{}: catalog sector count {} is larger than the disk", name, count));
        }
    }

    return isStandard;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::ReadEntry
//
//  An entry with track 0 is unused; $FF is deleted, its first list's track
//  kept in the name's last byte.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33MapReader::ReadEntry (FileMapWriter & writer, const SectorSource & source, const Entry & entry)
{
    MappedFile    file;
    int           index = -1;
    const Byte *  bytes = entry.bytes;
    Byte          track = bytes[0];



    if (track != 0)
    {
        file.isDeleted     = track == s_kDeleted;
        track              = file.isDeleted ? bytes[s_kEntryName + s_kNameLength - 1] : track;
        file.path          = ReadName (bytes + s_kEntryName, file.isDeleted ? s_kNameLength - 1 : s_kNameLength);
        file.type          = GetTypeLetter (bytes[s_kEntryType]);
        file.isLocked      = (bytes[s_kEntryType] & s_kLocked) != 0;
        file.recordedCount = bytes[s_kEntryCount] | (bytes[s_kEntryCount + 1] << 8);
        index              = writer.AddFile (std::move (file));

        WalkLists   (writer, source, index, track, bytes[1]);
        CheckCount  (writer, index, entry);
        CheckLength (writer, source, writer.GetFile (index), bytes[s_kEntryType]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::WalkLists
//
//  List by list: each track/sector list, then the data sectors it gives. A
//  pair of zeros is a hole when a later pair gives a sector; trailing zeros
//  only end the file. Each list's first sector in the file, at $05, is
//  compared with its place in the chain, as a note.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33MapReader::WalkLists (FileMapWriter & writer, const SectorSource & source, int file, int track, int sector)
{
    std::set<int>  visited;
    ChainReason    reason   = ChainReason::None;
    int            cell     = 0;
    int            holes    = 0;
    int            pair     = 0;
    int            list     = 0;
    int            offset   = 0;
    bool           isBroken = false;
    bool           isNoted  = false;



    while (!isBroken && (track != 0 || sector != 0))
    {
        cell = (track < SectorSource::kTracks && sector < SectorSource::kSectors) ? GetCell (track, sector) : -1;

        if (!writer.Step (cell, visited, reason))
        {
            writer.BreakChain (file, reason, cell);
            break;
        }

        const Byte *  bytes = source.GetDos33 (track, sector).bytes;

        writer.Own (file, cell, SectorRole::IndexBlock);

        offset = bytes[s_kListOffset] | (bytes[s_kListOffset + 1] << 8);

        if (!isNoted && offset != list * s_kPairs)
        {
            isNoted = true;
            writer.AddFinding (FindingKind::Dos33ListOffsetDiffers, cell, std::format (L"{}: track/sector list {} starts at sector {} of the file instead of {}",
                                                                                       writer.GetFile (file).path, list + 1, offset, list * s_kPairs));
        }

        for (pair = 0; !isBroken && pair < s_kPairs; pair++)
        {
            int  dataTrack  = bytes[s_kFirstPair + 2 * pair];
            int  dataSector = bytes[s_kFirstPair + 2 * pair + 1];

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

        list++;
        track  = bytes[s_kCatalogTrack];
        sector = bytes[s_kCatalogSector];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::CheckCount
//
//  The count of sectors in the chain against the catalog's, one either way
//  allowed; a count of zero isn't compared, and a file over 255 sectors is
//  compared modulo 256, since DOS's count wraps. A count that differs is
//  damage, which Repair sets right.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33MapReader::CheckCount (FileMapWriter & writer, int file, const Entry & entry)
{
    const MappedFile &  mapped   = writer.GetFile (file);
    int                 used     = mapped.GetUsedCount();
    int                 recorded = mapped.recordedCount;
    int                 apart    = std::abs (used - recorded);
    int                 wrapped  = 0;



    if (used >= s_kCountModulus)
    {
        wrapped = std::abs (used % s_kCountModulus - recorded % s_kCountModulus);
        apart   = std::min (wrapped, s_kCountModulus - wrapped);
    }

    if (!mapped.isDeleted && mapped.chainReason == ChainReason::None && recorded > 0 && apart > s_kCountTolerance)
    {
        writer.AddFinding (FindingKind::FileCountDiffers, entry.cell, std::format (L"{}: expected a catalog sector count of {}, but found {}", mapped.path, used, recorded));
        writer.AddCount ({ entry.cell, entry.offset + s_kEntryCount, recorded, used, std::format (L"the catalog sector count of {}", mapped.path) });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::CheckLength
//
//  A BASIC program's length in its first two bytes, or a binary file's in
//  bytes 2 and 3 after its load address, against the sectors the file has,
//  as a note: data files are often saved under a program's type.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33MapReader::CheckLength (FileMapWriter & writer, const SectorSource & source, const MappedFile & file, Byte type)
{
    int           kind    = type & ~s_kLocked;
    int           header  = (kind == s_kIntegerType || kind == s_kApplesoftType) ? s_kBasicHeader : (kind == s_kBinaryType) ? s_kBinaryHeader : 0;
    int           places  = 0;
    int           first   = -1;
    int           length  = 0;
    int           sectors = 0;
    const Byte *  bytes   = nullptr;



    for (const FilePlace & place : file.sectors)
    {
        if (place.role == SectorRole::FileData)
        {
            first   = (places == 0) ? place.cell : first;
            places += 1;
        }
    }

    if (!file.isDeleted && file.chainReason == ChainReason::None && header != 0 && first >= 0 &&
        SectorSource::IsReadable (source.GetDos33 (first / SectorSource::kSectors, first % SectorSource::kSectors).result))
    {
        bytes = source.GetDos33 (first / SectorSource::kSectors, first % SectorSource::kSectors).bytes;
    }

    if (bytes != nullptr)
    {
        length = (header == s_kBasicHeader) ? (bytes[0] | (bytes[1] << 8)) : (bytes[2] | (bytes[3] << 8));
        sectors = (length + header + s_kSectorBytes - 1) / s_kSectorBytes;

        if (sectors != places)
        {
            writer.AddFinding (FindingKind::Dos33StoredLengthDiffers, first, std::format (L"{}: the stored length of {} bytes fills {} sectors, but the file has {}",
                                                                                          file.path, length, sectors, places));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::IsBelievableSector
//
//  A catalog sector whose link is inside the disk and whose every entry is
//  unused, deleted, or believable.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33MapReader::IsBelievableSector (const Byte * sector)
{
    bool  isBelievable = sector[s_kCatalogTrack] < SectorSource::kTracks && sector[s_kCatalogSector] < SectorSource::kSectors;



    for (int e = 0; isBelievable && e < s_kEntries; e++)
    {
        isBelievable = IsBelievableEntry (sector + s_kFirstEntry + e * s_kEntrySize);
    }

    return isBelievable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::IsBelievableEntry
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33MapReader::IsBelievableEntry (const Byte * entry)
{
    Byte  track = entry[0];



    return track == 0 || track == s_kDeleted ||
           (track < SectorSource::kTracks && entry[1] < SectorSource::kSectors && IsKnownType (entry[s_kEntryType]) &&
            !IsNameMixed (entry + s_kEntryName) && (entry[s_kEntryName] & s_kHighBit) != 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::IsKnownType
//
//  Text, Integer BASIC, Applesoft, binary, S, relocatable, A and B, each
//  with the lock bit set or clear.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33MapReader::IsKnownType (Byte type)
{
    int  kind = type & ~s_kLocked;



    return kind == 0x00 || kind == 0x01 || kind == 0x02 || kind == 0x04 || kind == 0x08 || kind == 0x10 || kind == 0x20 || kind == 0x40;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::IsNameMixed
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33MapReader::IsNameMixed (const Byte * name)
{
    int  set = 0;



    for (int i = 0; i < s_kNameLength; i++)
    {
        set += (name[i] & s_kHighBit) != 0 ? 1 : 0;
    }

    return set > 0 && set < s_kNameLength;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader::CountClearBits
//
//  The characters of a name with the high bit clear.
//
////////////////////////////////////////////////////////////////////////////////

int Dos33MapReader::CountClearBits (const Byte * name)
{
    int  clear = 0;



    for (int i = 0; i < s_kNameLength; i++)
    {
        clear += (name[i] & s_kHighBit) == 0 ? 1 : 0;
    }

    return clear;
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
