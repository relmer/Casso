#include "Pch.h"

#include "CassoExplorer/Model/AppleFileProperties.h"
#include "Core/NumberFormat.h"
#include "Machines/Apple2/Common/ProDosTypeNames.h"
#include "Ui/DiskInspector/Blocks/BlockBrowser.h"
#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"





static constexpr uint32_t  s_kCatalogTree = 4;
static constexpr uint32_t  s_kExtentsTree = 3;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::Start
//
////////////////////////////////////////////////////////////////////////////////

void BlockBrowser::Start()
{
    vector<int>  volumes = GetVolumes();



    m_back.clear();
    m_forward.clear();
    m_place = BlockPlace();

    if (HasCard())
    {
        m_place.kind = BlockPlace::Kind::Card;
    }
    else if (!volumes.empty())
    {
        m_place.kind   = BlockPlace::Kind::Volume;
        m_place.volume = volumes[0];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::GoTo
//
////////////////////////////////////////////////////////////////////////////////

void BlockBrowser::GoTo (const BlockPlace & place)
{
    if (place == m_place)
    {
        return;
    }

    m_back.push_back (m_place);
    m_forward.clear();
    m_place = place;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::Replace
//
////////////////////////////////////////////////////////////////////////////////

void BlockBrowser::Replace (const BlockPlace & place)
{
    m_place = place;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::GoBack
//
////////////////////////////////////////////////////////////////////////////////

bool BlockBrowser::GoBack()
{
    if (m_back.empty())
    {
        return false;
    }

    m_forward.push_back (m_place);
    m_place = m_back.back();
    m_back.pop_back();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::GoForward
//
////////////////////////////////////////////////////////////////////////////////

bool BlockBrowser::GoForward()
{
    if (m_forward.empty())
    {
        return false;
    }

    m_back.push_back (m_place);
    m_place = m_forward.back();
    m_forward.pop_back();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::MakeBlockPlace
//
////////////////////////////////////////////////////////////////////////////////

BlockPlace BlockBrowser::MakeBlockPlace (uint32_t block) const
{
    BlockPlace  place;



    place.kind   = BlockPlace::Kind::Block;
    place.volume = m_model != nullptr ? m_model->FindVolume (block) : -1;
    place.block  = block;

    return place;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::GetContainer
//
////////////////////////////////////////////////////////////////////////////////

BlockPlace BlockBrowser::GetContainer (const BlockPlace & place) const
{
    BlockPlace  container = place;
    int         entry     = place.entry;
    uint32_t    inner     = 0;



    if (place.kind == BlockPlace::Kind::Card || place.kind == BlockPlace::Kind::Volume || place.kind == BlockPlace::Kind::Directory)
    {
        return place;
    }

    if (place.volume < 0 || m_model == nullptr || place.volume >= m_model->GetVolumeCount())
    {
        container.kind = HasCard() ? BlockPlace::Kind::Card : BlockPlace::Kind::Volume;
        return container;
    }

    const BlockMap  & map = m_model->GetVolumeMap (place.volume);

    if (place.kind == BlockPlace::Kind::Block)
    {
        inner = place.block - m_model->GetVolumeFirst (place.volume);
        entry = inner < map.GetBlockCount() && !map.blocks[inner].owners.empty() ? map.blocks[inner].owners[0] : -1;

        if (entry >= 0 && (size_t) entry < map.entries.size() && map.entries[entry].isDirectory)
        {
            return { BlockPlace::Kind::Directory, place.volume, entry };
        }
    }

    entry = entry >= 0 && (size_t) entry < map.entries.size() ? map.entries[entry].parent : -1;

    return entry >= 0 ? BlockPlace { BlockPlace::Kind::Directory, place.volume, entry } : BlockPlace { BlockPlace::Kind::Volume, place.volume, -1 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::GetVolumes
//
//  The volumes in the order of their partitions, or the one volume of an
//  image with none.
//
////////////////////////////////////////////////////////////////////////////////

vector<int> BlockBrowser::GetVolumes() const
{
    vector<int>  volumes;



    if (m_model == nullptr || !m_model->IsOpen())
    {
        return volumes;
    }

    for (const BlockBand & band : m_model->GetBands())
    {
        if (!band.isMap && band.volume >= 0 && band.volume < m_model->GetVolumeCount())
        {
            volumes.push_back (band.volume);
        }
    }

    if (volumes.empty() && m_model->GetVolumeCount() > 0 && !m_model->IsSectorVolume (0))
    {
        volumes.push_back (0);
    }

    return volumes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::HasCard
//
////////////////////////////////////////////////////////////////////////////////

bool BlockBrowser::HasCard() const
{
    size_t  count = 0;



    if (m_model == nullptr || !m_model->IsOpen())
    {
        return false;
    }

    for (const BlockBand & band : m_model->GetBands())
    {
        count += (!band.isMap && band.volume >= 0) ? 1 : 0;
    }

    return count > 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::BuildTree
//
////////////////////////////////////////////////////////////////////////////////

vector<DxuiTreeNode> BlockBrowser::BuildTree (const std::wstring & cardLabel) const
{
    vector<DxuiTreeNode>  volumes;
    DxuiTreeNode          card;
    bool                  isCard = HasCard();



    for (int volume : GetVolumes())
    {
        DxuiTreeNode  node;

        node.id       = MakeId ({ BlockPlace::Kind::Volume, volume });
        node.label    = GetVolumeName (volume);
        node.expanded = !isCard;
        node.children = BuildDirectoryNodes (volume, -1);
        volumes.push_back (std::move (node));
    }

    if (!isCard)
    {
        return volumes;
    }

    card.id       = MakeId ({ BlockPlace::Kind::Card });
    card.label    = cardLabel;
    card.expanded = true;
    card.children = std::move (volumes);

    return { card };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::BuildDirectoryNodes
//
////////////////////////////////////////////////////////////////////////////////

vector<DxuiTreeNode> BlockBrowser::BuildDirectoryNodes (int volume, int directory) const
{
    const BlockMap        & map = m_model->GetVolumeMap (volume);
    vector<DxuiTreeNode>    nodes;



    for (int i = 0; i < (int) map.entries.size(); i++)
    {
        if (map.entries[i].parent != directory || !map.entries[i].isDirectory || i == directory)
        {
            continue;
        }

        DxuiTreeNode  node;

        node.id       = MakeId ({ BlockPlace::Kind::Directory, volume, i });
        node.label    = map.entries[i].name;
        node.expanded = false;
        node.children = BuildDirectoryNodes (volume, i);
        nodes.push_back (std::move (node));
    }

    return nodes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::MakeId
//
//  "card", "v1" for volume 1, or "v1d7" for its entry 7.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockBrowser::MakeId (const BlockPlace & place)
{
    switch (place.kind)
    {
        case BlockPlace::Kind::Card:      return L"card";
        case BlockPlace::Kind::Volume:    return std::format (L"v{}", place.volume);
        case BlockPlace::Kind::Directory: return std::format (L"v{}d{}", place.volume, place.entry);
        default:                          break;
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::ParseId
//
////////////////////////////////////////////////////////////////////////////////

bool BlockBrowser::ParseId (const std::wstring & id, BlockPlace & outPlace)
{
    int     volume = -1;
    int     entry  = -1;
    size_t  split  = id.find (L'd');



    if (id == L"card")
    {
        outPlace = { BlockPlace::Kind::Card };
        return true;
    }

    if (id.size() < 2 || id[0] != L'v')
    {
        return false;
    }

    volume = _wtoi (id.substr (1, split == std::wstring::npos ? std::wstring::npos : split - 1).c_str());
    entry  = split == std::wstring::npos ? -1 : _wtoi (id.substr (split + 1).c_str());

    outPlace = { entry < 0 ? BlockPlace::Kind::Volume : BlockPlace::Kind::Directory, volume, entry };

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::BuildListing
//
////////////////////////////////////////////////////////////////////////////////

BlockListing BlockBrowser::BuildListing (const BlockPlace & place, const std::wstring & query) const
{
    BlockListing  listing;
    bool          isVolume = m_model != nullptr && place.volume >= 0 && place.volume < m_model->GetVolumeCount();



    if (m_model == nullptr || !m_model->IsOpen())
    {
        return listing;
    }

    if (!query.empty())
    {
        SetEntryColumns (listing);

        for (int volume : GetVolumes())
        {
            if (place.kind == BlockPlace::Kind::Card || volume == place.volume)
            {
                AddSearchRows (listing, volume, query);
            }
        }

        return listing;
    }

    switch (place.kind)
    {
        case BlockPlace::Kind::Card:
            AddCardRows (listing);
            break;

        case BlockPlace::Kind::Volume:
            if (isVolume)
            {
                SetEntryColumns (listing);
                AddVolumeRows (listing, place.volume);
                AddEntryRows  (listing, place.volume, -1);
            }

            break;

        case BlockPlace::Kind::Directory:
            if (isVolume && place.entry >= 0 && (size_t) place.entry < m_model->GetVolumeMap (place.volume).entries.size())
            {
                SetEntryColumns (listing);
                AddFileRows  (listing, place.volume, place.entry);
                AddEntryRows (listing, place.volume, place.entry);
            }

            break;

        case BlockPlace::Kind::File:
            if (isVolume && place.entry >= 0 && (size_t) place.entry < m_model->GetVolumeMap (place.volume).entries.size())
            {
                SetEntryColumns (listing);
                AddFileRows (listing, place.volume, place.entry);
            }

            break;

        case BlockPlace::Kind::Block:
            break;
    }

    std::erase_if (listing.groups, [] (const BlockListGroup & group) { return group.rows.empty(); });

    return listing;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::SetEntryColumns
//
////////////////////////////////////////////////////////////////////////////////

void BlockBrowser::SetEntryColumns (BlockListing & listing)
{
    listing.columns  = { L"Name", L"Type", L"Aux", L"Blocks", L"EOF", L"Storage", L"Block", L"Modified" };
    listing.isNumber = { false, false, false, true, true, false, true, false };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::AddCardRows
//
//  A row for each partition with a volume of blocks; a kind of damage is
//  one problem, however many blocks it is in.
//
////////////////////////////////////////////////////////////////////////////////

void BlockBrowser::AddCardRows (BlockListing & listing) const
{
    BlockListGroup        group  = { L"Partitions" };
    std::set<SectorRole>  kinds;
    int                   number = 0;
    size_t                used   = 0;
    size_t                damage = 0;



    listing.columns  = { L"Partition", L"File system", L"Name", L"Blocks", L"Used", L"Findings" };
    listing.isNumber = { true, false, false, true, true, false };

    for (int volume : GetVolumes())
    {
        const BlockMap  & map = m_model->GetVolumeMap (volume);
        BlockListRow      row;

        used = 0;
        kinds.clear();

        for (const BlockMapEntry & entry : map.blocks)
        {
            used += entry.role != SectorRole::Free ? 1 : 0;

            if (entry.role == SectorRole::AllocatedUnowned || entry.role == SectorRole::OwnedMarkedFree || entry.role == SectorRole::CrossLinked)
            {
                kinds.insert (entry.role);
            }
        }

        damage = kinds.size() + (map.isCatalogComplete ? 0 : 1);
        number++;

        row.kind   = BlockListRow::Kind::Partition;
        row.volume = volume;
        row.first  = m_model->GetVolumeFirst (volume);
        row.count  = m_model->GetVolumeSize (volume);
        row.cells  = { NumberFormat::FormatInteger (number), GetFileSystemName (map.fileSystem), GetVolumeName (volume), NumberFormat::FormatInteger (map.GetBlockCount()),
                       NumberFormat::FormatInteger ((int64_t) used),
                       damage == 0 ? std::wstring (L"OK") : damage == 1 ? std::wstring (L"1 problem") : NumberFormat::FormatInteger ((int64_t) damage) + L" problems" };

        group.rows.push_back (std::move (row));
    }

    listing.groups.push_back (std::move (group));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::AddVolumeRows
//
//  The volume's structures outside its files, as the boot blocks, the
//  volume directory and the bitmap, then its findings.
//
////////////////////////////////////////////////////////////////////////////////

void BlockBrowser::AddVolumeRows (BlockListing & listing, int volume) const
{
    const BlockMap                                        & map    = m_model->GetVolumeMap (volume);
    uint32_t                                                first  = m_model->GetVolumeFirst (volume);
    std::map<std::pair<int, uint32_t>, vector<uint32_t>>    parts;
    std::map<SectorRole, vector<uint32_t>>                  damage;
    BlockListGroup                                          structures = { L"Structures" };
    BlockListGroup                                          findings   = { L"Findings" };
    uint32_t                                                block  = 0;



    for (block = 0; block < map.GetBlockCount(); block++)
    {
        const BlockMapEntry  & entry = map.blocks[block];

        if (entry.role == SectorRole::AllocatedUnowned || entry.role == SectorRole::OwnedMarkedFree || entry.role == SectorRole::CrossLinked)
        {
            damage[entry.role].push_back (first + block);
        }

        if (entry.owners.empty() && GetStructureName (entry.structure, entry.tree) != nullptr)
        {
            parts[{ (int) entry.structure, entry.structure == BlockStructure::HfsNode ? entry.tree : 0 }].push_back (first + block);
        }
    }

    for (const auto & [key, blocks] : parts)
    {
        BlockStructure  structure = (BlockStructure) key.first;
        BlockListRow    row       = MakeBlocksRow (BlockListRow::Kind::Structure, volume, GetStructureName (structure, key.second), blocks);



        if (structure == BlockStructure::BootLoader || structure == BlockStructure::HfsBootBlock)
        {
            row.cells[kType] = map.bootLoader;
        }

        structures.rows.push_back (std::move (row));
    }

    for (const auto & [role, blocks] : damage)
    {
        findings.rows.push_back (MakeBlocksRow (BlockListRow::Kind::Finding, volume, BlockMap::GetRoleName (role), blocks));
    }

    for (const BlockCount & count : map.counts)
    {
        findings.rows.push_back (MakeBlocksRow (BlockListRow::Kind::Finding, volume, count.finding, { first + count.keyBlock }));
    }

    listing.groups.push_back (std::move (structures));
    listing.groups.push_back (std::move (findings));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::AddEntryRows
//
//  A directory's directories, then its files.
//
////////////////////////////////////////////////////////////////////////////////

void BlockBrowser::AddEntryRows (BlockListing & listing, int volume, int directory) const
{
    const BlockMap            & map    = m_model->GetVolumeMap (volume);
    vector<vector<uint32_t>>    blocks = map.GetBlocksOfEach();
    BlockListGroup              folders = { L"Directories" };
    BlockListGroup              files   = { L"Files" };



    for (int i = 0; i < (int) map.entries.size(); i++)
    {
        if (map.entries[i].parent == directory && i != directory)
        {
            (map.entries[i].isDirectory ? folders : files).rows.push_back (MakeEntryRow (volume, i, (uint32_t) blocks[i].size(), false));
        }
    }

    listing.groups.push_back (std::move (folders));
    listing.groups.push_back (std::move (files));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::AddSearchRows
//
////////////////////////////////////////////////////////////////////////////////

void BlockBrowser::AddSearchRows (BlockListing & listing, int volume, const std::wstring & query) const
{
    const BlockMap            & map    = m_model->GetVolumeMap (volume);
    vector<vector<uint32_t>>    blocks = map.GetBlocksOfEach();
    std::wstring                lower  = query;
    std::wstring                name;
    BlockListGroup              found  = { L"Found in " + GetVolumeName (volume) };



    std::transform (lower.begin(), lower.end(), lower.begin(), ::towlower);

    for (int i = 0; i < (int) map.entries.size(); i++)
    {
        name = map.entries[i].name;
        std::transform (name.begin(), name.end(), name.begin(), ::towlower);

        if (name.find (lower) != std::wstring::npos)
        {
            found.rows.push_back (MakeEntryRow (volume, i, (uint32_t) blocks[i].size(), true));
        }
    }

    if (!found.rows.empty())
    {
        listing.groups.push_back (std::move (found));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::AddFileRows
//
//  The file's index blocks, then its runs of data. The rows stop at
//  kMaxIndexRows and kMaxRunRows.
//
////////////////////////////////////////////////////////////////////////////////

void BlockBrowser::AddFileRows (BlockListing & listing, int volume, int file) const
{
    const BlockMap    & map     = m_model->GetVolumeMap (volume);
    uint32_t            first   = m_model->GetVolumeFirst (volume);
    vector<uint32_t>    data;
    BlockListGroup      indexes = { L"Index blocks" };
    BlockListGroup      runs    = { map.entries[file].isDirectory ? L"Directory blocks" : L"Data" };
    vector<Run>         found;
    size_t              i       = 0;



    for (uint32_t block : map.GetBlocksOf (file))
    {
        BlockStructure  structure = map.blocks[block].structure;
        LPCWSTR         label     = structure == BlockStructure::ProDosMasterIndex ? L"Master index block"
                                  : structure == BlockStructure::ProDosIndex       ? L"Index block"
                                  : structure == BlockStructure::ProDosExtendedKey ? L"Extended key block"
                                  : nullptr;

        if (label == nullptr)
        {
            data.push_back (first + block);
        }
        else if (indexes.rows.size() < kMaxIndexRows)
        {
            indexes.rows.push_back (MakeBlocksRow (BlockListRow::Kind::Blocks, volume, label, { first + block }));
        }
    }

    found = FindRuns (data);

    for (i = 0; i < found.size() && i < kMaxRunRows; i++)
    {
        vector<uint32_t>  run (found[i].count);

        std::iota (run.begin(), run.end(), found[i].first);
        runs.rows.push_back (MakeBlocksRow (BlockListRow::Kind::Blocks, volume, L"Run " + NumberFormat::FormatInteger ((int64_t) i + 1), run));
    }

    listing.groups.push_back (std::move (indexes));
    listing.groups.push_back (std::move (runs));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::MakeEntryRow
//
//  A ProDOS entry has its type, aux type, EOF, storage and date; another
//  file system's has its name, blocks and key block.
//
////////////////////////////////////////////////////////////////////////////////

BlockListRow BlockBrowser::MakeEntryRow (int volume, int entry, uint32_t blocks, bool isPath) const
{
    const BlockMap        & map      = m_model->GetVolumeMap (volume);
    const BlockFileEntry  & e        = map.entries[entry];
    uint32_t                first    = m_model->GetVolumeFirst (volume);
    bool                    isProDos = map.fileSystem == BlockFileSystem::ProDos || map.fileSystem == BlockFileSystem::Sos;
    BlockListRow            row;



    row.kind   = e.isDirectory ? BlockListRow::Kind::Directory : BlockListRow::Kind::File;
    row.volume = volume;
    row.entry  = entry;
    row.first  = e.keyBlock != BlockFileEntry::kNoBlock && e.keyBlock < map.GetBlockCount() ? first + e.keyBlock : first;
    row.cells.assign (kEntryColumns, std::wstring());

    row.cells[kName]   = isPath ? map.files[entry] : e.name;
    row.cells[kBlocks] = NumberFormat::FormatInteger (blocks);
    row.cells[kBlock]  = e.keyBlock != BlockFileEntry::kNoBlock && e.keyBlock < map.GetBlockCount() ? NumberFormat::FormatInteger (first + e.keyBlock) : std::wstring();

    if (isProDos)
    {
        row.cells[kType]     = ProDosTypeNames::Format (e.isDirectory ? (Byte) 0x0F : e.type);
        row.cells[kAux]      = e.isDirectory ? std::wstring() : std::format (L"${:04X}", e.aux);
        row.cells[kEof]      = e.isDirectory ? std::wstring() : NumberFormat::FormatInteger (e.eof);
        row.cells[kStorage]  = GetStorageName (e);
        row.cells[kModified] = AppleFileProperties::FormatDate (e.date, e.time);
    }
    else if (e.isDirectory)
    {
        row.cells[kType] = L"Directory";
    }

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::MakeBlocksRow
//
////////////////////////////////////////////////////////////////////////////////

BlockListRow BlockBrowser::MakeBlocksRow (BlockListRow::Kind kind, int volume, const std::wstring & name, const vector<uint32_t> & blocks) const
{
    BlockListRow  row;
    vector<Run>   runs = FindRuns (blocks);



    row.kind   = kind;
    row.volume = volume;
    row.first  = blocks.empty() ? 0 : blocks[0];
    row.cells.assign (kEntryColumns, std::wstring());

    if (runs.size() == 1)
    {
        row.count = runs[0].count;
    }
    else
    {
        row.blocks = blocks;
    }

    row.cells[kName]   = name;
    row.cells[kBlocks] = NumberFormat::FormatInteger ((int64_t) blocks.size());
    row.cells[kBlock]  = blocks.empty() ? std::wstring() : NumberFormat::FormatInteger (blocks[0]);

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::GetRowBlocks
//
//  Empty for a file's or a directory's row, whose entry is highlighted.
//
////////////////////////////////////////////////////////////////////////////////

vector<uint32_t> BlockBrowser::GetRowBlocks (const BlockListRow & row)
{
    vector<uint32_t>  blocks = row.blocks;



    if (blocks.empty() && row.count > 0)
    {
        blocks.resize (row.count);
        std::iota (blocks.begin(), blocks.end(), row.first);
    }

    return blocks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::FindRow
//
//  A file's or a directory's row covers its entry's blocks, a directory's
//  with every file's under it.
//
////////////////////////////////////////////////////////////////////////////////

int BlockBrowser::FindRow (const BlockListing & listing, uint32_t block) const
{
    int       index  = 0;
    int       volume = m_model != nullptr ? m_model->FindVolume (block) : -1;
    uint32_t  inner  = 0;



    for (const BlockListGroup & group : listing.groups)
    {
        for (const BlockListRow & row : group.rows)
        {
            bool  isMatch = false;

            if ((row.kind == BlockListRow::Kind::File || row.kind == BlockListRow::Kind::Directory) && row.volume == volume && volume >= 0)
            {
                const BlockMap  & map = m_model->GetVolumeMap (volume);
                vector<int>       own = row.kind == BlockListRow::Kind::Directory ? GetDescendants (map, row.entry) : vector<int> { row.entry };

                inner   = block - m_model->GetVolumeFirst (volume);
                isMatch = inner < map.GetBlockCount() &&
                          std::any_of (map.blocks[inner].owners.begin(), map.blocks[inner].owners.end(), [&own] (int owner) { return std::find (own.begin(), own.end(), owner) != own.end(); });
            }
            else if (row.count > 0)
            {
                isMatch = block >= row.first && block - row.first < row.count;
            }
            else
            {
                isMatch = std::find (row.blocks.begin(), row.blocks.end(), block) != row.blocks.end();
            }

            if (isMatch)
            {
                return index;
            }

            index++;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::BuildCrumbs
//
//  The card, the volume, each directory down to the file, then the block.
//  A block in a file has the file's path before it.
//
////////////////////////////////////////////////////////////////////////////////

vector<std::wstring> BlockBrowser::BuildCrumbs (const BlockPlace & place, const std::wstring & cardLabel, vector<BlockPlace> & outTargets) const
{
    vector<std::wstring>  segments;
    vector<int>           chain;
    int                   entry  = place.entry;
    int                   volume = place.volume;
    uint32_t              inner  = 0;



    outTargets.clear();

    auto  add = [&segments, &outTargets] (const std::wstring & text, const BlockPlace & target)
    {
        segments.push_back (text);
        outTargets.push_back (target);
    };

    if (m_model == nullptr || !m_model->IsOpen())
    {
        return segments;
    }

    if (HasCard())
    {
        add (cardLabel, { BlockPlace::Kind::Card });
    }

    if (place.kind != BlockPlace::Kind::Card && volume >= 0 && volume < m_model->GetVolumeCount())
    {
        const BlockMap  & map = m_model->GetVolumeMap (volume);

        add (GetVolumeName (volume), { BlockPlace::Kind::Volume, volume });

        if (place.kind == BlockPlace::Kind::Block)
        {
            inner = place.block - m_model->GetVolumeFirst (volume);
            entry = inner < map.GetBlockCount() && !map.blocks[inner].owners.empty() ? map.blocks[inner].owners[0] : -1;
        }

        for (int at = entry; at >= 0 && (size_t) at < map.entries.size() && chain.size() <= map.entries.size(); at = map.entries[at].parent)
        {
            chain.push_back (at);
        }

        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        {
            add (map.entries[*it].name, { map.entries[*it].isDirectory ? BlockPlace::Kind::Directory : BlockPlace::Kind::File, volume, *it });
        }
    }

    if (place.kind == BlockPlace::Kind::Block)
    {
        add (L"Block " + NumberFormat::FormatInteger (place.block), place);
    }

    return segments;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::GetDescendants
//
////////////////////////////////////////////////////////////////////////////////

vector<int> BlockBrowser::GetDescendants (const BlockMap & map, int directory)
{
    vector<int>  found   = { directory };
    size_t       visited = 0;



    while (visited < found.size() && found.size() <= map.entries.size())
    {
        for (int i = 0; i < (int) map.entries.size(); i++)
        {
            if (map.entries[i].parent == found[visited] && i != found[visited])
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
//  BlockBrowser::FindRuns
//
////////////////////////////////////////////////////////////////////////////////

vector<BlockBrowser::Run> BlockBrowser::FindRuns (const vector<uint32_t> & blocks)
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
//  BlockBrowser::DescribeBlocks
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockBrowser::DescribeBlocks (const vector<uint32_t> & blocks)
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
//  BlockBrowser::GetFileSystemName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockBrowser::GetFileSystemName (BlockFileSystem fileSystem)
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
//  BlockBrowser::GetStorageName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockBrowser::GetStorageName (const BlockFileEntry & entry)
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
//  BlockBrowser::GetVolumeName
//
//  The name in the volume's header, or the partition's label where the
//  header's name is empty.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockBrowser::GetVolumeName (int volume) const
{
    const BlockMap  & map = m_model->GetVolumeMap (volume);



    if (!map.volumeName.empty())
    {
        return map.volumeName;
    }

    for (const BlockBand & band : m_model->GetBands())
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
//  BlockBrowser::GetStructureName
//
//  The name of a structure the volume has outside its files, or null.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR BlockBrowser::GetStructureName (BlockStructure structure, uint32_t tree)
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
        case BlockStructure::HfsNode:                     return tree == s_kCatalogTree ? L"Catalog file" : tree == s_kExtentsTree ? L"Extents file" : L"B-tree nodes";
        case BlockStructure::Dos33Vtoc:                   return L"VTOC";
        case BlockStructure::Dos33Catalog:                return L"Catalog";
        default:                                          break;
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::FormatPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockBrowser::FormatPath (const BlockPlace & place) const
{
    vector<BlockPlace>    targets;
    vector<std::wstring>  segments = BuildCrumbs (place, std::wstring(), targets);
    std::wstring          path;
    size_t                i        = 0;



    for (i = 0; i < segments.size(); i++)
    {
        if (targets[i].kind != BlockPlace::Kind::Card)
        {
            path += L"/" + segments[i];
        }
    }

    return path.empty() ? L"/" : path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::FindPath
//
//  Each part of the path is the next place down: the card, a volume,
//  then a directory or a file in the place before it.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockBrowser::FindPath (const std::wstring & path, const std::wstring & cardLabel, BlockPlace & outPlace) const
{
    vector<std::wstring>  parts;
    std::wstring          part;
    BlockPlace            place;
    bool                  isFound = false;
    size_t                i       = 0;



    auto  isSame = [] (const std::wstring & a, const std::wstring & b)
    {
        return CompareStringOrdinal (a.c_str(), (int) a.size(), b.c_str(), (int) b.size(), TRUE) == CSTR_EQUAL;
    };

    if (m_model == nullptr || !m_model->IsOpen())
    {
        return false;
    }

    for (wchar_t ch : path)
    {
        if (ch == L'/' || ch == L'\\')
        {
            parts.push_back (part);
            part.clear();
        }
        else
        {
            part += ch;
        }
    }

    parts.push_back (part);
    std::erase_if (parts, [] (const std::wstring & text) { return text.empty(); });

    place = HasCard() ? BlockPlace { BlockPlace::Kind::Card } : BlockPlace { BlockPlace::Kind::Volume, 0 };

    //  The card's name, or for an image of one volume, its name, may lead.
    if (!parts.empty() && (HasCard() ? isSame (parts[0], cardLabel) : m_model->GetVolumeCount() > 0 && isSame (parts[0], GetVolumeName (0))))
    {
        parts.erase (parts.begin());
    }

    for (i = 0; i < parts.size(); i++)
    {
        isFound = false;

        for (const auto & [label, child] : GetChildPlaces (place))
        {
            if (!isFound && isSame (label, parts[i]))
            {
                place   = child;
                isFound = true;
            }
        }

        //  A file is a place too, though no menu lists it.
        if (!isFound && (place.kind == BlockPlace::Kind::Volume || place.kind == BlockPlace::Kind::Directory))
        {
            const BlockMap  & map    = m_model->GetVolumeMap (place.volume);
            int               parent = place.kind == BlockPlace::Kind::Volume ? -1 : place.entry;

            for (size_t entry = 0; entry < map.entries.size() && !isFound; entry++)
            {
                if (map.entries[entry].parent == parent && isSame (map.entries[entry].name, parts[i]))
                {
                    place   = { map.entries[entry].isDirectory ? BlockPlace::Kind::Directory : BlockPlace::Kind::File, place.volume, (int) entry };
                    isFound = true;
                }
            }
        }

        //  The volume alone may come first, with no card before it.
        if (!isFound && i == 0 && place.kind == BlockPlace::Kind::Card)
        {
            for (int volume : GetVolumes())
            {
                if (!isFound && isSame (GetVolumeName (volume), parts[i]))
                {
                    place   = { BlockPlace::Kind::Volume, volume };
                    isFound = true;
                }
            }
        }

        if (!isFound)
        {
            return false;
        }
    }

    outPlace = place;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser::GetChildPlaces
//
////////////////////////////////////////////////////////////////////////////////

vector<std::pair<std::wstring, BlockPlace>> BlockBrowser::GetChildPlaces (const BlockPlace & place) const
{
    vector<std::pair<std::wstring, BlockPlace>>  children;



    if (m_model == nullptr || !m_model->IsOpen())
    {
        return children;
    }

    if (place.kind == BlockPlace::Kind::Card)
    {
        for (int volume : GetVolumes())
        {
            children.push_back ({ GetVolumeName (volume), { BlockPlace::Kind::Volume, volume } });
        }
    }
    else if ((place.kind == BlockPlace::Kind::Volume || place.kind == BlockPlace::Kind::Directory) && place.volume >= 0 && place.volume < m_model->GetVolumeCount())
    {
        const BlockMap  & map    = m_model->GetVolumeMap (place.volume);
        int               parent = place.kind == BlockPlace::Kind::Volume ? -1 : place.entry;

        for (size_t entry = 0; entry < map.entries.size(); entry++)
        {
            if (map.entries[entry].parent == parent && map.entries[entry].isDirectory)
            {
                children.push_back ({ map.entries[entry].name, { BlockPlace::Kind::Directory, place.volume, (int) entry } });
            }
        }
    }

    return children;
}
