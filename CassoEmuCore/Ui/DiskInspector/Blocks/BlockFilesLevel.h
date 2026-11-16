#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel
//
//  Block mode's Files level: a search box across the top, a tree of the
//  volume's directories on the left and a list of the selected directory's
//  entries on the right, as Casso Explorer's tree and list. Text in the
//  search box lists every entry of the volume whose name has it, with its
//  path. Selecting a directory or an entry reports it, so its blocks are
//  highlighted; opening a directory selects it in the tree, and opening a
//  file reports it, for the File level. A column's heading sorts the list.
//
////////////////////////////////////////////////////////////////////////////////

class BlockFilesLevel : public DxuiPanel
{
public:
    static constexpr int    kSearchDip = 32;
    static constexpr int    kGapDip    = 8;
    static constexpr float  kTreeShare = 0.26f;

    enum Column { kName, kType, kAux, kBlocks, kEof, kStorage, kKey, kModified, kColumnCount };

    using EntryFn = std::function<void (int entry)>;

    void  Init (const IDxuiTheme * theme, HWND hwnd);

    //  The volume's entries, with `first`, the image block the volume starts
    //  at, for the Key block column. The tree opens at the volume directory.
    void  SetVolume (const BlockMap * map, uint32_t first);

    //  Selects the directory in the tree, -1 for the volume directory, and
    //  lists its entries.
    void  ShowDirectory (int directory);

    //  Selects the entry's row, listing its directory first.
    void  SelectEntry (int entry);

    void  SetSearch (const std::wstring & text);

    void  SetOnSelect (EntryFn fn) { m_onSelect = std::move (fn); }
    void  SetOnOpen   (EntryFn fn) { m_onOpen   = std::move (fn); }

    //  A directory picked in the tree or opened from the list.
    void  SetOnDirectory (EntryFn fn) { m_onDirectory = std::move (fn); }

    int                  GetDirectory  () const { return m_directory; }
    const vector<int>  & GetRowEntries () const { return m_rows; }
    DxuiListView       & GetList       () { return *m_list; }
    DxuiTreeView       & GetTree       () { return *m_tree; }

    //  The text of each column for an entry of `blocks` blocks, with its
    //  path in place of its name for a search.
    static vector<std::wstring>  BuildCells (const BlockMap & map, int entry, uint32_t first, bool isPath, uint32_t blocks);

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    static std::wstring  MakeId (int directory) { return std::to_wstring (directory); }

    vector<DxuiTreeNode>  BuildNodes     (int directory) const;
    void                  FillRows       ();
    void                  SortRows       ();
    void                  OnRowSelected  (int row);
    void                  OnRowActivated (int row);

    const BlockMap       * m_map          = nullptr;
    uint32_t               m_first        = 0;
    DxuiSearchBox        * m_search       = nullptr;
    DxuiTreeView         * m_tree         = nullptr;
    DxuiListView         * m_list         = nullptr;
    EntryFn                m_onSelect;
    EntryFn                m_onOpen;
    EntryFn                m_onDirectory;
    vector<int>            m_rows;
    vector<vector<int>>    m_children;   // each directory's entries, the volume directory's first
    vector<uint32_t>       m_blockCounts;
    int                    m_directory    = -1;
    std::wstring           m_query;
    int                    m_sortColumn   = kName;
    bool                   m_isDescending = false;
};
