#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Devices/Disk/Inspector/FileMap/CpmMapReader.h"
#include "Devices/Disk/Inspector/FileMap/Dos33MapReader.h"
#include "Devices/Disk/Inspector/FileMap/PascalMapReader.h"
#include "Devices/Disk/Inspector/FileMap/ProDosMapReader.h"
#include "Devices/Disk/Inspector/FileMap/SectorSource.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"





static constexpr int  s_kStandardTracks  = 35;
static constexpr int  s_kStandardSectors = 16;
static constexpr int  s_kStandardBlocks  = 280;





////////////////////////////////////////////////////////////////////////////////
//
//  NotMapped
//
//  A map that has only why the disk is not mapped, and each sector's
//  result by track and physical sector (FR-094).
//
////////////////////////////////////////////////////////////////////////////////

static FileMap NotMapped (const SectorSource & source, NotMappedReason reason, int size, const std::wstring & sizeText)
{
    FileMap  map;
    int      t   = 0;
    int      s   = 0;



    map.notMapped = reason;
    map.otherSize = size;
    map.cells.assign (static_cast<size_t> (SectorSource::kTracks * SectorSource::kSectors), MapCell());

    for (t = 0; t < SectorSource::kTracks; t++)
    {
        for (s = 0; s < SectorSource::kSectors; s++)
        {
            MapCell &  cell = map.cells[t * SectorSource::kSectors + s];

            cell.result = source.GetPhysical (t, s).result;
            cell.halves = { cell.result, cell.result };
        }
    }

    map.likelyCause = sizeText;

    return map;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LikelyCause
//
//  When no volume is found, the structure sector a reader needed that could
//  not be read: track 17 sector $0, a half of block 2, or a CP/M directory
//  sector of track 3 (FR-094).
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring LikelyCause (const SectorSource & source)
{
    std::wstring  cause;



    if (!SectorSource::IsReadable (source.GetDos33 (Dos33MapReader::kVtocTrack, 0).result))
    {
        cause = L"Track 17, sector $0, where a DOS 3.3 volume keeps its VTOC, could not be read";
    }
    else if (!SectorSource::IsReadable (source.GetHalf (ProDosMapReader::kKeyBlock, 0).result) || !SectorSource::IsReadable (source.GetHalf (ProDosMapReader::kKeyBlock, 1).result))
    {
        cause = L"Block 2, where a ProDOS or Pascal volume keeps its directory, could not be read";
    }

    for (int s = 0; cause.empty() && s < CpmMapReader::kDirectorySectors; s++)
    {
        if (!SectorSource::IsReadable (source.GetCpm (CpmMapReader::kDirectoryTrack, s).result))
        {
            cause = std::format (L"Track 3, CP/M sector {}, where a CP/M volume keeps its directory, could not be read", InspectorFormat::FormatSector (s));
        }
    }

    return cause;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapBuilder::Build
//
//  DOS 3.3, ProDOS and Pascal are each looked for, and every one found is
//  mapped; CP/M only when none of them is (FR-085).
//
////////////////////////////////////////////////////////////////////////////////

vector<FileMap> FileMapBuilder::Build (const DiskAnalysis & analysis)
{
    SectorSource  source (analysis);



    return Build (source, analysis.summary.commonVolume);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapBuilder::Build
//
//  A DOS 3.3 disk whose VTOC header is lost is read as DOS's INIT would have
//  written it, when no other volume is found.
//
////////////////////////////////////////////////////////////////////////////////

vector<FileMap> FileMapBuilder::Build (const SectorSource & source, int volume)
{
    vector<FileMap>  maps;
    int              tracks  = 0;
    int              sectors = 0;
    int              blocks  = 0;
    bool             isFound = false;



    if (source.IsThirteenSector())
    {
        maps.push_back (NotMapped (source, NotMappedReason::ThirteenSector, 0, L""));
    }
    else
    {
        if (Dos33MapReader::IsFound (source, tracks, sectors))
        {
            isFound = true;
            maps.emplace_back();

            if (tracks == s_kStandardTracks && sectors == s_kStandardSectors)
            {
                Dos33MapReader::Read (source, maps.back());
            }
            else
            {
                maps.back() = NotMapped (source, NotMappedReason::OtherSize, tracks * sectors, std::format (L"A DOS 3.3 volume of {} tracks of {} sectors", tracks, sectors));
            }
        }

        for (bool isProDos : { true, false })
        {
            if (isProDos ? ProDosMapReader::IsFound (source, blocks) : PascalMapReader::IsFound (source, blocks))
            {
                isFound = true;
                maps.emplace_back();

                if (blocks != s_kStandardBlocks)
                {
                    maps.back() = NotMapped (source, NotMappedReason::OtherSize, blocks, std::format (L"{} volume of {} blocks", isProDos ? L"A ProDOS" : L"An Apple Pascal", blocks));
                }
                else if (isProDos)
                {
                    ProDosMapReader::Read (source, maps.back());
                }
                else
                {
                    PascalMapReader::Read (source, maps.back());
                }
            }
        }

        if (!isFound && Dos33MapReader::IsHeaderLost (source))
        {
            isFound = true;
            maps.emplace_back();
            ReadLostHeader (source, volume, maps.back());
        }

        if (!isFound && CpmMapReader::IsFound (source))
        {
            isFound = true;
            maps.emplace_back();
            CpmMapReader::Read (source, maps.back());
        }

        if (!isFound)
        {
            maps.push_back (NotMapped (source, NotMappedReason::NoFileSystem, 0, LikelyCause (source)));
        }
    }

    for (FileMap & map : maps)
    {
        map.verdict = GetVerdict (map);
    }

    return maps;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapBuilder::ReadLostHeader
//
//  The volume read through a VTOC with DOS's header and the disk's
//  bitmap, and the lost header reported as damage.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapBuilder::ReadLostHeader (const SectorSource & source, int volume, FileMap & outMap)
{
    SectorSource                              patched = source;
    std::array<Byte, SectorSource::kBytes>    vtoc    = {};
    const Byte                              * bytes   = source.GetDos33 (Dos33MapReader::kVtocTrack, 0).bytes;
    Byte                                      number  = (volume >= 1 && volume <= 0xFE) ? static_cast<Byte> (volume) : Dos33MapReader::kStandardVolume;
    Finding                                   finding;
    bool                                      isZero  = false;



    isZero = std::all_of (bytes, bytes + Dos33MapReader::kHeaderBytes, [] (Byte b) { return b == 0; });
    std::copy (bytes, bytes + SectorSource::kBytes, vtoc.begin());
    Dos33MapReader::MakeStandardHeader (number, vtoc.data());
    patched.SetDos33 (Dos33MapReader::kVtocTrack, 0, vtoc.data());

    Dos33MapReader::Read (patched, outMap);

    finding.category     = FindingCategory::FileSystem;
    finding.kind         = FindingKind::Dos33HeaderLost;
    finding.weight       = FindingWeight::Damage;
    finding.quarterTrack = Dos33MapReader::kVtocTrack * DiskImage::kQuarterTracksPerWholeTrack;
    finding.sector       = SectorSource::GetDos33Physical (0);
    finding.mapCell      = Dos33MapReader::GetCell (Dos33MapReader::kVtocTrack, 0);
    finding.detail       = std::format ("The VTOC header is {}. The catalog is read from track 17, sector $F.", isZero ? "zeroed" : "not DOS 3.3's");

    outMap.isHeaderLost = true;
    outMap.findings.insert (outMap.findings.begin(), finding);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapBuilder::GetVerdict
//
//  Bad for any finding of damage, Suspect for any finding worth a look, OK
//  otherwise; None for a disk that isn't mapped.
//
////////////////////////////////////////////////////////////////////////////////

FileSystemVerdict FileMapBuilder::GetVerdict (const FileMap & map)
{
    FileSystemVerdict  verdict = FileSystemVerdict::Ok;



    for (const Finding & finding : map.findings)
    {
        if (finding.weight == FindingWeight::Damage)
        {
            verdict = FileSystemVerdict::Bad;
        }
        else if (finding.weight == FindingWeight::Suspect && verdict == FileSystemVerdict::Ok)
        {
            verdict = FileSystemVerdict::Suspect;
        }
    }

    return (map.notMapped != NotMappedReason::None || map.fileSystem == MapFileSystem::None) ? FileSystemVerdict::None : verdict;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapBuilder::GetVerdict
//
////////////////////////////////////////////////////////////////////////////////

FileSystemVerdict FileMapBuilder::GetVerdict (const vector<FileMap> & maps)
{
    FileSystemVerdict  verdict = FileSystemVerdict::None;



    for (const FileMap & map : maps)
    {
        if (map.verdict != FileSystemVerdict::None && (verdict == FileSystemVerdict::None || map.verdict > verdict))
        {
            verdict = map.verdict;
        }
    }

    return verdict;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMap::GetPhysical
//
//  A map that is not mapped has its grid by physical sector already.
//
////////////////////////////////////////////////////////////////////////////////

int FileMap::GetPhysical (int cell, int half) const
{
    int  physical = GetColumn (cell);



    switch (fileSystem)
    {
        case MapFileSystem::Dos33:  physical = SectorSource::GetDos33Physical (GetColumn (cell)); break;
        case MapFileSystem::ProDos:
        case MapFileSystem::Pascal: physical = SectorSource::GetBlockPhysical (cell, half);      break;
        case MapFileSystem::Cpm:    physical = SectorSource::GetCpmPhysical (GetColumn (cell));   break;
        default:                                                                                 break;
    }

    return physical;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMap::GetCellOf
//
////////////////////////////////////////////////////////////////////////////////

int FileMap::GetCellOf (int track, int physical, int & outHalf) const
{
    int  found = -1;



    outHalf = 0;

    for (int column = 0; found < 0 && track >= 0 && track < tracks && column < cellsPerTrack; column++)
    {
        int  cell = track * cellsPerTrack + column;

        for (int half = 0; found < 0 && half < 2; half++)
        {
            if (GetPhysical (cell, half) == physical)
            {
                found   = cell;
                outHalf = half;
            }
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMap::GetPlaceInFile
//
////////////////////////////////////////////////////////////////////////////////

int FileMap::GetPlaceInFile (int file, int cell, SectorRole & outRole) const
{
    int  place = 0;
    int  count = 0;



    outRole = SectorRole::FileData;

    for (const FilePlace & at : files[file].sectors)
    {
        count += (at.cell >= 0 && at.role == SectorRole::FileData) ? 1 : 0;

        if (place == 0 && at.cell == cell)
        {
            place   = (at.role == SectorRole::FileData) ? count : 1;
            outRole = at.role;
        }
    }

    return place;
}
