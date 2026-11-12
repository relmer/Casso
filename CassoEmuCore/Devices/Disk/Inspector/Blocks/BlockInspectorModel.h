#pragma once

#include "Pch.h"

#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/Inspector/Blocks/BlockEditBuffer.h"
#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"
#include "Devices/Disk/Inspector/Blocks/BlockHistory.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Devices/Disk/Inspector/Blocks/BlockMap.h"
#include "Devices/Disk/OffsetBlockStore.h"





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
////////////////////////////////////////////////////////////////////////////////

class BlockInspectorModel
{
public:
    static constexpr size_t  kBlockBytes = 512;

    using Block = BlockEditBuffer::Block;

    BlockInspectorModel  ();
    ~BlockInspectorModel ();

    HRESULT  Open (std::unique_ptr<BlockImageFile> file);

    BlockImageFile  & GetFile       () { return *m_file; }
    uint32_t          GetBlockCount () const;
    uint32_t          GetBlock      () const { return m_block; }

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

    //  The field a link at `offset` of the current block leads to, stepped
    //  to. False when no field there links anywhere.
    bool  TryFollowLink (int offset);

    //  The file whose blocks are highlighted: the first file of a block.
    bool          HighlightFileAt    (uint32_t block);
    void          ClearHighlight     () { m_highlightVolume = -1; m_highlightFile = -1; }
    bool          IsHighlighted      (uint32_t block) const;
    bool          HasHighlight       () const { return m_highlightFile >= 0; }
    std::wstring  GetHighlightedPath () const;

    const vector<BlockBand> &  GetBands () const { return m_bands; }

    //  The map of the volume a block is in, with the block's number in it.
    const BlockMap *  FindMap (uint32_t block, uint32_t & outInner) const;

    //  Typing into the current block.
    bool                     TypeHexDigit  (int offset, bool isHighNibble, wchar_t digit);
    bool                     TypeCharacter (int offset, wchar_t ch);
    bool                     Undo          ()           { return m_edits.Undo(); }
    bool                     Redo          ()           { return m_edits.Redo(); }
    void                     DiscardEdits  ()   { m_edits.DiscardAll(); }
    const BlockEditBuffer  & GetEdits      () const { return m_edits; }

    //  Writes each changed block and reads the map again. Fails, writing
    //  nothing, when the file changed since it was opened.
    HRESULT  ApplyEdits (std::wstring & outError);

private:
    struct Volume
    {
        uint32_t                           first = 0;
        uint32_t                           count = 0;
        std::unique_ptr<OffsetBlockStore>  store;
        std::unique_ptr<BlockImage>        image;
        BlockMap                           map;
    };

    HRESULT  BuildVolumes    ();
    void     AddVolume       (uint32_t first, uint32_t count, const std::wstring & label);
    void     AddPartitionMap ();
    void     AddPartitions   ();
    int      FindVolume      (uint32_t block) const;
    bool     IsMapBlock      (uint32_t block, bool & outIsDescriptor) const;

    std::unique_ptr<BlockImageFile>  m_file;
    vector<std::unique_ptr<Volume>>  m_volumes;
    vector<BlockBand>                m_bands;
    BlockHistory                     m_history;
    BlockEditBuffer                  m_edits;
    uint32_t                         m_block           = 0;
    int                              m_highlightVolume = -1;
    int                              m_highlightFile   = -1;
    uint32_t                         m_mapBlocks       = 0;
};
