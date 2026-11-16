#include "Pch.h"

#include "CassoExplorer/Model/AppleFileProperties.h"
#include "Core/NumberFormat.h"
#include "Machines/Apple2/Common/ProDosTypeNames.h"
#include "Ui/DiskInspector/Blocks/BlockLevels.h"
#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"





static constexpr uint32_t  s_kCatalogTree = 4;
static constexpr uint32_t  s_kExtentsTree = 3;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels::BuildVolume
//
//  The volume's structures are the blocks of each structure no file owns,
//  as the boot blocks, the volume directory and the bitmap; a structure of
//  a file, as an index block, is at the File level.
//
////////////////////////////////////////////////////////////////////////////////

vector<BlockSection> BlockLevels::BuildVolume (const BlockInspectorModel & model, int volume)
{
    const BlockMap                                     & map      = model.GetVolumeMap (volume);
    uint32_t                                             first    = model.GetVolumeFirst (volume);
    std::map<std::pair<int, uint32_t>, vector<uint32_t>>  parts;
    std::map<SectorRole, vector<uint32_t>>                damage;
    vector<BlockSection>                                 sections (3);
    size_t                                               used     = 0;
    size_t                                               files    = 0;
    size_t                                               folders  = 0;
    uint32_t                                             block    = 0;
    uint32_t                                             tree     = 0;



    for (block = 0; block < map.GetBlockCount(); block++)
    {
        const BlockMapEntry  & entry = map.blocks[block];

        used += entry.role != SectorRole::Free ? 1 : 0;

        if (entry.role == SectorRole::AllocatedUnowned || entry.role == SectorRole::OwnedMarkedFree || entry.role == SectorRole::CrossLinked)
        {
            damage[entry.role].push_back (first + block);
        }

        if (entry.owners.empty() && GetStructureName (entry.structure) != nullptr)
        {
            tree = entry.structure == BlockStructure::HfsNode ? entry.tree : 0;
            parts[{ (int) entry.structure, tree }].push_back (first + block);
        }
    }

    for (const BlockFileEntry & entry : map.entries)
    {
        folders += entry.isDirectory ? 1 : 0;
        files   += entry.isDirectory ? 0 : 1;
    }

    sections[0].heading = L"Volume";
    sections[0].rows.push_back (MakeRow (L"Name",        GetVolumeName (model, volume)));
    sections[0].rows.push_back (MakeRow (L"File system", GetFileSystemName (map.fileSystem)));
    sections[0].rows.push_back (MakeRow (L"Blocks",      NumberFormat::FormatInteger (map.GetBlockCount())));
    sections[0].rows.push_back (MakeRow (L"Used",        DescribeShare (used, map.GetBlockCount())));
    sections[0].rows.push_back (MakeRow (L"Free",        DescribeShare (map.GetBlockCount() - used, map.GetBlockCount())));
    sections[0].rows.push_back (MakeRow (L"Files",       NumberFormat::FormatInteger ((int64_t) files)));
    sections[0].rows.push_back (MakeRow (L"Directories", NumberFormat::FormatInteger ((int64_t) folders)));

    sections[1].heading = L"Structures";

    for (const auto & [key, blocks] : parts)
    {
        LPCWSTR  name = GetStructureName ((BlockStructure) key.first);

        if (key.first == (int) BlockStructure::HfsNode)
        {
            name = key.second == s_kCatalogTree ? L"Catalog file" : key.second == s_kExtentsTree ? L"Extents file" : L"B-tree nodes";
        }

        sections[1].rows.push_back (MakeRow (name, DescribeBlocks (blocks), blocks[0]));
    }

    sections[2].heading = L"Findings";

    for (const auto & [role, blocks] : damage)
    {
        sections[2].rows.push_back (MakeRow (BlockMapGrid::GetRoleName (role), DescribeBlocks (blocks), blocks[0]));
    }

    if (!map.isCatalogComplete)
    {
        sections[2].rows.push_back (MakeRow (L"Catalog", L"Not read to its end"));
    }

    if (sections[2].rows.empty())
    {
        sections[2].rows.push_back (MakeRow (L"No problems found", L""));
    }

    if (!model.GetHeaderFields().empty())
    {
        sections.push_back ({ L"Container header", model.GetHeaderFields() });
    }

    return sections;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels::BuildFile
//
//  A ProDOS entry has its type, aux type, EOF, storage and dates; another
//  file system's has its name and blocks. The rows of index blocks and of
//  runs stop at kMaxIndexRows and kMaxRunRows, with a row for the rest.
//
////////////////////////////////////////////////////////////////////////////////

vector<BlockSection> BlockLevels::BuildFile (const BlockInspectorModel & model, int volume, int file)
{
    const BlockMap        & map      = model.GetVolumeMap (volume);
    uint32_t                first    = model.GetVolumeFirst (volume);
    const BlockFileEntry  & entry    = map.entries[file];
    bool                    isProDos = map.fileSystem == BlockFileSystem::ProDos || map.fileSystem == BlockFileSystem::Sos;
    vector<uint32_t>        blocks   = map.GetBlocksOf (file);
    vector<uint32_t>        content;
    vector<BlockSection>    sections (3);
    vector<Run>             runs;
    size_t                  shown   = 0;
    std::wstring            date;



    for (uint32_t & block : blocks)
    {
        BlockStructure  structure = map.blocks[block].structure;
        LPCWSTR         label     = structure == BlockStructure::ProDosMasterIndex ? L"Master index block"
                                  : structure == BlockStructure::ProDosIndex       ? L"Index block"
                                  : structure == BlockStructure::ProDosExtendedKey ? L"Extended key block"
                                  : nullptr;

        if (label == nullptr)
        {
            content.push_back (first + block);
        }
        else if (shown++ < kMaxIndexRows)
        {
            sections[1].rows.push_back (MakeRow (label, L"Block " + NumberFormat::FormatInteger (first + block), first + block));
        }
    }

    sections[0].heading = L"Entry";
    sections[0].rows.push_back (MakeRow (L"Name", entry.name));
    sections[0].rows.push_back (MakeRow (L"Path", map.files[file]));
    sections[0].rows.push_back (MakeRow (L"Kind", entry.isDirectory ? L"Directory" : L"File"));

    if (isProDos && !entry.isDirectory)
    {
        sections[0].rows.push_back (MakeRow (L"Type",     ProDosTypeNames::Format (entry.type)));
        sections[0].rows.push_back (MakeRow (L"Aux type", std::format (L"${:04X}", entry.aux)));
        sections[0].rows.push_back (MakeRow (L"EOF",      NumberFormat::FormatInteger (entry.eof) + (entry.eof == 1 ? L" byte" : L" bytes")));
    }

    sections[0].rows.push_back (MakeRow (L"Blocks", NumberFormat::FormatInteger ((int64_t) blocks.size())));

    if (isProDos)
    {
        date = AppleFileProperties::FormatDate (entry.date, entry.time);

        sections[0].rows.push_back (MakeRow (L"Storage",  GetStorageName (entry)));
        sections[0].rows.push_back (MakeRow (L"Modified", date.empty() ? std::wstring (L"No date") : date));
    }

    if (entry.keyBlock != BlockFileEntry::kNoBlock && entry.keyBlock < map.GetBlockCount())
    {
        sections[0].rows.push_back (MakeRow (L"Key block", L"Block " + NumberFormat::FormatInteger (first + entry.keyBlock), first + entry.keyBlock));
    }

    if (entry.entryBlock != BlockFileEntry::kNoBlock && entry.entryBlock < map.GetBlockCount())
    {
        sections[0].rows.push_back (MakeRow (L"Entry's directory block", L"Block " + NumberFormat::FormatInteger (first + entry.entryBlock), first + entry.entryBlock));
    }

    sections[1].heading = L"Index blocks";

    if (shown > kMaxIndexRows)
    {
        sections[1].rows.push_back (MakeRow (std::format (L"{} more", NumberFormat::FormatInteger ((int64_t) (shown - kMaxIndexRows))), L""));
    }

    runs = FindRuns (content);

    sections[2].heading = entry.isDirectory ? L"Directory blocks" : L"Data";
    sections[2].rows.push_back (MakeRow (L"Fragmentation", runs.size() <= 1 ? std::wstring (L"Contiguous") : NumberFormat::FormatInteger ((int64_t) runs.size()) + L" runs"));

    for (size_t i = 0; i < runs.size() && i < kMaxRunRows; i++)
    {
        vector<uint32_t>  run (runs[i].count);

        std::iota (run.begin(), run.end(), runs[i].first);
        sections[2].rows.push_back (MakeRow (L"Run " + NumberFormat::FormatInteger ((int64_t) i + 1), DescribeBlocks (run), runs[i].first));
    }

    if (runs.size() > kMaxRunRows)
    {
        sections[2].rows.push_back (MakeRow (std::format (L"{} more runs", NumberFormat::FormatInteger ((int64_t) (runs.size() - kMaxRunRows))), L""));
    }

    std::erase_if (sections, [] (const BlockSection & section) { return section.rows.empty(); });

    return sections;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels::GetCardColumns
//
////////////////////////////////////////////////////////////////////////////////

vector<std::wstring> BlockLevels::GetCardColumns()
{
    return { L"Partition", L"File system", L"Name", L"Blocks", L"Used", L"Findings" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels::BuildCard
//
////////////////////////////////////////////////////////////////////////////////

vector<BlockPartitionRow> BlockLevels::BuildCard (const BlockInspectorModel & model)
{
    vector<BlockPartitionRow>  rows;
    int                        number = 0;
    size_t                     used   = 0;
    size_t                     damage = 0;



    for (const BlockBand & band : model.GetBands())
    {
        if (band.isMap || band.volume < 0 || band.volume >= model.GetVolumeCount())
        {
            continue;
        }

        const BlockMap  & map = model.GetVolumeMap (band.volume);

        used   = 0;
        damage = map.isCatalogComplete ? 0 : 1;

        for (const BlockMapEntry & entry : map.blocks)
        {
            used   += entry.role != SectorRole::Free ? 1 : 0;
            damage += (entry.role == SectorRole::AllocatedUnowned || entry.role == SectorRole::OwnedMarkedFree || entry.role == SectorRole::CrossLinked) ? 1 : 0;
        }

        number++;

        rows.push_back ({ band.volume, { NumberFormat::FormatInteger (number),
                                         GetFileSystemName (map.fileSystem),
                                         GetVolumeName (model, band.volume),
                                         NumberFormat::FormatInteger (map.GetBlockCount()),
                                         DescribeShare (used, map.GetBlockCount()),
                                         damage == 0 ? std::wstring (L"OK") : damage == 1 ? std::wstring (L"1 problem") : NumberFormat::FormatInteger ((int64_t) damage) + L" problems" } });
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels::GetDescendants
//
////////////////////////////////////////////////////////////////////////////////

vector<int> BlockLevels::GetDescendants (const BlockMap & map, int directory)
{
    vector<int>  found   = { directory };
    size_t       visited = 0;



    while (visited < found.size())
    {
        for (int i = 0; i < (int) map.entries.size(); i++)
        {
            if (map.entries[i].parent == found[visited])
            {
                found.push_back (i);
            }
        }

        visited++;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels::FindRuns
//
////////////////////////////////////////////////////////////////////////////////

vector<BlockLevels::Run> BlockLevels::FindRuns (const vector<uint32_t> & blocks)
{
    vector<Run>  runs;



    for (uint32_t block : blocks)
    {
        if (!runs.empty() && runs.back().first + runs.back().count == block)
        {
            runs.back().count++;
        }
        else
        {
            runs.push_back ({ block, 1 });
        }
    }

    return runs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels::DescribeBlocks
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockLevels::DescribeBlocks (const vector<uint32_t> & blocks)
{
    vector<Run>  runs = FindRuns (blocks);



    if (blocks.empty())
    {
        return std::wstring();
    }

    if (blocks.size() == 1)
    {
        return L"Block " + NumberFormat::FormatInteger (blocks[0]);
    }

    if (runs.size() == 1)
    {
        return L"Blocks " + NumberFormat::FormatInteger (runs[0].first) + L" to " + NumberFormat::FormatInteger (runs[0].first + runs[0].count - 1);
    }

    return NumberFormat::FormatInteger ((int64_t) blocks.size()) + L" blocks in " + NumberFormat::FormatInteger ((int64_t) runs.size()) + L" runs";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels::GetFileSystemName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockLevels::GetFileSystemName (BlockFileSystem fileSystem)
{
    switch (fileSystem)
    {
        case BlockFileSystem::ProDos: return L"ProDOS";
        case BlockFileSystem::Sos:    return L"SOS";
        case BlockFileSystem::Pascal: return L"Apple Pascal";
        case BlockFileSystem::Hfs:    return L"HFS";
        case BlockFileSystem::Dos33:  return L"DOS 3.3";
        case BlockFileSystem::None:   break;
    }

    return L"No standard file system";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels::GetStorageName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockLevels::GetStorageName (const BlockFileEntry & entry)
{
    static constexpr Byte  kSeedling = 0x1;
    static constexpr Byte  kSapling  = 0x2;
    static constexpr Byte  kTree     = 0x3;
    static constexpr Byte  kExtended = 0x5;



    if (entry.isDirectory)
    {
        return L"Directory";
    }

    switch (entry.storage)
    {
        case kSeedling: return L"Seedling";
        case kSapling:  return L"Sapling";
        case kTree:     return L"Tree";
        case kExtended: return L"Extended, with a resource fork";
        default:        break;
    }

    return std::format (L"${:X}", entry.storage);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels::GetVolumeName
//
//  The name in the volume's header, or the partition's label where the
//  header's name is empty.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockLevels::GetVolumeName (const BlockInspectorModel & model, int volume)
{
    const BlockMap  & map = model.GetVolumeMap (volume);



    if (!map.volumeName.empty())
    {
        return map.volumeName;
    }

    for (const BlockBand & band : model.GetBands())
    {
        if (band.volume == volume && !band.label.empty())
        {
            return band.label;
        }
    }

    return L"Untitled";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels::GetStructureName
//
//  The name of a structure the volume has outside its files, or null.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR BlockLevels::GetStructureName (BlockStructure structure)
{
    switch (structure)
    {
        case BlockStructure::BootLoader:                  return L"Boot blocks";
        case BlockStructure::ProDosDirectory:             return L"Volume directory";
        case BlockStructure::ProDosBitmap:                return L"Volume bitmap";
        case BlockStructure::PascalDirectory:             return L"Directory";
        case BlockStructure::HfsBootBlock:                return L"Boot blocks";
        case BlockStructure::HfsMasterDirectory:          return L"Master directory block";
        case BlockStructure::HfsAlternateMasterDirectory: return L"Alternate master directory block";
        case BlockStructure::HfsVolumeBitmap:             return L"Volume bitmap";
        case BlockStructure::HfsNode:                     return L"B-tree nodes";
        case BlockStructure::Dos33Vtoc:                   return L"VTOC";
        case BlockStructure::Dos33Catalog:                return L"Catalog";
        default:                                          break;
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels::MakeRow
//
////////////////////////////////////////////////////////////////////////////////

BlockField BlockLevels::MakeRow (const std::wstring & label, const std::wstring & value, int64_t link)
{
    BlockField  row;



    row.label = label;
    row.value = value;
    row.link  = link;

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels::DescribeShare
//
//  A count and its share of the total, as "25 (0.4%)".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockLevels::DescribeShare (size_t count, size_t total)
{
    static constexpr double  kPercent = 100.0;



    return NumberFormat::FormatInteger ((int64_t) count) + L" (" + NumberFormat::FormatFixed (total == 0 ? 0.0 : kPercent * (double) count / (double) total, 1, true) + L"%)";
}
