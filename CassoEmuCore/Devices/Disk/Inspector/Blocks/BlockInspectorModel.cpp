#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/Blocks/ApplePartitionMap.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/Blocks/BlockMapBuilder.h"
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
    m_edits.DiscardAll();
    ClearHighlight();

    hr = BuildVolumes();
    CHR (hr);

    m_block = 0;
    m_history.Record (m_block);

Error:
    return hr;
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
//  CFFA slices, MicroDrive and FocusDrive partitions, and the DOS volumes of
//  an 800K disk. A partition of DOS sectors is a band with no volume.
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

        if (entry.partition.span == PartitionSpan::WholeBlocks && !entry.partition.dosSectors)
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
        if (block >= m_volumes[i]->first && block - m_volumes[i]->first < m_volumes[i]->count)
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
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::ReadStoredBlock (uint32_t block, Block & outBytes)
{
    HRESULT  hr = S_OK;



    outBytes.fill (0);

    hr = m_file->GetStore().ReadBlocks (block, 1, outBytes);
    CHR (hr);

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
//  BlockInspectorModel::GetRole
//
//  A partition map's descriptor is boot area and its entries a key block,
//  as the structures every other block of the disk is found from.
//
////////////////////////////////////////////////////////////////////////////////

SectorRole BlockInspectorModel::GetRole (uint32_t block) const
{
    bool              isDescriptor = false;
    uint32_t          inner        = 0;
    const BlockMap  * map          = nullptr;



    if (IsMapBlock (block, isDescriptor))
    {
        return isDescriptor ? SectorRole::BootArea : SectorRole::VtocOrKeyBlock;
    }

    map = FindMap (block, inner);

    return map != nullptr ? map->blocks[inner].role : SectorRole::Free;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::GetOwners
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockInspectorModel::GetOwners (uint32_t block) const
{
    uint32_t          inner = 0;
    const BlockMap  * map   = FindMap (block, inner);



    return map != nullptr ? map->DescribeOwners (inner) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::GetFields
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::GetFields (uint32_t block, const Block & bytes, vector<BlockField> & outFields) const
{
    bool              isDescriptor = false;
    uint32_t          inner        = 0;
    const BlockMap  * map          = nullptr;
    int               volume       = FindVolume (block);



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

    map = FindMap (block, inner);

    if (map == nullptr)
    {
        return;
    }

    BlockMapBuilder::Annotate (*map, inner, bytes, outFields);

    for (BlockField & field : outFields)
    {
        if (field.link != BlockField::kNoLink)
        {
            field.link += m_volumes[volume]->first;
        }
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
    uint32_t  inner  = 0;
    int       volume = FindVolume (block);



    ClearHighlight();

    if (volume < 0)
    {
        return false;
    }

    inner = block - m_volumes[volume]->first;

    if (m_volumes[volume]->map.blocks[inner].owners.empty())
    {
        return false;
    }

    m_highlightVolume = volume;
    m_highlightFile   = m_volumes[volume]->map.blocks[inner].owners[0];

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::IsHighlighted
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorModel::IsHighlighted (uint32_t block) const
{
    int                   volume = m_highlightFile >= 0 ? FindVolume (block) : -1;
    const vector<int>   * owners = nullptr;



    if (volume < 0 || volume != m_highlightVolume)
    {
        return false;
    }

    owners = &m_volumes[volume]->map.blocks[block - m_volumes[volume]->first].owners;

    return std::find (owners->begin(), owners->end(), m_highlightFile) != owners->end();
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
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::ApplyEdits (std::wstring & outError)
{
    HRESULT           hr        = S_OK;
    vector<uint32_t>  changed   = m_edits.GetChangedBlocks();
    bool              isCurrent = false;
    bool              isChanged = !changed.empty();
    Block             bytes     = {};



    BAIL_OUT_IF (!isChanged, S_OK);

    isCurrent = m_file->IsFileUnchanged();
    CBRF (isCurrent, outError = L"The file changed after it was opened, so the edits weren't written. Reload it to edit it.");

    for (uint32_t block : changed)
    {
        hr = ReadBlock (block, bytes);
        CHRF (hr, outError = L"A changed block can't be read.");

        hr = m_file->GetStore().WriteBlocks (block, bytes);
        CHRF (hr, outError = L"A changed block can't be written.");
    }

    hr = m_file->Commit();
    CHRF (hr, outError = L"The file can't be written.");

    m_edits.DiscardAll();
    ClearHighlight();

    hr = BuildVolumes();
    CHR (hr);

    if (IsComparing())
    {
        hr = ComputeDifferences();
        CHR (hr);
    }

    if (m_hasLeftovers)
    {
        FindLeftovers();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::CompareWith
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::CompareWith (std::unique_ptr<BlockImageFile> other)
{
    HRESULT  hr      = S_OK;
    bool     isOther = other != nullptr;



    CBRAEx (isOther, E_INVALIDARG);

    m_other = std::move (other);

    hr = ComputeDifferences();
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
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::ComputeDifferences()
{
    HRESULT   hr     = S_OK;
    uint32_t  count  = std::max (GetBlockCount(), m_other->GetBlockCount());
    uint32_t  block  = 0;
    Block     mine   = {};
    Block     theirs = {};



    m_differs.assign (count, false);
    m_differCount = 0;

    for (block = 0; block < count; block++)
    {
        mine.fill (0);
        theirs.fill (0);

        if (block < GetBlockCount())
        {
            hr = ReadStoredBlock (block, mine);
            CHR (hr);
        }

        if (block < m_other->GetBlockCount())
        {
            hr = ReadOtherBlock (block, theirs);
            CHR (hr);
        }

        if (mine != theirs)
        {
            m_differs[block] = true;
            m_differCount++;
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

    m_differCount = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::ReadOtherBlock
//
//  A block past the other image's end is zeros.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::ReadOtherBlock (uint32_t block, Block & outBytes)
{
    HRESULT  hr      = S_OK;
    bool     isOther = m_other != nullptr;
    bool     isPast  = isOther && block >= m_other->GetBlockCount();



    outBytes.fill (0);

    CBRAEx (isOther, E_UNEXPECTED);
    BAIL_OUT_IF (isPast, S_OK);

    hr = m_other->GetStore().ReadBlocks (block, 1, outBytes);
    CHR (hr);

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





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::FindLeftovers
//
//  Each free block's bytes as the file has them.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorModel::FindLeftovers()
{
    vector<LeftoverKind>  kinds (GetBlockCount(), LeftoverKind::None);
    uint32_t              block = 0;
    Block                 bytes = {};
    HRESULT               hr    = S_OK;



    for (block = 0; block < GetBlockCount(); block++)
    {
        if (GetRole (block) != SectorRole::Free)
        {
            continue;
        }

        hr = ReadStoredBlock (block, bytes);

        if (SUCCEEDED (hr))
        {
            kinds[block] = BlockLeftovers::Classify (bytes);
        }
    }

    m_leftovers    = BlockLeftovers::FindRuns (kinds);
    m_hasLeftovers = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::FindLeftoverRun
//
////////////////////////////////////////////////////////////////////////////////

const LeftoverRun * BlockInspectorModel::FindLeftoverRun (uint32_t block) const
{
    for (const LeftoverRun & run : m_leftovers)
    {
        if (run.Contains (block))
        {
            return &run;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::ReadRun
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::ReadRun (const LeftoverRun & run, vector<Byte> & outBytes)
{
    HRESULT   hr     = S_OK;
    bool      inside = (uint64_t) run.first + run.count <= GetBlockCount();



    outBytes.clear();

    CBRAEx (inside, E_INVALIDARG);

    outBytes.resize ((size_t) run.count * kBlockBytes);

    hr = m_file->GetStore().ReadBlocks (run.first, run.count, outBytes);
    CHR (hr);

Error:
    return hr;
}