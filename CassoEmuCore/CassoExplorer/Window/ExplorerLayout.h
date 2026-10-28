#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerCommands.h"

class CassoLinkConnections;
class FirewallMonitor;
class RunningCassoRegistry;
class ILinkAnnouncements;
struct CassoExplorerSessionWindow;
class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout
//
//  The window's widgets and their arrangement: each widget set up and
//  wired to its handlers, the panes and splitters laid out, the command
//  bar refilled, the status bar's fields, and the theme chosen and applied.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerLayout
{
public:
    explicit ExplorerLayout (CassoExplorerWindow & window);

    //  Explorer's tabs are 33 dip tall. Its strip is 9 dip taller because it is
    //  also the window's caption; this window has a caption bar of its own, so
    //  the strip keeps only a small gap above the tabs.
    static constexpr int       kTabHeightDip       = 37;

    //  Explorer's two strips, measured at 100, 125, 150 and 200%: the command
    //  bar is 47 dip rounded down, its line included; the address bar's strip
    //  is 48 dip rounded down with its line added. The address box is 32 dip
    //  tall, and the command bar starts 4 dip in.
    static constexpr float  kCommandBarDip        = 47.0f;
    static constexpr float  kNavStripFillDip      = 48.0f;
    static constexpr UINT   kListRowHalfDip       = 14;
    static constexpr int    kMinTreeWidthDip      = 140;
    static constexpr int    kMinListWidthDip      = 220;
    static constexpr int    kMinPreviewWidthDip   = 280;   // wide enough for the hex view's toolbar

    //  The widest a pane's message runs before it wraps.
    static constexpr int     kMessageWidthDip = 400;

    //  How far each level of the tree sits in from its parent: File
    //  Explorer's, measured at 125% as 10 pixels.
    static constexpr int     kTreeIndentDip   = 8;

    //  A tree row's icon starts 18 dip past the middle of its twisty and its
    //  label 3.33 dip past the icon: measured at 150%.
    static constexpr float   kTreeIconLeadDip = 10.0f;
    static constexpr float   kTreeIconGapDip  = 3.33f;
    static constexpr float   kTreeLeftPadDip  = 8.0f;    // the top level from the pane's left edge

    //  The status bar's fields, left to right: Explorer's item count and
    //  selection, flowing from the left; the space between; then free space,
    //  the preview's detail and its zoom.
    static constexpr size_t  kStatusCount    = 0;
    static constexpr size_t  kStatusSelected = 1;
    static constexpr size_t  kStatusFill     = 2;
    static constexpr size_t  kStatusFree     = 3;
    static constexpr size_t  kStatusDetail   = 4;
    static constexpr size_t  kStatusZoom     = 5;

    //  Status bar field widths: free space, and the preview's detail and zoom
    //  when the preview is hidden and they cannot follow its edge.
    static constexpr int   kStatusBandDip   = 26;   // Explorer's status bar, measured at 150%
    static constexpr int   kStatusFreeDip   = 140;
    static constexpr int   kStatusDetailDip = 280;
    static constexpr int   kStatusZoomDip   = 64;
    static constexpr UINT  kDrivePollMs     = 2000;

    //  The strip a pointer event goes to: the command bar over its band, the
    //  navigation toolbar anywhere else.
    //  The panes' widths in a body this wide, from the widths the user left
    //  them at. The list gives up width first, down to its minimum; then the
    //  preview, down to its own; then the tree. Past all three minimums the
    //  three shrink together. A preview width of zero is a hidden preview.
    struct PaneWidths
    {
        int  tree    = 0;
        int  list    = 0;
        int  preview = 0;
    };

    static PaneWidths  FitPanes (int bodyDip, int treeDip, int previewDip);

    //  File Explorer's list row, measured at nine scales from 100% to 350%:
    //  twice 14 dip rounded up, and a pixel more at any scale that is not a
    //  whole multiple -- 28, 37, 43, 51, 56, 65, 71, 84 and 99 pixels.
    static int  GetListRowHeightPx (UINT dpi);

    void  ConfigureWidgets();
    void  ApplyTheme();
    void  AdoptSystemColors();
    void  PreviewTheme (int index);
    void  SelectTheme  (const char * name);

    static bool  IsCassoThemeName (const std::string & name);
    void  RecomputeLayout();
    void  UpdateCommandBar();
    void  FillStatus();
    void  LayoutStatusFields ();

    //  The part of a pane a message wraps within.
    RECT  GetMessageRect (const RECT & pane) const;

private:
    friend class CassoExplorerWindow;
    friend class ExplorerDispatch;
    friend class ExplorerInput;
    friend class ExplorerLinks;
    friend class ExplorerTips;

    CassoExplorerWindow  & m_window;

    DxuiHitTester                             m_dropHits;

    //  The location's buttons on the command bar, if any.
    CassoExplorerCommands::CommandBarExtra  m_commandBarExtra = CassoExplorerCommands::CommandBarExtra::None;
};
