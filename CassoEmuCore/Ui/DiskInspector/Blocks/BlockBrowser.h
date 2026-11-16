#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockPlace
//
//  A place in block mode: the card, a volume, a directory, a file whose
//  blocks are listed, or a block in the hex view. `entry` is the directory
//  or the file, -1 for a volume's directory.
//
////////////////////////////////////////////////////////////////////////////////

struct BlockPlace
{
    enum class Kind { Card, Volume, Directory, File, Block };

    Kind      kind   = Kind::Block;
    int       volume = -1;
    int       entry  = -1;
    uint32_t  block  = 0;

    bool operator== (const BlockPlace & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockListRow / BlockListGroup / BlockListing
//
//  The list's rows, in groups, for a place: each row's cells, what it is,
//  and the blocks it covers, `count` from `first`, or each of `blocks` for
//  blocks that aren't side by side. A file's or a directory's row covers
//  the entry's blocks.
//
////////////////////////////////////////////////////////////////////////////////

struct BlockListRow
{
    enum class Kind { Partition, Structure, Finding, Directory, File, Blocks };

    Kind                  kind   = Kind::File;
    vector<std::wstring>  cells;
    int                   volume = -1;
    int                   entry  = -1;
    uint32_t              first  = 0;
    uint32_t              count  = 0;
    vector<uint32_t>      blocks;
};


struct BlockListGroup
{
    std::wstring          title;
    vector<BlockListRow>  rows;
};


struct BlockListing
{
    vector<std::wstring>    columns;
    vector<bool>            isNumber;      // a column of numbers, aligned right and sorted by value
    vector<BlockListGroup>  groups;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBrowser
//
//  Block mode's tree, list and breadcrumb, from the model's maps, and the
//  places gone to, for Back and Forward. Block numbers are the image's, and
//  every number is written by NumberFormat. Pure apart from reading the
//  model.
//
////////////////////////////////////////////////////////////////////////////////

class BlockBrowser
{
public:
    enum EntryColumn { kName, kType, kAux, kBlocks, kEof, kStorage, kBlock, kModified, kEntryColumns };

    struct Run
    {
        uint32_t  first = 0;
        uint32_t  count = 0;
    };

    explicit BlockBrowser (const BlockInspectorModel * model = nullptr) : m_model (model) {}

    void  SetModel (const BlockInspectorModel * model) { m_model = model; }

    //  The places: Start opens the history at the card for an image with
    //  partitions, a volume's place for a volume, or block 0 for an image
    //  with no file system of blocks. GoTo puts the current place in the
    //  Back history; Replace doesn't, as a step to the next block doesn't.
    void  Start        ();
    void  GoTo         (const BlockPlace & place);
    void  Replace      (const BlockPlace & place);
    bool  GoBack       ();
    bool  GoForward    ();
    bool  CanGoBack    () const { return !m_back.empty(); }
    bool  CanGoForward () const { return !m_forward.empty(); }

    const BlockPlace  & GetPlace () const { return m_place; }

    //  The Block place at a block, with its volume.
    BlockPlace  MakeBlockPlace (uint32_t block) const;

    //  The tree's place for a place: a file's directory, a block's file's
    //  directory, or else its volume.
    BlockPlace  GetContainer (const BlockPlace & place) const;

    bool  HasCard () const;

    //  The tree: the card and its partitions for an image with partitions,
    //  or the volume, each with its directories below it. A node's id is
    //  its place's.
    vector<DxuiTreeNode>  BuildTree (const std::wstring & cardLabel) const;

    static std::wstring  MakeId  (const BlockPlace & place);
    static bool          ParseId (const std::wstring & id, BlockPlace & outPlace);

    //  The list for a place: a card's partitions; a volume's structures,
    //  findings, directories and files; a directory's blocks, then its
    //  directories and files; a file's index blocks and runs of data; or
    //  with `query`, each entry of the volume or the card whose name has it,
    //  without case.
    BlockListing  BuildListing (const BlockPlace & place, const std::wstring & query = std::wstring()) const;

    //  The breadcrumb's segments for a place, and where each goes.
    vector<std::wstring>  BuildCrumbs (const BlockPlace & place, const std::wstring & cardLabel, vector<BlockPlace> & outTargets) const;

    //  The address bar's path for a place, as "/VOLUME/DIR/FILE", with
    //  "/Block 1,234" after it for a block, and the place a typed path
    //  leads to, matched without case. A path may start at the volume or at
    //  the card, and may be a block's number alone.
    std::wstring  FormatPath (const BlockPlace & place) const;
    bool          FindPath   (const std::wstring & path, const std::wstring & cardLabel, BlockPlace & outPlace) const;

    //  What an address bar separator's menu lists: a card's volumes, or a
    //  volume's or a directory's directories, with their places.
    vector<std::pair<std::wstring, BlockPlace>>  GetChildPlaces (const BlockPlace & place) const;

    //  The row of a listing that covers a block, as an index in the order
    //  of its groups, or -1.
    int  FindRow (const BlockListing & listing, uint32_t block) const;

    //  The image's blocks a row covers, for its highlight on the map.
    static vector<uint32_t>  GetRowBlocks (const BlockListRow & row);

    //  Pure helpers.
    static vector<int>   GetDescendants    (const BlockMap & map, int directory);
    static vector<Run>   FindRuns          (const vector<uint32_t> & blocks);
    static std::wstring  DescribeBlocks    (const vector<uint32_t> & blocks);
    static std::wstring  GetFileSystemName (BlockFileSystem fileSystem);
    static std::wstring  GetStorageName    (const BlockFileEntry & entry);
    std::wstring         GetVolumeName     (int volume) const;

private:
    static constexpr size_t  kMaxRunRows   = 256;
    static constexpr size_t  kMaxIndexRows = 64;

    void                  AddCardRows         (BlockListing & listing) const;
    void                  AddVolumeRows       (BlockListing & listing, int volume) const;
    void                  AddEntryRows        (BlockListing & listing, int volume, int directory) const;
    void                  AddFileRows         (BlockListing & listing, int volume, int file) const;
    void                  AddSearchRows       (BlockListing & listing, int volume, const std::wstring & query) const;
    BlockListRow          MakeEntryRow        (int volume, int entry, uint32_t blocks, bool isPath) const;
    BlockListRow          MakeBlocksRow       (BlockListRow::Kind kind, int volume, const std::wstring & name, const vector<uint32_t> & blocks) const;
    vector<DxuiTreeNode>  BuildDirectoryNodes (int volume, int directory) const;
    vector<int>           GetVolumes          () const;
    static void           SetEntryColumns     (BlockListing & listing);
    static LPCWSTR        GetStructureName    (BlockStructure structure, uint32_t tree);

    const BlockInspectorModel  * m_model = nullptr;
    BlockPlace                   m_place;
    vector<BlockPlace>           m_back;
    vector<BlockPlace>           m_forward;
};
