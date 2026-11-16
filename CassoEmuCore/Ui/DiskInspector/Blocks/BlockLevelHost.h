#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"

class BlockFilesLevel;
class BlockSectionsView;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost
//
//  Block mode's levels over its right column: Card for an image with more
//  than one partition, Volume, Files, File and Block. A breadcrumb across
//  the top has each level from the card or the volume down to the current
//  one, and a click on a segment goes to it. Back and Forward step through
//  the locations gone to. The Block level is drawn by the panel under the
//  breadcrumb; every other level fills the column. A click on an item
//  stored in blocks reports the block, for the Block level.
//
//  Selecting a partition, a directory or a file highlights its blocks on
//  the grid through the model.
//
////////////////////////////////////////////////////////////////////////////////

class BlockLevelHost : public DxuiPanel
{
public:
    static constexpr int  kCrumbDip  = 34;
    static constexpr int  kBrowseDip = 28;
    static constexpr int  kGapDip    = 8;

    enum class Level { Card, Volume, Files, File, Block };

    struct Location
    {
        Level     level  = Level::Block;
        int       volume = -1;
        int       entry  = -1;          // Files: the directory listed; File: the file
        uint32_t  block  = 0;           // Block: the block

        bool operator== (const Location & other) const = default;
    };

    using BlockFn   = std::function<void (uint32_t block)>;
    using ChangedFn = std::function<void ()>;

    BlockLevelHost  ();
    ~BlockLevelHost () override;

    void  Init (BlockInspectorModel * model, const IDxuiTheme * theme, HWND hwnd, IDxuiTextRenderer * text);

    void  SetOnBlock   (BlockFn fn)   { m_onBlock   = std::move (fn); }
    void  SetOnChanged (ChangedFn fn) { m_onChanged = std::move (fn); }

    //  After the model opens: the Card level for an image of partitions, the
    //  Volume level for a volume, or the Block level for an image with no
    //  file system of blocks. The history is cleared.
    void  Open ();

    //  After the model's maps are read again: the start for an image just
    //  opened, or the current level drawn again from the new maps.
    void  Sync ();

    //  Goes to a location, and puts the current one in the Back history.
    void  GoTo (const Location & where);

    //  The Block level at a block: the current location goes in the Back
    //  history, or is replaced, as a step to the next block replaces it.
    void  ShowBlock    (uint32_t block);
    void  ReplaceBlock (uint32_t block);

    bool  GoBack       ();
    bool  GoForward    ();
    bool  CanGoBack    () const { return !m_back.empty(); }
    bool  CanGoForward () const { return !m_forward.empty(); }

    const Location  & GetHere  () const { return m_here; }
    Level             GetLevel () const { return m_here.level; }
    bool              HasCard  () const;

    //  The breadcrumb's segments for a location, and where each goes.
    static vector<std::wstring>  BuildCrumbs (const BlockInspectorModel & model, const Location & here, const std::wstring & cardLabel, bool hasCard,
                                              vector<Location> & outTargets);

    int  GetCrumbHeightPx (const DxuiDpiScaler & scaler) const { return scaler.ToPx (kCrumbDip); }

    BlockFilesLevel    & GetFiles      () { return *m_files; }
    BlockSectionsView  & GetVolumeView () { return *m_volume; }
    BlockSectionsView  & GetFileView   () { return *m_file; }
    DxuiAddressBar     & GetCrumbs     () { return *m_crumbs; }
    DxuiListView       & GetCard       () { return *m_card; }
    DxuiButton         & GetBrowse     () { return *m_browse; }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    void          Show           ();
    void          HighlightEntry (int entry);
    void          NotifyChanged  ();
    std::wstring  GetCardLabel   () const;

    BlockInspectorModel  * m_model       = nullptr;
    DxuiAddressBar       * m_crumbs      = nullptr;
    DxuiListView         * m_card        = nullptr;
    DxuiButton           * m_browse      = nullptr;
    BlockSectionsView    * m_volume      = nullptr;
    BlockFilesLevel      * m_files       = nullptr;
    BlockSectionsView    * m_file        = nullptr;
    BlockFn                m_onBlock;
    ChangedFn              m_onChanged;
    Location               m_here;
    vector<Location>       m_back;
    vector<Location>       m_forward;
    vector<Location>       m_targets;
    vector<int>            m_cardVolumes;
    int                    m_filesVolume = -1;
    uint32_t               m_generation  = 0;
    DxuiDpiScaler          m_scaler;
};
