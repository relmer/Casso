#include "Pch.h"

#include "Ui/DiskInspector/FileMapText.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/FileMap/SectorSource.h"
#include "Ui/DiskInspector/InspectorText.h"





static constexpr LPCWSTR  s_kpszLineEnd = L"\r\n";





////////////////////////////////////////////////////////////////////////////////
//
//  ToLowerFirst
//
//  A role's name in the middle of a sentence.
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring ToLowerFirst (std::wstring text)
{
    if (!text.empty() && text[0] >= L'A' && text[0] <= L'Z')
    {
        text[0] = static_cast<wchar_t> (text[0] - L'A' + L'a');
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::GetFileColumns
//
////////////////////////////////////////////////////////////////////////////////

vector<std::wstring> FileMapText::GetFileColumns (const FileMap & map)
{
    bool  isBlocks = map.fileSystem == MapFileSystem::ProDos || map.fileSystem == MapFileSystem::Pascal;



    return { L"Path", L"Type", L"Size", isBlocks ? L"Blocks" : L"Sectors", L"State" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::BuildFileRows
//
//  Each file, deleted ones only when shown, or only those touching bad
//  sectors; sorted by a column, numbers by value. A row's finding holds the
//  file's index.
//
////////////////////////////////////////////////////////////////////////////////

vector<TableRow> FileMapText::BuildFileRows (const FileMap & map, bool isShowingDeleted, bool isBadOnly, int sortColumn, bool isDescending)
{
    vector<TableRow>  rows;
    int               index = 0;



    for (const MappedFile & file : map.files)
    {
        if ((isShowingDeleted || !file.isDeleted) && (!isBadOnly || (file.states & MappedFile::kTouchesBadSectors) != 0))
        {
            TableRow  row;

            row.cells   = { file.path, file.type, FormatSize (map, file), std::to_wstring (file.GetUsedCount()), FormatStates (file) };
            row.finding = index;
            rows.push_back (std::move (row));
        }

        index++;
    }

    if (sortColumn >= 0 && sortColumn < static_cast<int> (GetFileColumns (map).size()))
    {
        std::stable_sort (rows.begin(), rows.end(), [&] (const TableRow & a, const TableRow & b)
        {
            const std::wstring &  x = isDescending ? b.cells[sortColumn] : a.cells[sortColumn];
            const std::wstring &  y = isDescending ? a.cells[sortColumn] : b.cells[sortColumn];

            return InspectorTables::IsLess (x, y);
        });
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::FormatRole
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapText::FormatRole (MapFileSystem fileSystem, SectorRole role)
{
    bool          isDos = fileSystem == MapFileSystem::Dos33;
    bool          isCpm = fileSystem == MapFileSystem::Cpm;
    std::wstring  text;



    switch (role)
    {
        case SectorRole::Free:                text = L"Free";                                                                break;
        case SectorRole::BootArea:            text = isDos ? L"Boot and DOS image" : isCpm ? L"System area" : L"Boot blocks"; break;
        case SectorRole::VtocOrKeyBlock:      text = isDos ? L"VTOC" : L"Volume directory key block";                         break;
        case SectorRole::CatalogOrDirectory:  text = isDos ? L"Catalog sector" : fileSystem == MapFileSystem::ProDos ? L"Volume directory block"
                                                                                                                      : L"Directory block";  break;
        case SectorRole::UnusedCatalogSector: text = L"Unused catalog track sector";                                         break;
        case SectorRole::VolumeBitmap:        text = L"Volume bitmap";                                                       break;
        case SectorRole::Subdirectory:        text = L"Subdirectory block";                                                  break;
        case SectorRole::IndexBlock:          text = isDos ? L"Track/sector list" : L"Index block";                          break;
        case SectorRole::FileData:            text = L"File data";                                                           break;
        case SectorRole::BadBlocksFile:       text = L"Bad blocks the file system recorded";                                 break;
        case SectorRole::AllocatedUnowned:    text = L"Allocated but unowned";                                               break;
        case SectorRole::OwnedMarkedFree:     text = L"Owned but marked free";                                               break;
        case SectorRole::CrossLinked:         text = L"Cross-linked";                                                        break;
        default:                                                                                                             break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::FormatResult
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapText::FormatResult (SectorResult result)
{
    std::wstring  text;



    switch (result)
    {
        case SectorResult::Good:       text = L"Good";        break;
        case SectorResult::NotChecked: text = L"Not checked"; break;
        case SectorResult::Bad:        text = L"Bad";         break;
        default:                       text = L"Missing";     break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::FormatCell
//
//  A cell and the physical sector that holds it (FR-089): a DOS 3.3 logical
//  sector, a block and the sectors of its halves, or a CP/M sector with its
//  allocation block.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapText::FormatCell (const FileMap & map, int cell)
{
    int           track  = map.GetTrack (cell);
    int           column = map.GetColumn (cell);
    std::wstring  text;



    switch (map.fileSystem)
    {
        case MapFileSystem::Dos33:
            text = std::format (L"Track {}, DOS 3.3 sector {} (physical {})", track, InspectorFormat::FormatSector (column), InspectorFormat::FormatSector (map.GetPhysical (cell)));
            break;

        case MapFileSystem::ProDos:
        case MapFileSystem::Pascal:
            text = std::format (L"Block {} (track {}, physical {} and {})", cell, track, InspectorFormat::FormatSector (map.GetPhysical (cell, 0)),
                                InspectorFormat::FormatSector (map.GetPhysical (cell, 1)));
            break;

        case MapFileSystem::Cpm:
            text = std::format (L"Track {}, CP/M sector {}, block {} (physical {})", track, InspectorFormat::FormatSector (column),
                                ((track + SectorSource::kTracks - 3) % SectorSource::kTracks) * 4 + column / 4, InspectorFormat::FormatSector (map.GetPhysical (cell)));
            break;

        default:
            text = std::format (L"Track {}, sector {}", track, InspectorFormat::FormatSector (column));
            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::FormatOwners
//
//  Each file that owns a cell with the cell's place in it, as "HELLO, data
//  sector 3 of 5" or "HELLO, track/sector list".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapText::FormatOwners (const FileMap & map, int cell)
{
    std::wstring  text;
    SectorRole    role  = SectorRole::FileData;
    int           place = 0;
    int           total = 0;



    for (int owner : map.cells[cell].owners)
    {
        const MappedFile &  file = map.files[owner];

        place = map.GetPlaceInFile (owner, cell, role);
        total = static_cast<int> (std::count_if (file.sectors.begin(), file.sectors.end(), [] (const FilePlace & p) { return p.cell >= 0 && p.role == SectorRole::FileData; }));
        text += (text.empty() ? L"" : L"; ") + file.path + L", ";
        text += (role == SectorRole::FileData) ? std::format (L"data sector {} of {}", place, total) : ToLowerFirst (FormatRole (map.fileSystem, role));
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::FormatTooltip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapText::FormatTooltip (const FileMap & map, int cell)
{
    const MapCell &  at   = map.cells[cell];
    std::wstring     text = FormatCell (map, cell) + L"\n" + FormatRole (map.fileSystem, at.role) + L", " + FormatResult (at.result);



    if ((map.fileSystem == MapFileSystem::ProDos || map.fileSystem == MapFileSystem::Pascal) && at.halves[0] != at.halves[1])
    {
        text += std::format (L" (first half {}, second half {})", FormatResult (at.halves[0]), FormatResult (at.halves[1]));
    }

    if (!at.owners.empty())
    {
        text += L"\n" + FormatOwners (map, cell);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::FormatStates
//
//  Every state that applies, in order; "Complete" for none, "Deleted" alone
//  for a deleted file (FR-089).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapText::FormatStates (const MappedFile & file)
{
    std::wstring  text;



    if (file.isDeleted)
    {
        text = L"Deleted";
    }
    else
    {
        text += (file.states & MappedFile::kChainBroken)       ? L"Chain broken" : L"";
        text += (file.states & MappedFile::kCrossLinked)       ? (text.empty() ? L"" : L", ") + std::wstring (L"Cross-linked") : L"";
        text += (file.states & MappedFile::kTouchesBadSectors) ? (text.empty() ? L"" : L", ") + std::wstring (L"Touches bad sectors") : L"";
        text  = text.empty() ? L"Complete" : text;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::FormatSize
//
//  As the catalog or directory records it: DOS 3.3 in sectors, the others
//  in bytes.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapText::FormatSize (const FileMap & map, const MappedFile & file)
{
    return (map.fileSystem == MapFileSystem::Dos33) ? InspectorText::FormatCount (file.recordedCount, L"sector", L"sectors")
                                                    : InspectorFormat::FormatCount (file.recordedSize) + L" bytes";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::FormatNotMapped
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapText::FormatNotMapped (const FileMap & map)
{
    std::wstring  text;



    switch (map.notMapped)
    {
        case NotMappedReason::ThirteenSector: text = L"This is a 13-sector disk, which the map does not read."; break;
        case NotMappedReason::OtherSize:      text = map.likelyCause + L" is not mapped; the map reads 35 tracks of 16 sectors."; break;

        default:
            text = L"No DOS 3.3, ProDOS, Apple Pascal or CP/M volume was found.";
            text += map.likelyCause.empty() ? L"" : L" " + map.likelyCause + L", which may be why.";
            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::FormatVolume
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapText::FormatVolume (const FileMap & map)
{
    std::wstring  system;



    switch (map.fileSystem)
    {
        case MapFileSystem::Dos33:  system = L"DOS 3.3";    break;
        case MapFileSystem::ProDos: system = L"ProDOS";     break;
        case MapFileSystem::Pascal: system = L"Apple Pascal"; break;
        case MapFileSystem::Cpm:    system = L"CP/M";       break;
        default:                    system = L"Not mapped"; break;
    }

    return (map.volumeName.empty() || map.fileSystem == MapFileSystem::Dos33 || map.fileSystem == MapFileSystem::Cpm) ? (map.volumeName.empty() ? system : map.volumeName)
                                                                                                                       : system + L" " + map.volumeName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::FormatMap
//
//  "Copy map" (FR-095): each track's number and a letter per cell, then
//  the key to the letters used.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapText::FormatMap (const FileMap & map)
{
    std::wstring           text;
    std::set<SectorRole>   used;
    int                    t     = 0;
    int                    c     = 0;



    for (t = 0; t < map.tracks; t++)
    {
        text += std::format (L"{:2}  ", t);

        for (c = 0; c < map.cellsPerTrack; c++)
        {
            SectorRole  role = map.cells[t * map.cellsPerTrack + c].role;

            text.push_back (GetRoleLetter (role));
            used.insert (role);
        }

        text += s_kpszLineEnd;
    }

    text += s_kpszLineEnd;

    for (SectorRole role : used)
    {
        text += std::format (L"{}  {}", GetRoleLetter (role), FormatRole (map.fileSystem, role)) + s_kpszLineEnd;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::GetRoleLetter
//
////////////////////////////////////////////////////////////////////////////////

wchar_t FileMapText::GetRoleLetter (SectorRole role)
{
    static constexpr wchar_t  kLetters[] = { L'.', L'B', L'V', L'C', L'U', L'M', L'S', L'L', L'D', L'X', L'A', L'F', L'!' };



    return static_cast<size_t> (role) < std::size (kLetters) ? kLetters[static_cast<size_t> (role)] : L'?';
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText::BuildSectorItems
//
////////////////////////////////////////////////////////////////////////////////

vector<HeaderItem> FileMapText::BuildSectorItems (const FileMap & map, int track, int physical)
{
    vector<HeaderItem>  items;
    int                 half = 0;
    int                 cell = (map.notMapped == NotMappedReason::None) ? map.GetCellOf (track, physical, half) : -1;



    if (cell >= 0)
    {
        if (map.fileSystem == MapFileSystem::Cpm)
        {
            items.push_back ({ L"CP/M sector", InspectorFormat::FormatSector (map.GetColumn (cell)) });
            items.push_back ({ L"Block", std::to_wstring (((track + SectorSource::kTracks - 3) % SectorSource::kTracks) * 4 + map.GetColumn (cell) / 4) });
        }

        items.push_back ({ L"Role", FormatRole (map.fileSystem, map.cells[cell].role) });

        if (!map.cells[cell].owners.empty())
        {
            items.push_back ({ map.cells[cell].owners.size() > 1 ? L"Owners" : L"Owner", FormatOwners (map, cell) });
        }
    }

    return items;
}
