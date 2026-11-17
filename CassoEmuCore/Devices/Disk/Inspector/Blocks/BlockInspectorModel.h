#pragma once

#include "Pch.h"

#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/Inspector/Blocks/BlockEditBuffer.h"
#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"
#include "Devices/Disk/Inspector/Blocks/BlockHistory.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorWorker.h"
#include "Devices/Disk/Inspector/Blocks/BlockMap.h"
#include "Devices/Disk/OffsetBlockStore.h"

struct Partition;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBand
//
//  A run of an image's blocks drawn as a band over the block map: a
//  partition, or the blocks of a partition map.
//
////////////////////////////////////////////////////////////////////////////////

struct BlockBand
{
    uint32_t      first    = 0;
    uint32_t      count    = 0;
    std::wstring  label;
    bool          isMap    = false;
    int           volume   = -1;

    bool  Contains (uint32_t block) const { return block >= first && block - first < count; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel
//
//  The disk inspector's block mode over one image file: the block the user
//  is on, Back and Forward, the block map with each block's role, files and
//  fields, the partitions as bands, the file whose blocks are highlighted,
//  and the pending edits. A partitioned image has a volume for each
//  partition with a file system of blocks, mapped over the image's blocks;
//  an image with no partitions is one volume. Edits are applied to the file
//  in place, only when it has not changed since it was opened, and the map
//  is read again after. Pure apart from the file's reads and writes.
//
//  Each block read is cached, so a block loaded by LoadBlock is read from
//  memory after. Open, CompareWith, ApplyEdits and LoadBlock read or write
//  the file, and a window calls them on its BlockInspectorWorker's thread. Every other method is pure once the
//  current block is loaded.
//
////////////////////////////////////////////////////////////////////////////////

class BlockInspectorModel
{
public:
    static constexpr size_t  kBlockBytes = 512;

    using Block = BlockEditBuffer::Block;
    using Tags  = std::array<Byte, BlockImageFile::kTagBytesPerBlock>;

    BlockInspectorModel  ();
    ~BlockInspectorModel ();

    HRESULT  Open (std::unique_ptr<BlockImageFile> file);

    bool              IsOpen        () const { return m_file != nullptr; }

    //  Counts each time the volumes' maps are read, as when an image opens
    //  or edits are applied; zero before an image opens.
    uint32_t          GetMapGeneration () const { return m_generation; }
    BlockImageFile  & GetFile          () { return *m_file; }
    uint32_t          GetBlockCount    () const;
    uint32_t          GetBlock         () const { return m_block; }

    //  A block's bytes, its Disk Copy tag bytes and, while comparing, the
    //  other image's block, read and cached. IsLoaded is true once they are.
    void  LoadBlock (uint32_t block);
    bool  IsLoaded  (uint32_t block) const;

    //  A loaded block's Disk Copy tag bytes. False for an image without tag
    //  bytes.
    bool  TryGetTags (uint32_t block, Tags & outTags) const;

    //  The container header's fields, read when the file is opened and after
    //  each write.
    const vector<BlockField> &  GetHeaderFields () const { return m_headerFields; }

    //  Steps to a block, clamped to the image, and adds it to the history for
    //  Back and Forward. ReplaceBlock steps in place of the current entry, as
    //  while a block number is typed.
    void  SelectBlock  (uint32_t block);
    void  ReplaceBlock (uint32_t block);
    void  StepBlock    (int64_t delta);
    bool  GoBack       ();
    bool  GoForward    ();
    bool  CanGoBack    () const { return m_history.CanGoBack(); }
    bool  CanGoForward () const { return m_history.CanGoForward(); }

    //  A block's bytes with its pending edits, and as the file has them.
    HRESULT  ReadBlock       (uint32_t block, Block & outBytes);
    HRESULT  ReadStoredBlock (uint32_t block, Block & outBytes);

    //  A block's fields, with links in the image's block numbers.
    void          GetFields (uint32_t block, const Block & bytes, vector<BlockField> & outFields) const;
    SectorRole    GetRole   (uint32_t block) const;
    std::wstring  GetOwners (uint32_t block) const;

    //  A number for the first file that uses a block, the same for every
    //  block of the file and different for each file on the image, or -1.
    int64_t       GetOwnerKey (uint32_t block) const;

    //  "Driver descriptor" or "Partition map" for a block of an Apple
    //  Partition Map, or null for any other block.
    LPCWSTR  GetMapLabel (uint32_t block) const;

    //  The field a link at `offset` of the current block leads to, stepped
    //  to. False when no field there links anywhere.
    bool  TryFollowLink (int offset);

    //  The blocks highlighted on the grid: the first file of a block, a set
    //  of files of a volume, such as a directory and the files under it, or
    //  a set of the image's blocks, such as a structure's. The path is the
    //  first file's.
    bool          HighlightFileAt    (uint32_t block);
    void          HighlightFiles     (int volume, const vector<int> & files);
    void          HighlightBlocks    (const vector<uint32_t> & blocks);
    void          ClearHighlight     ();
    bool          IsHighlighted      (uint32_t block) const;
    bool          HasHighlight       () const { return m_highlightFile >= 0 || !m_highlightBlocks.empty(); }
    std::wstring  GetHighlightedPath () const;

    const vector<BlockBand> &  GetBands () const { return m_bands; }

    //  The volumes of blocks: a partition's, or the image's when it has no
    //  partitions. Each has its first block in the image, its count of
    //  blocks and its map. FindVolume is -1 for a block in no volume of
    //  blocks.
    int               GetVolumeCount () const           { return (int) m_volumes.size(); }
    uint32_t          GetVolumeFirst (int volume) const { return m_volumes[volume]->first; }
    uint32_t          GetVolumeSize  (int volume) const { return m_volumes[volume]->count; }
    bool              IsSectorVolume (int volume) const { return m_volumes[volume]->isSectors; }
    const BlockMap  & GetVolumeMap   (int volume) const { return m_volumes[volume]->map; }
    int               FindVolume     (uint32_t block) const;

    //  The map of the volume a block is in, with the block's number in it.
    const BlockMap *  FindMap (uint32_t block, uint32_t & outInner) const;

    //  Typing into the current block.
    bool                     TypeHexDigit  (int offset, bool isHighNibble, wchar_t digit);
    bool                     TypeCharacter (int offset, wchar_t ch);
    int                      Paste         (int offset, const std::wstring & text, bool isTextColumn, int & outOverflow);
    bool                     Undo          ()                 { return m_edits.Undo(); }
    bool                     Redo          ()                 { return m_edits.Redo(); }
    void                     DiscardEdits  ()                 { m_edits.DiscardAll(); }
    void                     ClearEdits    ()                 { m_edits = BlockEditBuffer(); }
    void                     DiscardBlock  (uint32_t block)   { m_edits.DiscardBlock (block); }
    const BlockEditBuffer  & GetEdits      () const           { return m_edits; }

    //  The other image's bytes for a block, as a pending edit. False when
    //  not comparing or when the block has those bytes.
    bool  UseOtherBlock (uint32_t block);

    //  Writes each changed block and reads the map again. Fails, writing
    //  nothing, when the file changed since it was opened. UndoApplied puts
    //  back the blocks the last save changed and makes its edits unsaved
    //  again, and RedoApplied saves those edits again; each fails when the
    //  file's blocks changed since. A save can be undone only with no unsaved
    //  changes, and redone only while the unsaved changes are the ones the
    //  undo restored.
    HRESULT  ApplyEdits     (std::wstring & outError, BlockJobProgress * progress = nullptr);
    HRESULT  UndoApplied    (std::wstring & outError, BlockJobProgress * progress = nullptr);
    HRESULT  RedoApplied    (std::wstring & outError, BlockJobProgress * progress = nullptr);
    bool     CanUndoApplied () const { return m_appliedAt > 0 && m_edits.IsEmpty(); }
    bool     CanRedoApplied () const { return m_appliedAt < m_applied.size() && m_edits.HasSameChanges (m_applied[m_appliedAt].edits); }

    //  The pending edits written into a copy of the image that the caller
    //  made at `path`.
    HRESULT  WriteEditsTo (IDiskFileIo & fileIo, const std::string & path, std::wstring & outError);

    //  Another image compared block by block with this one: each block
    //  whose bytes differ, a block past either image's end differing when it
    //  isn't all zeros. A comparison canceled through `progress` ends with
    //  E_ABORT.
    HRESULT           CompareWith    (std::unique_ptr<BlockImageFile> other, BlockJobProgress * progress = nullptr);
    void              EndComparison  ();
    bool              IsComparing    () const { return m_other != nullptr; }
    bool              IsDifferent    (uint32_t block) const { return block < m_differs.size() && m_differs[block]; }
    size_t            CountDifferent () const { return m_differCount; }
    HRESULT           ReadOtherBlock (uint32_t block, Block & outBytes);
    bool              FindDifference (bool isForward, uint32_t & outBlock) const;
    BlockImageFile  * GetOther       () const { return m_other.get(); }

private:
    static constexpr uint32_t  kRunBlocks = 256;     // blocks read at once while comparing

    //  A volume of blocks, or of DOS sectors in track and sector order: two
    //  to a block, or one in the first or second half of each block, by
    //  `half`. A sector volume's map units are its sectors.
    struct Volume
    {
        uint32_t                           first           = 0;
        uint32_t                           count           = 0;
        std::unique_ptr<OffsetBlockStore>  store;
        std::unique_ptr<BlockImage>        image;
        BlockMap                           map;
        bool                               isSectors       = false;
        int                                half            = -1;
        int                                sectorsPerTrack = 0;
        int                                number          = 0;
    };

    //  Where a block is in a volume: its unit, and for a sector volume the
    //  half of the block the sector is in, or -1 for a whole block.
    struct Place
    {
        int       volume = -1;
        uint32_t  unit   = 0;
        int       half   = -1;
    };

    //  One save's blocks as they were before it and as it wrote them, and
    //  the unsaved changes it saved.
    struct AppliedEdit
    {
        std::map<uint32_t, Block>  before;
        std::map<uint32_t, Block>  after;
        BlockEditBuffer            edits;
    };

    HRESULT  WriteBlocks        (const std::map<uint32_t, Block> & blocks, std::wstring & outError, BlockJobProgress * progress);
    HRESULT  IsStoredAs         (const std::map<uint32_t, Block> & blocks, bool & outIsSame);
    HRESULT  BuildVolumes       ();
    void     AddVolume          (uint32_t first, uint32_t count, const std::wstring & label);
    void     AddSectorVolume    (const Partition & partition);
    void     AddPartitionMap    ();
    void     AddPartitions      ();
    void     FindPlaces         (uint32_t block, vector<Place> & outPlaces) const;
    uint32_t GetUnitBlock       (const Volume & volume, uint32_t unit) const;
    void     AnnotateSector     (const Place & place, const Block & bytes, vector<BlockField> & ioFields) const;
    bool     IsMapBlock         (uint32_t block, bool & outIsDescriptor) const;
    HRESULT  ComputeDifferences (BlockJobProgress * progress);
    void     ClearCache         ();

    std::unique_ptr<BlockImageFile>  m_file;
    vector<std::unique_ptr<Volume>>  m_volumes;
    vector<BlockBand>                m_bands;
    BlockHistory                     m_history;
    BlockEditBuffer                  m_edits;
    vector<AppliedEdit>              m_applied;
    size_t                           m_appliedAt       = 0;
    uint32_t                         m_block           = 0;
    uint32_t                         m_generation      = 0;
    int                              m_highlightVolume = -1;
    int                              m_highlightFile   = -1;
    vector<bool>                     m_highlightFiles;
    vector<bool>                     m_highlightBlocks;
    uint32_t                         m_mapBlocks       = 0;
    std::unique_ptr<BlockImageFile>  m_other;
    vector<bool>                     m_differs;
    size_t                           m_differCount     = 0;
    std::map<uint32_t, Block>        m_stored;
    std::map<uint32_t, Block>        m_storedOther;
    std::map<uint32_t, Tags>         m_tags;
    std::set<uint32_t>               m_untagged;
    vector<BlockField>               m_headerFields;
};
