#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/Blocks/ApplePartitionMap.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/Blocks/BlockMapBuilder.h"
#include "Devices/Disk/Inspector/Blocks/Dos33SectorLayout.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::BlockInspectorModel
//
////////////////////////////////////////////////////////////////////////////////

BlockInspectorModel::BlockInspectorModel()  = default;
BlockInspectorModel::~BlockInspectorModel() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::Open (std::unique_ptr<BlockImageFile> file)
{
    HRESULT  hr     = S_OK;
    bool     isFile = file != nullptr;



    CBRAEx (isFile, E_INVALIDARG);

    m_file = std::move (file);

    m_history.Clear();
    m_edits     = BlockEditBuffer();
    m_applied.clear();
    m_appliedAt = 0;
    ClearHighlight();
    ClearCache();

    hr = BuildVolumes();
    CHR (hr);

    m_headerFields.clear();

    if (!m_file->GetHeader().empty())
    {
        m_file->AnnotateHeader (m_headerFields);
    }

    m_block = 0;
    m_history.Record (m_block);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::ClearCache
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::ClearCache()
{
    m_stored.clear();
    m_storedOther.clear();
    m_tags.clear();
    m_untagged.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::LoadBlock
//
//  A block that can't be read is cached as zeros, as ReadStoredBlock returns
//  it, so it is drawn as zeros rather than read again.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::LoadBlock (uint32_t block)
{
    Block    bytes = {};
    Tags     tags  = {};
    HRESULT  hr    = S_OK;



    if (m_file == nullptr || block >= GetBlockCount())
    {
        return;
    }

    hr = ReadStoredBlock (block, bytes);

    if (FAILED (hr))
    {
        m_stored[block] = bytes;
    }

    if (!m_tags.contains (block) && !m_untagged.contains (block))
    {
        if (m_file->TryReadTags (block, tags))
        {
            m_tags[block] = tags;
        }
        else
        {
            m_untagged.insert (block);
        }
    }

    if (IsComparing())
    {
        hr = ReadOtherBlock (block, bytes);

        if (FAILED (hr))
        {
            m_storedOther[block] = bytes;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::IsLoaded
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorModel::IsLoaded (uint32_t block) const
{
    bool  isStored = m_stored.contains (block);
    bool  isTagged = m_tags.contains (block) || m_untagged.contains (block);
    bool  isOther  = !IsComparing() || m_storedOther.contains (block);



    return isStored && isTagged && isOther;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::TryGetTags
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorModel::TryGetTags (uint32_t block, Tags & outTags) const
{
    auto  found = m_tags.find (block);



    if (found == m_tags.end())
    {
        return false;
    }

    outTags = found->second;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::GetBlockCount
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockInspectorModel::GetBlockCount() const
{
    return m_file != nullptr ? m_file->GetBlockCount() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::BuildVolumes
//
//  An Apple Partition Map first, then the layouts PartitionAccess reads,
//  and with neither the image as one volume.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::BuildVolumes()
{
    HRESULT  hr = S_OK;



    m_volumes.clear();
    m_bands.clear();
    m_mapBlocks = 0;
    m_generation++;

    AddPartitionMap();

    if (m_bands.empty())
    {
        AddPartitions();
    }

    if (m_bands.empty())
    {
        AddVolume (0, GetBlockCount(), L"");

        if (m_volumes.empty())
        {
            hr = E_OUTOFMEMORY;
        }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::AddVolume
//
//  A volume over a run of blocks, added when its file system is found or
//  when it is the whole image. A band's label is set when it is a partition.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::AddVolume (uint32_t first, uint32_t count, const std::wstring & label)
{
    std::unique_ptr<Volume>  volume  = std::make_unique<Volume>();
    bool                     isWhole = label.empty();



    volume->first = first;
    volume->count = count;
    volume->store = std::make_unique<OffsetBlockStore> (m_file->GetStore(), first, count);
    volume->image = std::make_unique<BlockImage> (*volume->store);

    BlockMapBuilder::Build (*volume->image, volume->map);

    if (volume->map.fileSystem == BlockFileSystem::None && !isWhole)
    {
        return;
    }

    if (!m_bands.empty() && !isWhole)
    {
        m_bands.back().volume = (int) m_volumes.size();
    }

    m_volumes.push_back (std::move (volume));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::AddSectorVolume
//
//  A DOS volume of 32 sectors a track, as an 800K disk's are, or of 16, as
//  DOS Master's are, by its VTOC.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::AddSectorVolume (const Partition & partition)
{
    static constexpr int  kTrackSizes[] = { 32, 16 };



    HRESULT                  hr      = S_OK;
    vector<Byte>             sectors;
    std::unique_ptr<Volume>  volume  = std::make_unique<Volume>();
    int                      tracks  = 0;
    int                      spt     = 0;
    bool                     isFound = false;



    hr = PartitionLayout::ReadPartition (m_file->GetStore(), partition, sectors);

    if (FAILED (hr))
    {
        return;
    }

    for (int size : kTrackSizes)
    {
        isFound = isFound || Dos33SectorLayout::TryReadGeometry (sectors, size, tracks, spt);
    }

    if (!isFound)
    {
        return;
    }

    volume->first           = partition.firstBlock;
    volume->count           = partition.blockCount;
    volume->isSectors       = true;
    volume->half            = partition.span == PartitionSpan::FirstHalves ? 0 : partition.span == PartitionSpan::SecondHalves ? 1 : -1;
    volume->sectorsPerTrack = spt;
    volume->number          = partition.index;

    Dos33SectorLayout::Walk (sectors, tracks, spt, volume->map);

    m_bands.back().volume = (int) m_volumes.size();
    m_volumes.push_back (std::move (volume));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::AddPartitionMap
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::AddPartitionMap()
{
    vector<ApplePartitionMap::Entry>  entries;
    BlockBand                         band;



    if (!ApplePartitionMap::TryRead (m_file->GetStore(), entries))
    {
        return;
    }

    for (const ApplePartitionMap::Entry & entry : entries)
    {
        m_mapBlocks = std::max (m_mapBlocks, entry.mapBlock + 1);
    }

    m_bands.push_back ({ 0, 1, L"Driver descriptor", true, -1 });
    m_bands.push_back ({ 1, m_mapBlocks - 1, L"Partition map", true, -1 });

    for (const ApplePartitionMap::Entry & entry : entries)
    {
        if (entry.type == L"Apple_partition_map" || entry.blockCount == 0)
        {
            continue;
        }

        band       = BlockBand();
        band.first = entry.firstBlock;
        band.count = entry.blockCount;
        band.label = (entry.name.empty() ? entry.type : entry.name) + L" (" + entry.type + L")";

        m_bands.push_back (band);

        if (entry.type != L"Apple_Free")
        {
            AddVolume (entry.firstBlock, entry.blockCount, band.label);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::AddPartitions
//
//  CFFA slices, MicroDrive and FocusDrive partitions, DOS Master's DOS
//  volumes, and the DOS volumes of an 800K UniDOS, AmDOS or OzDOS disk.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::AddPartitions()
{
    HRESULT                 hr      = S_OK;
    PartitionLayoutKind     kind    = PartitionLayoutKind::None;
    vector<PartitionEntry>  entries;
    BlockBand               band;



    hr = PartitionAccess::ListStore (m_file->GetStore(), kind, entries);

    if (FAILED (hr) || kind == PartitionLayoutKind::None)
    {
        return;
    }

    for (const PartitionEntry & entry : entries)
    {
        band       = BlockBand();
        band.first = entry.partition.firstBlock;
        band.count = entry.partition.blockCount;
        band.label = TextEncoding::NarrowToWide (std::string (PartitionLayout::GetLayoutName (kind)) + " " + PartitionAccess::GetLabel (entry, false));

        m_bands.push_back (band);

        if (entry.partition.dosSectors)
        {
            AddSectorVolume (entry.partition);
        }
        else if (entry.partition.span == PartitionSpan::WholeBlocks)
        {
            AddVolume (band.first, band.count, band.label);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::FindVolume
//
////////////////////////////////////////////////////////////////////////////////

int BlockInspectorModel::FindVolume (uint32_t block) const
{
    int  i = 0;



    for (i = 0; i < (int) m_volumes.size(); i++)
    {
        if (!m_volumes[i]->isSectors && block >= m_volumes[i]->first && block - m_volumes[i]->first < m_volumes[i]->count)
        {
            return i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::FindMap
//
////////////////////////////////////////////////////////////////////////////////

const BlockMap * BlockInspectorModel::FindMap (uint32_t block, uint32_t & outInner) const
{
    int  volume = FindVolume (block);



    if (volume < 0)
    {
        return nullptr;
    }

    outInner = block - m_volumes[volume]->first;

    return &m_volumes[volume]->map;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::IsMapBlock
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorModel::IsMapBlock (uint32_t block, bool & outIsDescriptor) const
{
    outIsDescriptor = block == 0;

    return block < m_mapBlocks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::SelectBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::SelectBlock (uint32_t block)
{
    uint32_t  count = GetBlockCount();



    if (count == 0)
    {
        return;
    }

    m_block = std::min (block, count - 1);
    m_history.Record (m_block);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::ReplaceBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::ReplaceBlock (uint32_t block)
{
    uint32_t  count = GetBlockCount();



    if (count == 0)
    {
        return;
    }

    m_block = std::min (block, count - 1);
    m_history.Replace (m_block);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::StepBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::StepBlock (int64_t delta)
{
    int64_t  target = (int64_t) m_block + delta;



    SelectBlock ((uint32_t) std::clamp<int64_t> (target, 0, (int64_t) GetBlockCount() - 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::GoBack
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorModel::GoBack()
{
    return m_history.GoBack (m_block);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::GoForward
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorModel::GoForward()
{
    return m_history.GoForward (m_block);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::ReadStoredBlock
//
//  From the cache, or read from the file and cached.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::ReadStoredBlock (uint32_t block, Block & outBytes)
{
    HRESULT  hr     = S_OK;
    auto     cached = m_stored.find (block);



    outBytes.fill (0);

    if (cached != m_stored.end())
    {
        outBytes = cached->second;
        BAIL_OUT_IF (true, S_OK);
    }

    hr = m_file->GetStore().ReadBlocks (block, 1, outBytes);
    CHR (hr);

    m_stored[block] = outBytes;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::ReadBlock
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::ReadBlock (uint32_t block, Block & outBytes)
{
    HRESULT  hr = S_OK;



    hr = ReadStoredBlock (block, outBytes);
    CHR (hr);

    m_edits.Overlay (block, outBytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::FindPlaces
//
//  Every volume a block is in. An OzDOS block is in two, a sector in each
//  half.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::FindPlaces (uint32_t block, vector<Place> & outPlaces) const
{
    uint32_t  inner = 0;
    int       i     = 0;
    int       half  = 0;



    outPlaces.clear();

    for (i = 0; i < (int) m_volumes.size(); i++)
    {
        const Volume  & volume = *m_volumes[i];

        if (block < volume.first || block - volume.first >= volume.count)
        {
            continue;
        }

        inner = block - volume.first;

        if (!volume.isSectors)
        {
            outPlaces.push_back ({ i, inner, -1 });
        }
        else if (volume.half >= 0)
        {
            outPlaces.push_back ({ i, inner, volume.half });
        }
        else
        {
            for (half = 0; half < 2; half++)
            {
                outPlaces.push_back ({ i, inner * 2 + (uint32_t) half, half });
            }
        }
    }

    std::erase_if (outPlaces, [this] (const Place & place) { return place.unit >= m_volumes[place.volume]->map.GetBlockCount(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::GetUnitBlock
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockInspectorModel::GetUnitBlock (const Volume & volume, uint32_t unit) const
{
    if (!volume.isSectors || volume.half >= 0)
    {
        return volume.first + unit;
    }

    return volume.first + unit / 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::GetRole
//
//  A partition map's descriptor is boot area and its entries a key block,
//  as the structures every other block of the disk is found from. A block
//  of two sectors takes the more notable role.
//
////////////////////////////////////////////////////////////////////////////////

SectorRole BlockInspectorModel::GetRole (uint32_t block) const
{
    thread_local vector<Place>  places;
    bool                        isDescriptor = false;
    SectorRole                  role         = SectorRole::Free;



    if (IsMapBlock (block, isDescriptor))
    {
        return isDescriptor ? SectorRole::BootArea : SectorRole::VtocOrKeyBlock;
    }

    FindPlaces (block, places);

    for (const Place & place : places)
    {
        role = BlockMap::PickRole (role, m_volumes[place.volume]->map.blocks[place.unit].role);
    }

    return role;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::GetMapLabel
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR BlockInspectorModel::GetMapLabel (uint32_t block) const
{
    bool  isDescriptor = false;



    if (!IsMapBlock (block, isDescriptor))
    {
        return nullptr;
    }

    return isDescriptor ? L"Driver descriptor" : L"Partition map";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::GetOwnerKey
//
////////////////////////////////////////////////////////////////////////////////

int64_t BlockInspectorModel::GetOwnerKey (uint32_t block) const
{
    thread_local vector<Place>  places;



    FindPlaces (block, places);

    for (const Place & place : places)
    {
        const vector<int> &  owners = m_volumes[place.volume]->map.blocks[place.unit].owners;

        if (!owners.empty() && owners[0] >= 0)
        {
            return ((int64_t) place.volume << 32) | (uint32_t) owners[0];
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::GetOwners
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockInspectorModel::GetOwners (uint32_t block) const
{
    vector<Place>  places;
    std::wstring   text;
    std::wstring   owners;



    FindPlaces (block, places);

    for (const Place & place : places)
    {
        owners = m_volumes[place.volume]->map.DescribeOwners (place.unit);

        if (owners.empty() || text.find (owners) != std::wstring::npos)
        {
            continue;
        }

        text += (text.empty() ? L"" : L"\n") + owners;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::GetFields
//
//  A field's link is the image's block. A sector's fields are moved to the
//  half of the block the sector is in.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::GetFields (uint32_t block, const Block & bytes, vector<BlockField> & outFields) const
{
    bool                isDescriptor = false;
    vector<Place>       places;
    vector<BlockField>  fields;



    outFields.clear();

    if (IsMapBlock (block, isDescriptor))
    {
        if (isDescriptor)
        {
            ApplePartitionMap::AnnotateDescriptor (bytes, outFields);
        }
        else
        {
            ApplePartitionMap::AnnotateEntry (bytes, GetBlockCount(), outFields);
        }

        return;
    }

    FindPlaces (block, places);

    for (const Place & place : places)
    {
        if (place.half >= 0)
        {
            AnnotateSector (place, bytes, outFields);
            continue;
        }

        BlockMapBuilder::Annotate (m_volumes[place.volume]->map, place.unit, bytes, fields);

        for (BlockField & field : fields)
        {
            if (field.link != BlockField::kNoLink)
            {
                field.link += m_volumes[place.volume]->first;
            }

            outFields.push_back (std::move (field));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::AnnotateSector
//
//  Each label starts with the volume and the sector's track and sector.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::AnnotateSector (const Place & place, const Block & bytes, vector<BlockField> & ioFields) const
{
    const Volume        & volume = *m_volumes[place.volume];
    uint32_t              shift  = (uint32_t) place.half * Dos33SectorLayout::kSectorBytes;
    vector<BlockField>    fields;
    std::wstring          head   = L"Volume " + std::to_wstring (volume.number) + L", T" + std::to_wstring (place.unit / volume.sectorsPerTrack)
                                 + L" S" + std::to_wstring (place.unit % volume.sectorsPerTrack) + L": ";



    Dos33SectorLayout::Annotate (volume.map, place.unit, volume.sectorsPerTrack,
                                 BlockBytes (bytes).subspan (shift, Dos33SectorLayout::kSectorBytes), fields);

    for (BlockField & field : fields)
    {
        field.offset += shift;
        field.label   = head + field.label;

        if (field.pairOffset >= 0)
        {
            field.pairOffset += (int32_t) shift;
        }

        if (field.link != BlockField::kNoLink)
        {
            field.link = GetUnitBlock (volume, (uint32_t) field.link);
        }

        ioFields.push_back (std::move (field));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::TryFollowLink
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorModel::TryFollowLink (int offset)
{
    Block               bytes  = {};
    vector<BlockField>  fields;
    HRESULT             hr     = ReadBlock (m_block, bytes);



    if (FAILED (hr) || offset < 0)
    {
        return false;
    }

    GetFields (m_block, bytes, fields);

    for (const BlockField & field : fields)
    {
        if (field.link != BlockField::kNoLink && field.Contains ((uint32_t) offset) && field.link < (int64_t) GetBlockCount())
        {
            SelectBlock ((uint32_t) field.link);
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::HighlightFileAt
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorModel::HighlightFileAt (uint32_t block)
{
    vector<Place>  places;



    ClearHighlight();
    FindPlaces (block, places);

    for (const Place & place : places)
    {
        const vector<int> &  owners = m_volumes[place.volume]->map.blocks[place.unit].owners;

        if (!owners.empty())
        {
            HighlightFiles (place.volume, { owners[0] });
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::HighlightFiles
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::HighlightFiles (int volume, const vector<int> & files)
{
    ClearHighlight();

    if (volume < 0 || volume >= (int) m_volumes.size() || files.empty())
    {
        return;
    }

    m_highlightVolume = volume;
    m_highlightFile   = files[0];
    m_highlightFiles.assign (m_volumes[volume]->map.files.size(), false);

    for (int file : files)
    {
        if (file >= 0 && (size_t) file < m_highlightFiles.size())
        {
            m_highlightFiles[file] = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::HighlightBlocks
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::HighlightBlocks (const vector<uint32_t> & blocks)
{
    ClearHighlight();

    if (blocks.empty())
    {
        return;
    }

    m_highlightBlocks.assign (GetBlockCount(), false);

    for (uint32_t block : blocks)
    {
        if (block < m_highlightBlocks.size())
        {
            m_highlightBlocks[block] = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::ClearHighlight
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::ClearHighlight()
{
    m_highlightVolume = -1;
    m_highlightFile   = -1;
    m_highlightFiles.clear();
    m_highlightBlocks.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::IsHighlighted
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorModel::IsHighlighted (uint32_t block) const
{
    thread_local vector<Place>  places;



    if (!m_highlightBlocks.empty())
    {
        return block < m_highlightBlocks.size() && m_highlightBlocks[block];
    }

    if (m_highlightFile < 0)
    {
        return false;
    }

    FindPlaces (block, places);

    for (const Place & place : places)
    {
        const vector<int> &  owners = m_volumes[place.volume]->map.blocks[place.unit].owners;

        if (place.volume != m_highlightVolume)
        {
            continue;
        }

        for (int owner : owners)
        {
            if (owner >= 0 && (size_t) owner < m_highlightFiles.size() && m_highlightFiles[owner])
            {
                return true;
            }
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::GetHighlightedPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockInspectorModel::GetHighlightedPath() const
{
    if (m_highlightVolume < 0 || m_highlightFile < 0)
    {
        return {};
    }

    return m_volumes[m_highlightVolume]->map.files[m_highlightFile];
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::TypeHexDigit
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorModel::TypeHexDigit (int offset, bool isHighNibble, wchar_t digit)
{
    Block    base = {};
    HRESULT  hr   = ReadStoredBlock (m_block, base);



    return SUCCEEDED (hr) && m_edits.TypeHexDigit (m_block, base, offset, isHighNibble, digit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::TypeCharacter
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorModel::TypeCharacter (int offset, wchar_t ch)
{
    Block    base = {};
    HRESULT  hr   = ReadStoredBlock (m_block, base);



    return SUCCEEDED (hr) && m_edits.TypeCharacter (m_block, base, offset, ch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::ApplyEdits
//
//  The changed blocks are written, and the blocks before and after go in
//  the history for Undo applied edit. A new apply clears the applied edits
//  that were undone.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::ApplyEdits (std::wstring & outError, BlockJobProgress * progress)
{
    HRESULT           hr      = S_OK;
    vector<uint32_t>  changed = m_edits.GetChangedBlocks();
    AppliedEdit       applied;
    Block             bytes   = {};



    BAIL_OUT_IF (changed.empty(), S_OK);

    for (uint32_t block : changed)
    {
        hr = ReadStoredBlock (block, bytes);
        CHRF (hr, outError = L"A changed block can't be read.");
        applied.before[block] = bytes;

        hr = ReadBlock (block, bytes);
        CHRF (hr, outError = L"A changed block can't be read.");
        applied.after[block] = bytes;
    }

    hr = WriteBlocks (applied.after, outError, progress);
    CHR (hr);

    m_applied.resize (m_appliedAt);
    m_applied.push_back (std::move (applied));
    m_appliedAt = m_applied.size();
    m_edits     = BlockEditBuffer();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::UndoApplied
//
//  The last applied edit's blocks put back as they were before it, when
//  the file has the blocks the apply wrote.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::UndoApplied (std::wstring & outError, BlockJobProgress * progress)
{
    HRESULT  hr        = S_OK;
    bool     isCurrent = false;



    BAIL_OUT_IF (m_appliedAt == 0, S_OK);

    hr = IsStoredAs (m_applied[m_appliedAt - 1].after, isCurrent);
    CHRF (hr, outError = L"A changed block can't be read.");
    CBRF (isCurrent, outError = L"The blocks changed after the edit was applied, so it can't be undone.");

    hr = WriteBlocks (m_applied[m_appliedAt - 1].before, outError, progress);
    CHR (hr);

    m_appliedAt--;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::RedoApplied
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::RedoApplied (std::wstring & outError, BlockJobProgress * progress)
{
    HRESULT  hr        = S_OK;
    bool     isCurrent = false;



    BAIL_OUT_IF (m_appliedAt >= m_applied.size(), S_OK);

    hr = IsStoredAs (m_applied[m_appliedAt].before, isCurrent);
    CHRF (hr, outError = L"A changed block can't be read.");
    CBRF (isCurrent, outError = L"The blocks changed after the edit was undone, so it can't be applied again.");

    hr = WriteBlocks (m_applied[m_appliedAt].after, outError, progress);
    CHR (hr);

    m_appliedAt++;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::IsStoredAs
//
//  Whether the file has each of the blocks as given.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::IsStoredAs (const std::map<uint32_t, Block> & blocks, bool & outIsSame)
{
    HRESULT  hr    = S_OK;
    Block    bytes = {};



    outIsSame = true;

    for (const auto & [block, expected] : blocks)
    {
        hr = ReadStoredBlock (block, bytes);
        CHR (hr);

        outIsSame = outIsSame && bytes == expected;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::UseOtherBlock
//
//  The other image's bytes for a block, as a pending edit of the block.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorModel::UseOtherBlock (uint32_t block)
{
    Block    base  = {};
    Block    other = {};
    HRESULT  hr    = S_OK;
    bool     isSet = false;



    BAIL_OUT_IF (!IsComparing() || block >= GetBlockCount(), S_OK);

    hr = ReadStoredBlock (block, base);
    CHR (hr);

    hr = ReadOtherBlock (block, other);
    CHR (hr);

    isSet = m_edits.SetBlock (block, base, other);

Error:
    return isSet;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::WriteEditsTo
//
//  The pending edits written into a copy of the image at `path`, which
//  the caller made. The image this model opened is left as it is.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::WriteEditsTo (IDiskFileIo & fileIo, const std::string & path, std::wstring & outError)
{
    HRESULT                          hr    = S_OK;
    std::unique_ptr<BlockImageFile>  copy;
    Block                            bytes = {};
    std::wstring                     error;



    hr = BlockImageFile::Open (fileIo, path, copy, error);
    CHRF (hr, outError = error.empty() ? L"The copy can't be opened." : error);

    for (uint32_t block : m_edits.GetChangedBlocks())
    {
        hr = ReadBlock (block, bytes);
        CHRF (hr, outError = L"A changed block can't be read.");

        hr = copy->GetStore().WriteBlocks (block, bytes);
        CHRF (hr, outError = L"The copy can't be written.");
    }

    hr = copy->Commit();
    CHRF (hr, outError = L"The copy can't be written.");

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::WriteBlocks
//
//  The blocks written to the file, which must be as it was when opened or
//  last written, and the maps read again. The cache is cleared after the
//  write, as a card's partition or a Disk Copy image can differ from the
//  blocks written once committed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::WriteBlocks (const std::map<uint32_t, Block> & blocks, std::wstring & outError, BlockJobProgress * progress)
{
    HRESULT  hr        = S_OK;
    bool     isCurrent = m_file->IsFileUnchanged();



    CBRF (isCurrent, outError = L"The file changed after it was opened, so the edits weren't written. Reload it to edit it.");

    for (const auto & [block, bytes] : blocks)
    {
        hr = m_file->GetStore().WriteBlocks (block, bytes);
        CHRF (hr, outError = L"A changed block can't be written.");
    }

    hr = m_file->Commit();
    CHRF (hr, outError = L"The file can't be written.");

    ClearHighlight();
    ClearCache();

    hr = BuildVolumes();
    CHR (hr);

    m_headerFields.clear();

    if (!m_file->GetHeader().empty())
    {
        m_file->AnnotateHeader (m_headerFields);
    }

    if (IsComparing())
    {
        hr = ComputeDifferences (progress);

        if (hr == E_ABORT)
        {
            EndComparison();
            hr = S_OK;
        }

        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::CompareWith
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::CompareWith (std::unique_ptr<BlockImageFile> other, BlockJobProgress * progress)
{
    HRESULT  hr      = S_OK;
    bool     isOther = other != nullptr;



    CBRAEx (isOther, E_INVALIDARG);

    m_other = std::move (other);
    m_storedOther.clear();

    hr = ComputeDifferences (progress);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        EndComparison();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::ComputeDifferences
//
//  Both images are read kRunBlocks at a time. A block past either image's
//  end is compared as zeros. Fails with E_ABORT once canceled.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::ComputeDifferences (BlockJobProgress * progress)
{
    HRESULT   hr         = S_OK;
    uint32_t  count      = std::max (GetBlockCount(), m_other->GetBlockCount());
    uint32_t  first      = 0;
    uint32_t  step       = 0;
    uint32_t  inMine     = 0;
    uint32_t  inOther    = 0;
    uint32_t  i          = 0;
    bool      isCanceled = false;
    vector<Byte>  mine       (kRunBlocks * kBlockBytes);
    vector<Byte>  theirs     (kRunBlocks * kBlockBytes);



    m_differs.assign (count, false);
    m_differCount = 0;

    if (progress != nullptr)
    {
        progress->total = count;
    }

    for (first = 0; first < count; first += step)
    {
        isCanceled = progress != nullptr && progress->IsCanceled();
        CBREx (!isCanceled, E_ABORT);

        if (progress != nullptr)
        {
            progress->done = first;
        }

        step    = std::min (kRunBlocks, count - first);
        inMine  = first < GetBlockCount()           ? std::min (step, GetBlockCount() - first)           : 0;
        inOther = first < m_other->GetBlockCount()  ? std::min (step, m_other->GetBlockCount() - first)  : 0;

        std::fill (mine.begin(),   mine.end(),   Byte (0));
        std::fill (theirs.begin(), theirs.end(), Byte (0));

        if (inMine > 0)
        {
            hr = m_file->GetStore().ReadBlocks (first, inMine, mine);
            CHR (hr);
        }

        if (inOther > 0)
        {
            hr = m_other->GetStore().ReadBlocks (first, inOther, theirs);
            CHR (hr);
        }

        for (i = 0; i < step; i++)
        {
            if (!std::equal (mine.begin() + (ptrdiff_t) i * kBlockBytes, mine.begin() + (ptrdiff_t) (i + 1) * kBlockBytes, theirs.begin() + (ptrdiff_t) i * kBlockBytes))
            {
                m_differs[first + i] = true;
                m_differCount++;
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::EndComparison
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::EndComparison()
{
    m_other.reset();
    m_differs.clear();
    m_storedOther.clear();

    m_differCount = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::ReadOtherBlock
//
//  A block past the other image's end is zeros. From the cache, or read
//  from the other image and cached.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::ReadOtherBlock (uint32_t block, Block & outBytes)
{
    HRESULT  hr      = S_OK;
    bool     isOther = m_other != nullptr;
    bool     isPast  = isOther && block >= m_other->GetBlockCount();
    auto     cached  = m_storedOther.find (block);



    outBytes.fill (0);

    CBRAEx (isOther, E_UNEXPECTED);

    if (cached != m_storedOther.end())
    {
        outBytes = cached->second;
        BAIL_OUT_IF (true, S_OK);
    }

    if (!isPast)
    {
        hr = m_other->GetStore().ReadBlocks (block, 1, outBytes);
        CHR (hr);
    }

    m_storedOther[block] = outBytes;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::FindDifference
//
//  The next or previous block that differs from the current one, wrapping
//  past either end.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorModel::FindDifference (bool isForward, uint32_t & outBlock) const
{
    uint32_t  count = (uint32_t) m_differs.size();
    uint32_t  step  = 0;
    uint32_t  at    = 0;



    for (step = 1; step <= count; step++)
    {
        at = isForward ? (m_block + step) % count : (m_block + count - step % count) % count;

        if (m_differs[at])
        {
            outBlock = at;
            return true;
        }
    }

    return false;
}
