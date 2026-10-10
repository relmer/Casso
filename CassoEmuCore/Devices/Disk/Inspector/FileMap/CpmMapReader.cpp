#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/CpmMapReader.h"





static constexpr int   s_kUserMax       = 31;
static constexpr int   s_kExtentMax     = 31;
static constexpr int   s_kRecordsMax    = 0x80;
static constexpr int   s_kRecordsPerBlk = 8;
static constexpr int   s_kRecordBytes   = 128;
static constexpr int   s_kName          = 1;
static constexpr int   s_kNameLength    = 8;
static constexpr int   s_kTypeLength    = 3;
static constexpr int   s_kExtent        = 12;
static constexpr int   s_kS2            = 14;
static constexpr int   s_kRecordCount   = 15;
static constexpr int   s_kBlocks        = 16;
static constexpr int   s_kReadOnly      = 9;
static constexpr int   s_kSystemTracks  = 3;
static constexpr int   s_kFoundEntries  = 4;
static constexpr Byte  s_kHighBit       = 0x80;





////////////////////////////////////////////////////////////////////////////////
//
//  CpmMapReader::GetBlockCell
//
//  Block b is CP/M sectors 4(b mod 4) to 4(b mod 4) + 3 of track
//  (3 + b div 4) mod 35 (FR-085).
//
////////////////////////////////////////////////////////////////////////////////

int CpmMapReader::GetBlockCell (int block, int sector)
{
    int  track = (kDirectoryTrack + block / kSectorsPerBlock) % SectorSource::kTracks;



    return track * SectorSource::kSectors + kSectorsPerBlock * (block % kSectorsPerBlock) + sector;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmMapReader::IsFound
//
////////////////////////////////////////////////////////////////////////////////

bool CpmMapReader::IsFound (const SectorSource & source)
{
    vector<Byte>  directory;
    int           badSector = -1;
    int           valid     = 0;
    int           invalid   = 0;
    int           free      = 0;
    bool          isInUse   = false;



    if (ReadDirectory (source, directory, badSector))
    {
        for (int e = 0; e < kEntries; e++)
        {
            const Byte *  entry = &directory[e * kEntrySize];

            if (entry[0] == kFree)
            {
                free++;
            }
            else if (IsValid (entry, isInUse))
            {
                valid += isInUse ? 1 : 0;
            }
            else
            {
                invalid++;
            }
        }
    }

    return (valid >= 1 && invalid == 0) || valid > s_kFoundEntries || free == kEntries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmMapReader::Read
//
////////////////////////////////////////////////////////////////////////////////

void CpmMapReader::Read (const SectorSource & source, FileMap & outMap)
{
    FileMapWriter                     writer    (source, outMap, MapFileSystem::Cpm);
    vector<Byte>                      directory;
    vector<Entry>                     entries;
    vector<Entry>                     deleted;
    std::map<std::wstring, int>       fileOf;
    std::set<std::wstring>            seen;
    int                               badSector = -1;
    int                               cell      = 0;
    bool                              isInUse   = false;



    outMap.volumeName = L"CP/M volume";

    for (cell = 0; cell < s_kSystemTracks * SectorSource::kSectors; cell++)
    {
        writer.SetStructure (cell, SectorRole::BootArea);
    }

    for (int b = 0; b < 2; b++)
    {
        for (int s = 0; s < kSectorsPerBlock; s++)
        {
            writer.SetStructure (GetBlockCell (b, s), SectorRole::CatalogOrDirectory);
        }
    }

    if (!ReadDirectory (source, directory, badSector))
    {
        outMap.isCatalogComplete = false;
        outMap.unreadableCell    = kDirectoryTrack * SectorSource::kSectors + badSector;
        writer.AddFinding (FindingKind::FileChainBroken, outMap.unreadableCell, L"The directory cannot be read at " + writer.DescribeCell (outMap.unreadableCell));
    }

    //  The entries: invalid ones reported, the system entry of user 31 left
    //  to the system area, deleted ones kept apart.
    for (int e = 0; e < kEntries; e++)
    {
        const Byte *  bytes = &directory[e * kEntrySize];
        Entry         entry = ReadEntry (bytes, e);

        if (bytes[0] == kFree)
        {
            if (!entry.name.empty() && std::any_of (entry.blocks.begin(), entry.blocks.end(), [] (Byte b) { return b != 0; }))
            {
                deleted.push_back (entry);
            }
        }
        else if (!IsValid (bytes, isInUse))
        {
            writer.AddFinding (FindingKind::CpmInvalidEntry, kDirectoryTrack * SectorSource::kSectors + e / 8,
                               std::format (L"Directory entry {} is not a valid CP/M entry", e));
        }
        else if (entry.user == s_kUserMax && std::any_of (entry.blocks.begin(), entry.blocks.end(), [] (Byte b) { return b >= kVolumeBlocks; }))
        {
            continue;
        }
        else if (isInUse)
        {
            std::wstring  key = std::format (L"{}:{}#{}", entry.user, entry.name, entry.extent);

            if (!seen.insert (key).second)
            {
                writer.AddFinding (FindingKind::CpmDuplicateEntry, -1, std::format (L"{}:{} has two entries for extent {}", entry.user, entry.name, entry.extent));
            }

            entries.push_back (entry);
        }
    }

    std::stable_sort (entries.begin(), entries.end(), [] (const Entry & a, const Entry & b) { return a.extent < b.extent; });

    for (bool isDeleted : { false, true })
    {
        for (const Entry & entry : isDeleted ? deleted : entries)
        {
            std::wstring  path   = std::format (L"{}:{}", isDeleted ? 0 : entry.user, entry.name);
            int           blocks = (entry.records + s_kRecordsPerBlk - 1) / s_kRecordsPerBlk;
            auto          found  = fileOf.find (path + (isDeleted ? L"*" : L""));
            int           index  = -1;

            if (found == fileOf.end())
            {
                MappedFile  file;

                file.path      = path;
                file.type      = entry.name.substr (entry.name.find (L'.') == std::wstring::npos ? entry.name.size() : entry.name.find (L'.') + 1);
                file.isLocked  = entry.isLocked;
                file.isDeleted = isDeleted;
                index          = writer.AddFile (std::move (file));
                fileOf[path + (isDeleted ? L"*" : L"")] = index;
            }
            else
            {
                index = found->second;
            }

            outMap.files[index].recordedSize += static_cast<uint64_t> (entry.records) * s_kRecordBytes;

            for (int i = 0; i < std::min (blocks, s_kBlocks); i++)
            {
                if (entry.blocks[i] == 0 || entry.blocks[i] >= kVolumeBlocks)
                {
                    writer.AddHole (index);
                    continue;
                }

                for (int s = 0; s < kSectorsPerBlock; s++)
                {
                    writer.Own (index, GetBlockCell (entry.blocks[i], s), SectorRole::FileData);
                }
            }
        }
    }

    writer.Finalize();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmMapReader::ReadDirectory
//
//  CP/M sectors $0 to $7 of track 3 end to end; a sector that cannot be
//  read is left as free entries and given back.
//
////////////////////////////////////////////////////////////////////////////////

bool CpmMapReader::ReadDirectory (const SectorSource & source, vector<Byte> & outBytes, int & outBadSector)
{
    bool  isRead = true;



    outBytes.assign (static_cast<size_t> (kDirectorySectors) * SectorSource::kBytes, kFree);

    for (int s = 0; s < kDirectorySectors; s++)
    {
        const SectorSource::Sector &  sector = source.GetCpm (kDirectoryTrack, s);

        if (SectorSource::IsReadable (sector.result) && sector.bytes != nullptr)
        {
            std::copy (sector.bytes, sector.bytes + SectorSource::kBytes, outBytes.begin() + s * SectorSource::kBytes);
        }
        else if (isRead)
        {
            isRead       = false;
            outBadSector = s;
        }
    }

    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmMapReader::IsValid
//
//  A user number of 0 to 31, an extent and record count CP/M 2.2 allows, no
//  control character in the name, and blocks inside the volume, except on
//  the system entry of user 31.
//
////////////////////////////////////////////////////////////////////////////////

bool CpmMapReader::IsValid (const Byte * entry, bool & outIsInUse)
{
    int   user    = entry[0];
    bool  isValid = user <= s_kUserMax && entry[s_kExtent] <= s_kExtentMax && entry[s_kS2] == 0 && entry[s_kRecordCount] <= s_kRecordsMax;
    int   i       = 0;



    for (i = 0; isValid && i < s_kNameLength + s_kTypeLength; i++)
    {
        isValid = (entry[s_kName + i] & ~s_kHighBit) >= 0x20;
    }

    for (i = 0; isValid && i < s_kBlocks; i++)
    {
        isValid = entry[s_kBlocks + i] < kVolumeBlocks || (user == s_kUserMax && entry[s_kBlocks + i] < kSystemBlocks);
    }

    outIsInUse = isValid;

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmMapReader::ReadEntry
//
//  The name as NAME.TYP with the attribute bits masked off; the read-only
//  attribute is the type's first high bit.
//
////////////////////////////////////////////////////////////////////////////////

CpmMapReader::Entry CpmMapReader::ReadEntry (const Byte * bytes, int index)
{
    Entry         entry;
    std::wstring  name;
    std::wstring  type;
    int           i     = 0;



    for (i = 0; i < s_kNameLength + s_kTypeLength; i++)
    {
        Byte  ch = static_cast<Byte> (bytes[s_kName + i] & ~s_kHighBit);

        (i < s_kNameLength ? name : type).push_back ((ch > 0x20 && ch < 0x7F) ? static_cast<wchar_t> (ch) : L'\0');
    }

    std::erase (name, L'\0');
    std::erase (type, L'\0');

    entry.index    = index;
    entry.user     = bytes[0];
    entry.name     = type.empty() ? name : name + L"." + type;
    entry.extent   = bytes[s_kExtent] + 32 * bytes[s_kS2];
    entry.records  = bytes[s_kRecordCount];
    entry.isLocked = (bytes[s_kReadOnly] & s_kHighBit) != 0;
    std::copy (bytes + s_kBlocks, bytes + s_kBlocks + 16, entry.blocks.begin());

    return entry;
}
