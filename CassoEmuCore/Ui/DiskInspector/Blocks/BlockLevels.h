#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockSection
//
//  A heading and its rows, each a label, a value and, for an item stored in
//  blocks, the image block a click steps to.
//
////////////////////////////////////////////////////////////////////////////////

struct BlockSection
{
    std::wstring        heading;
    vector<BlockField>  rows;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockPartitionRow
//
//  One partition at the Card level: its volume, and the text of each column.
//
////////////////////////////////////////////////////////////////////////////////

struct BlockPartitionRow
{
    int                   volume = -1;
    vector<std::wstring>  cells;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevels
//
//  The text of block mode's Card, Volume and File levels, from the model's
//  maps, with no window. Block numbers in links are the image's, and every
//  number is written by NumberFormat.
//
////////////////////////////////////////////////////////////////////////////////

class BlockLevels
{
public:
    static constexpr size_t  kMaxRunRows   = 256;
    static constexpr size_t  kMaxIndexRows = 64;

    struct Run
    {
        uint32_t  first = 0;
        uint32_t  count = 0;
    };

    //  The volume's header, its structures, the space used and free, its
    //  findings, and the image's container header.
    static vector<BlockSection>  BuildVolume (const BlockInspectorModel & model, int volume);

    //  A file's or a directory's entry, its index blocks and its runs of
    //  data or directory blocks.
    static vector<BlockSection>  BuildFile (const BlockInspectorModel & model, int volume, int file);

    //  A row for each partition with a volume of blocks.
    static vector<BlockPartitionRow>  BuildCard      (const BlockInspectorModel & model);
    static vector<std::wstring>       GetCardColumns ();

    //  The directory and every file and directory under it.
    static vector<int>  GetDescendants (const BlockMap & map, int directory);

    //  Runs of blocks side by side, from an ascending list.
    static vector<Run>  FindRuns (const vector<uint32_t> & blocks);

    //  "Block 2", "Blocks 2 to 5", or "12 blocks in 3 runs".
    static std::wstring  DescribeBlocks (const vector<uint32_t> & blocks);

    static std::wstring  GetFileSystemName (BlockFileSystem fileSystem);
    static std::wstring  GetStorageName    (const BlockFileEntry & entry);
    static std::wstring  GetVolumeName     (const BlockInspectorModel & model, int volume);

private:
    static LPCWSTR       GetStructureName (BlockStructure structure);
    static BlockField    MakeRow          (const std::wstring & label, const std::wstring & value, int64_t link = BlockField::kNoLink);
    static std::wstring  DescribeShare    (size_t count, size_t total);
};
