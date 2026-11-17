#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMapWriter.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MappedFile::GetUsedCount
//
////////////////////////////////////////////////////////////////////////////////

int MappedFile::GetUsedCount() const
{
    return static_cast<int> (std::count_if (sectors.begin(), sectors.end(), [] (const FilePlace & place) { return place.cell >= 0; }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::FileMapWriter
//
//  A cell per DOS-order sector index, ProDOS or Pascal block, or CP/M
//  sector; a block's result is the worse of its two halves (FR-086).
//
////////////////////////////////////////////////////////////////////////////////

FileMapWriter::FileMapWriter (const SectorSource & source, FileMap & map, MapFileSystem fileSystem) :
    m_source (source),
    m_map    (map)
{
    bool  isBlocks = fileSystem == MapFileSystem::ProDos || fileSystem == MapFileSystem::Pascal;
    int   cell     = 0;



    m_map.fileSystem    = fileSystem;
    m_map.tracks        = SectorSource::kTracks;
    m_map.cellsPerTrack = isBlocks ? SectorSource::kBlocksPerTrack : SectorSource::kSectors;
    m_map.cells.assign (static_cast<size_t> (m_map.tracks * m_map.cellsPerTrack), MapCell());
    m_structure.assign (m_map.cells.size(), SectorRole::Free);
    m_ownerRole.assign (m_map.cells.size(), SectorRole::Free);

    for (cell = 0; cell < static_cast<int> (m_map.cells.size()); cell++)
    {
        MapCell &  out = m_map.cells[cell];

        if (isBlocks)
        {
            out.halves = { source.GetHalf (cell, 0).result, source.GetHalf (cell, 1).result };
            out.result = std::max (out.halves[0], out.halves[1]);
        }
        else
        {
            out.result = (fileSystem == MapFileSystem::Cpm) ? source.GetCpm (m_map.GetTrack (cell), m_map.GetColumn (cell)).result
                                                            : source.GetDos33 (m_map.GetTrack (cell), m_map.GetColumn (cell)).result;
            out.halves = { out.result, out.result };
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::AddFile
//
////////////////////////////////////////////////////////////////////////////////

int FileMapWriter::AddFile (MappedFile file)
{
    m_map.files.push_back (std::move (file));

    return static_cast<int> (m_map.files.size()) - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::Own
//
//  A sector in a file's order; a deleted file lists it without owning it.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::Own (int file, int cell, SectorRole role)
{
    MappedFile &  owner = m_map.files[file];



    owner.sectors.push_back ({ cell, role });

    if (!owner.isDeleted && cell >= 0 && cell < static_cast<int> (m_map.cells.size()))
    {
        vector<int> &  owners = m_map.cells[cell].owners;

        if (std::find (owners.begin(), owners.end(), file) == owners.end())
        {
            owners.push_back (file);
        }

        m_ownerRole[cell] = role;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::AddHole
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::AddHole (int file)
{
    m_map.files[file].sectors.push_back ({ -1, SectorRole::FileData });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::SetStructure
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::SetStructure (int cell, SectorRole role)
{
    if (cell >= 0 && cell < static_cast<int> (m_structure.size()))
    {
        m_structure[cell] = role;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::SetMarkedUsed
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::SetMarkedUsed (vector<bool> markedUsed)
{
    m_markedUsed = std::move (markedUsed);
    m_hasBitmap  = m_markedUsed.size() == m_map.cells.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::BreakChain
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::BreakChain (int file, ChainReason reason, int cell)
{
    m_map.files[file].chainReason = reason;
    m_map.files[file].chainCell   = cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::Step
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapWriter::Step (int cell, std::set<int> & visited, ChainReason & outReason) const
{
    outReason = (cell < 0 || cell >= static_cast<int> (m_map.cells.size()))  ? ChainReason::OutsideVolume
              : visited.contains (cell)                                       ? ChainReason::Loop
              : visited.size() >= m_map.cells.size()                          ? ChainReason::TooLong
              : m_map.cells[cell].result == SectorResult::Missing             ? ChainReason::MissingSector
              : m_map.cells[cell].result == SectorResult::Bad                 ? ChainReason::BadSector
              :                                                                 ChainReason::None;

    if (outReason == ChainReason::None)
    {
        visited.insert (cell);
    }

    return outReason == ChainReason::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::AddFinding
//
//  A file system finding, placed at the physical sector that contains the cell
//  (the first half of a block), its text whole in the detail.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::AddFinding (FindingKind kind, int cell, const std::wstring & text)
{
    AddFinding (kind, cell, text, GetWeight (kind));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::AddFinding
//
//  The same with a weight other than the kind's, as for a chain that stops
//  at a sector that can't be read.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::AddFinding (FindingKind kind, int cell, const std::wstring & text, FindingWeight weight)
{
    Finding  finding;



    finding.category = FindingCategory::FileSystem;
    finding.kind     = kind;
    finding.weight   = weight;
    finding.detail   = TextEncoding::WideToUtf8 (text);

    if (cell >= 0 && cell < static_cast<int> (m_map.cells.size()))
    {
        finding.mapCell      = cell;
        finding.quarterTrack = m_map.GetTrack (cell) * DiskImage::kQuarterTracksPerWholeTrack;
        finding.sector       = GetPhysical (cell);
    }

    m_map.findings.push_back (std::move (finding));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::CheckCount
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::CheckCount (int file, int cell, int offset, LPCWSTR what)
{
    const MappedFile &  mapped = m_map.files[file];
    int                 used   = mapped.GetUsedCount();



    if (!mapped.isDeleted && mapped.chainReason == ChainReason::None && mapped.recordedCount >= 0 && mapped.recordedCount != used)
    {
        AddFinding (FindingKind::FileCountDiffers, cell, std::format (L"{}: expected a {} of {}, but found {}", mapped.path, what, used, mapped.recordedCount));
        AddCount   ({ cell, offset, mapped.recordedCount, used, std::format (L"the {} of {}", what, mapped.path) });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::AddCount
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::AddCount (const RecordedCount & count)
{
    RecordedCount  stamped = count;



    if (!m_map.findings.empty())
    {
        stamped.kind    = m_map.findings.back().kind;
        stamped.finding = m_map.findings.back().detail;
    }

    m_map.counts.push_back (std::move (stamped));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::Finalize
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::Finalize()
{
    int  cell = 0;



    for (cell = 0; cell < static_cast<int> (m_map.cells.size()); cell++)
    {
        SettleCell (cell);
    }

    SettleFiles();

    if (m_map.fileSystem != MapFileSystem::Cpm)
    {
        CheckRepeatedBytes();
        CheckAlmostBlank();
    }

    if (m_map.usesUnchecked)
    {
        AddFinding (FindingKind::FileMapUnchecked, -1, std::format (L"{} sectors of the file map were read with checksum checks turned off in Decode settings", CountUnchecked()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::SettleCell
//
//  One owner gives its role, two or more make the sector cross-linked, and
//  none leave the volume's own structure or, on a volume with a bitmap,
//  allocated but unowned when the bitmap marks it used.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::SettleCell (int cell)
{
    MapCell &     out    = m_map.cells[cell];
    bool          isUsed = m_hasBitmap && m_markedUsed[cell];
    std::wstring  names;



    if (out.owners.size() > 1)
    {
        for (int owner : out.owners)
        {
            names += (names.empty() ? L"" : (owner == out.owners.back() ? L" and " : L", ")) + m_map.files[owner].path;
        }

        out.role = SectorRole::CrossLinked;
        AddFinding (FindingKind::FileCrossLinked, cell, L"Files " + names + L" are cross-linked at " + DescribeCell (cell));
    }
    else if (out.owners.size() == 1 && IsStructure (cell))
    {
        out.role = SectorRole::CrossLinked;
        AddFinding (FindingKind::FileUsesStructure, cell, Capitalize (DescribeCell (cell)) + L" is " + DescribeStructure (cell) + L", but " + m_map.files[out.owners[0]].path + L" uses it");
    }
    else if (out.owners.size() == 1 && m_hasBitmap && !isUsed)
    {
        out.role = SectorRole::OwnedMarkedFree;
        AddFinding (FindingKind::FileOwnedMarkedFree, cell, Capitalize (DescribeCell (cell)) + L" is used by " + m_map.files[out.owners[0]].path + L" but marked free");
    }
    else if (out.owners.size() == 1)
    {
        out.role = m_ownerRole[cell];
    }
    else if (m_structure[cell] != SectorRole::Free)
    {
        out.role = m_structure[cell];
    }
    else if (isUsed)
    {
        out.role = SectorRole::AllocatedUnowned;
        AddFinding (FindingKind::FileAllocatedUnowned, cell, Capitalize (DescribeCell (cell)) + L" is marked used but isn't in any file");
    }

    m_map.usesUnchecked = m_map.usesUnchecked || (out.result == SectorResult::NotChecked && out.role != SectorRole::Free);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::SettleFiles
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::SettleFiles()
{
    for (MappedFile & file : m_map.files)
    {
        for (const FilePlace & place : file.sectors)
        {
            if (place.cell < 0)
            {
                continue;
            }

            const MapCell &  cell = m_map.cells[place.cell];

            if (file.isDeleted)
            {
                file.stillFree    += cell.owners.empty() ? 1 : 0;
                file.usedByOthers += cell.owners.empty() ? 0 : 1;
            }
            else
            {
                file.states |= (cell.owners.size() > 1) ? MappedFile::kCrossLinked : 0;
                file.states |= (cell.result == SectorResult::Bad || cell.result == SectorResult::Missing) ? MappedFile::kTouchesBadSectors : 0;
            }
        }

        if (!file.isDeleted && file.chainReason != ChainReason::None)
        {
            file.states |= MappedFile::kChainBroken;
            AddFinding (FindingKind::FileChainBroken, file.chainCell, file.path + L"'s chain " + DescribeChain (file.chainReason, file.chainCell));
        }

        if (!file.isDeleted && (file.states & MappedFile::kTouchesBadSectors) != 0)
        {
            AddFinding (FindingKind::FileTouchesBadSectors, -1, std::format (L"{} uses {} bad or missing {}", file.path, CountBad (file), CountBad (file) == 1 ? L"sector" : L"sectors"));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::CheckRepeatedBytes
//
//  A file whose data is one byte value throughout, up to the size in its
//  entry on ProDOS and Apple Pascal, is worth a look: a capture that read
//  nothing produces such a file, and so does a saved black screen. A file
//  with less than a sector of data to compare, or with a sector that can't
//  be read, isn't checked.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::CheckRepeatedBytes()
{
    vector<Byte>  bytes;
    vector<Byte>  data;
    bool          isRead  = true;
    size_t        count   = 0;
    bool          isSized = m_map.fileSystem == MapFileSystem::ProDos || m_map.fileSystem == MapFileSystem::Pascal;



    for (const MappedFile & file : m_map.files)
    {
        if (file.isDeleted || file.chainReason != ChainReason::None)
        {
            continue;
        }

        data.clear();
        isRead = true;

        for (const FilePlace & place : file.sectors)
        {
            if (place.cell < 0 || place.role != SectorRole::FileData)
            {
                continue;
            }

            isRead = isRead && ReadCell (place.cell, bytes);
            data.insert (data.end(), bytes.begin(), bytes.end());
        }

        count = isSized ? std::min<size_t> (data.size(), static_cast<size_t> (file.recordedSize)) : data.size();

        if (isRead && count >= static_cast<size_t> (SectorSource::kBytes) && IsOneByte (data, count))
        {
            AddFinding (FindingKind::FileRepeatedByte, file.sectors.front().cell, std::format (L"{}'s data is blank: every byte is ${:02X}", file.path, data[0]));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::CheckAlmostBlank
//
//  An image almost every sector of which is one byte value throughout, on a
//  volume whose files are almost all blank too, is damage: a capture of an
//  erased disk produces such an image. An empty volume isn't checked, since
//  a newly formatted disk is blank except for its structures.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapWriter::CheckAlmostBlank()
{
    static constexpr int  kBlankPercent     = 95;
    static constexpr int  kBlankFilePercent = 90;
    static constexpr int  kPercent          = 100;



    vector<Byte>  bytes;
    int           sectors    = SectorSource::kTracks * SectorSource::kSectors;
    int           blank      = 0;
    int           fileCells  = 0;
    int           blankCells = 0;
    int           cell       = 0;



    for (int s = 0; s < sectors; s++)
    {
        const SectorSource::Sector &  sector = m_source.GetPhysical (s / SectorSource::kSectors, s % SectorSource::kSectors);

        if (!SectorSource::IsReadable (sector.result) || sector.bytes == nullptr)
        {
            blank++;
            continue;
        }

        bytes.assign (sector.bytes, sector.bytes + SectorSource::kBytes);
        blank += IsOneByte (bytes, bytes.size()) ? 1 : 0;
    }

    for (cell = 0; cell < static_cast<int> (m_map.cells.size()); cell++)
    {
        if (m_map.cells[cell].owners.empty() || m_ownerRole[cell] != SectorRole::FileData)
        {
            continue;
        }

        fileCells++;
        blankCells += (!ReadCell (cell, bytes) || IsOneByte (bytes, bytes.size())) ? 1 : 0;
    }

    if (fileCells > 0 && blank * kPercent >= kBlankPercent * sectors && blankCells * kPercent >= kBlankFilePercent * fileCells)
    {
        AddFinding (FindingKind::ImageAlmostBlank, -1, std::format (L"{} of the disk's {} sectors are filled with one repeated byte", blank, sectors));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::ReadCell
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapWriter::ReadCell (int cell, vector<Byte> & outBytes) const
{
    std::array<Byte, 2 * SectorSource::kBytes>  block  = {};
    bool                                        isRead = cell >= 0 && cell < static_cast<int> (m_map.cells.size());



    outBytes.clear();

    if (isRead && (m_map.fileSystem == MapFileSystem::ProDos || m_map.fileSystem == MapFileSystem::Pascal))
    {
        isRead = m_source.ReadBlock (cell, block);
        outBytes.assign (block.begin(), block.end());
    }
    else if (isRead)
    {
        const SectorSource::Sector &  sector = (m_map.fileSystem == MapFileSystem::Cpm) ? m_source.GetCpm (m_map.GetTrack (cell), m_map.GetColumn (cell))
                                                                                       : m_source.GetDos33 (m_map.GetTrack (cell), m_map.GetColumn (cell));

        isRead = SectorSource::IsReadable (sector.result) && sector.bytes != nullptr;

        if (isRead)
        {
            outBytes.assign (sector.bytes, sector.bytes + SectorSource::kBytes);
        }
    }

    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::IsOneByte
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapWriter::IsOneByte (const vector<Byte> & bytes, size_t count)
{
    size_t  end = std::min (count, bytes.size());



    return end > 0 && std::all_of (bytes.begin(), bytes.begin() + static_cast<ptrdiff_t> (end), [&] (Byte b) { return b == bytes[0]; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::DescribeCell
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapWriter::DescribeCell (int cell) const
{
    std::wstring  text;



    if (cell < 0 || cell >= static_cast<int> (m_map.cells.size()))
    {
        text = std::format (L"a pointer outside the volume");
    }
    else if (m_map.fileSystem == MapFileSystem::ProDos || m_map.fileSystem == MapFileSystem::Pascal)
    {
        text = std::format (L"block {}", cell);
    }
    else
    {
        text = std::format (L"track {}, {}sector {}", m_map.GetTrack (cell), m_map.fileSystem == MapFileSystem::Cpm ? L"CP/M " : L"",
                            InspectorFormat::FormatSector (m_map.GetColumn (cell)));
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::GetPhysical
//
////////////////////////////////////////////////////////////////////////////////

int FileMapWriter::GetPhysical (int cell) const
{
    int  physical = 0;



    switch (m_map.fileSystem)
    {
        case MapFileSystem::ProDos:
        case MapFileSystem::Pascal: physical = SectorSource::GetBlockPhysical (cell, 0);                  break;
        case MapFileSystem::Cpm:    physical = SectorSource::GetCpmPhysical (m_map.GetColumn (cell));   break;
        default:                    physical = SectorSource::GetDos33Physical (m_map.GetColumn (cell)); break;
    }

    return physical;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::DescribeChain
//
//  Where a chain stopped, after "the chain".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapWriter::DescribeChain (ChainReason reason, int cell) const
{
    std::wstring  text;



    switch (reason)
    {
        case ChainReason::Loop:          text = L"loops back to " + DescribeCell (cell);                         break;
        case ChainReason::OutsideVolume: text = L"points outside the volume";                                   break;
        case ChainReason::BadSector:     text = L"leads to " + DescribeCell (cell) + L", which has a bad checksum"; break;
        case ChainReason::MissingSector: text = L"leads to " + DescribeCell (cell) + L", which is missing";        break;
        case ChainReason::TooLong:       text = L"is longer than the volume";                                   break;
        default:                                                                                                 break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::DescribeUnreadable
//
//  A cell that can't be read, as a clause.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapWriter::DescribeUnreadable (int cell) const
{
    bool  isMissing = cell >= 0 && cell < static_cast<int> (m_map.cells.size()) && m_map.cells[cell].result == SectorResult::Missing;



    return DescribeCell (cell) + (isMissing ? L" is missing" : L" has a bad checksum");
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::IsStructure
//
//  A cell of the volume's bookkeeping that no file may use: the VTOC, a
//  catalog sector and the boot sector on DOS 3.3, whose other sectors on
//  tracks 0 to 2 can be allocated to files on a data disk; the boot blocks,
//  the volume directory and the bitmap on ProDOS; the boot blocks and the
//  directory on Apple Pascal. CP/M is checked in CpmMapReader.
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapWriter::IsStructure (int cell) const
{
    SectorRole  role = m_structure[cell];



    return m_map.fileSystem != MapFileSystem::Cpm &&
           (role == SectorRole::VtocOrKeyBlock || role == SectorRole::CatalogOrDirectory || role == SectorRole::VolumeBitmap ||
            (role == SectorRole::BootArea && (m_map.fileSystem != MapFileSystem::Dos33 || cell == 0)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::DescribeStructure
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapWriter::DescribeStructure (int cell) const
{
    bool          isDos = m_map.fileSystem == MapFileSystem::Dos33;
    std::wstring  text;



    switch (m_structure[cell])
    {
        case SectorRole::VtocOrKeyBlock:     text = isDos ? L"the VTOC" : L"the volume directory's key block";                                                  break;
        case SectorRole::CatalogOrDirectory: text = isDos ? L"a catalog sector" : (m_map.fileSystem == MapFileSystem::ProDos) ? L"a volume directory block" : L"a directory block"; break;
        case SectorRole::VolumeBitmap:       text = L"a volume bitmap block";                                                                                   break;
        default:                             text = isDos ? L"the boot sector" : L"a boot block";                                                               break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::Capitalize
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileMapWriter::Capitalize (std::wstring text)
{
    if (!text.empty())
    {
        text[0] = static_cast<wchar_t> (towupper (text[0]));
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::CountUnchecked
//
////////////////////////////////////////////////////////////////////////////////

int FileMapWriter::CountUnchecked() const
{
    return static_cast<int> (std::count_if (m_map.cells.begin(), m_map.cells.end(), [] (const MapCell & cell)
    {
        return cell.result == SectorResult::NotChecked && cell.role != SectorRole::Free;
    }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::CountBad
//
////////////////////////////////////////////////////////////////////////////////

int FileMapWriter::CountBad (const MappedFile & file) const
{
    return static_cast<int> (std::count_if (file.sectors.begin(), file.sectors.end(), [this] (const FilePlace & place)
    {
        return place.cell >= 0 && (m_map.cells[place.cell].result == SectorResult::Bad || m_map.cells[place.cell].result == SectorResult::Missing);
    }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter::GetWeight
//
//  Damage makes a volume Bad: a link off the volume, a loop, a sector two
//  files use or a file uses that the bitmap marks free, a count that
//  disagrees, a header or directory that can't be read, or an image almost
//  blank. A file of one repeated byte is worth a look. A finding that is only
//  worth a look makes it Suspect, and a note doesn't change the verdict.
//
////////////////////////////////////////////////////////////////////////////////

FindingWeight FileMapWriter::GetWeight (FindingKind kind)
{
    FindingWeight  weight = FindingWeight::Note;



    switch (kind)
    {
        case FindingKind::FileCrossLinked:
        case FindingKind::FileOwnedMarkedFree:
        case FindingKind::FileChainBroken:
        case FindingKind::FileCountDiffers:
        case FindingKind::PascalEntryOutOfRange:
        case FindingKind::PascalFileCountDiffers:
        case FindingKind::ProDosFileCountDiffers:
        case FindingKind::Dos33HeaderLost:
        case FindingKind::ImageAlmostBlank:
        case FindingKind::CpmExtentMissing:
        case FindingKind::CpmRecordsPastBlocks:
        case FindingKind::CpmBlockInDirectory:
        case FindingKind::FileUsesStructure:
        case FindingKind::Dos33StructureMarkedFree:
        case FindingKind::ProDosEofPastStorage:
            weight = FindingWeight::Damage;
            break;

        case FindingKind::FileTouchesBadSectors:
        case FindingKind::PascalEntryOutOfOrder:
        case FindingKind::CpmInvalidEntry:
        case FindingKind::CpmDuplicateEntry:
        case FindingKind::Dos33SectorBytesDiffer:
        case FindingKind::Dos33PairsDiffer:
        case FindingKind::Dos33NextTrackOffDisk:
        case FindingKind::Dos33EntryNameMixed:
        case FindingKind::Dos33EntryTypeUnknown:
        case FindingKind::Dos33EntryCountTooLarge:
        case FindingKind::Dos33CatalogNonstandard:
        case FindingKind::ProDosDirectoryPointer:
        case FindingKind::ProDosParentPointer:
        case FindingKind::ProDosBitmapPastEnd:
        case FindingKind::ProDosNameInvalid:
        case FindingKind::FileRepeatedByte:
        case FindingKind::PascalLastBytesInvalid:
        case FindingKind::PascalNameInvalid:
            weight = FindingWeight::Suspect;
            break;

        default:
            break;
    }

    return weight;
}
